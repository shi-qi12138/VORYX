#include "opengl_api.hpp"
#include "opengl_buffer.hpp"
#include "opengl_color_target.hpp"
#include "opengl_context.hpp"
#include "opengl_draw.hpp"
#include "opengl_failure.hpp"
#include "opengl_pipeline.hpp"

#include <buffer_backend.hpp>
#include <opengl_factory.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace voryx::opengl_backend
{
namespace
{
bool IsSoftware(std::string name)
{
    std::ranges::transform(name, name.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    constexpr std::array<std::string_view, 6> renderers{
        "llvmpipe",    "softpipe",   "swrast", "software rasterizer",
        "swiftshader", "gdi generic"};
    return std::ranges::any_of(renderers, [&](std::string_view renderer) {
        return name.find(renderer) != std::string::npos;
    });
}

class Backend final : public rhi_detail::BufferBackend
{
public:
    DeviceInfo Initialize(const DeviceDesc &desc, DeviceCapabilities &caps)
    {
        if (desc.enableValidation) {
            throw Failure{ErrorCode::Unsupported};
        }
        context.Initialize();
        const Context::Current current(context);
        api.emplace(context);
        int major = 0;
        int minor = 0;
        api->getIntegerv(majorVersion, &major);
        api->getIntegerv(minorVersion, &minor);
        const auto *renderer = api->getString(rendererName);
        api->Check();
        if (major < 3 || (major == 3 && minor < 3) || !renderer) {
            throw Failure{ErrorCode::BackendUnavailable};
        }
        std::string name(reinterpret_cast<const char *>(renderer));
        const bool software = context.SoftwarePixelFormat() || IsSoftware(name);
        if (software && !desc.allowSoftwareAdapter) {
            throw Failure{ErrorCode::BackendUnavailable};
        }
        int textureLimit = 0;
        api->getIntegerv(maxTextureSize, &textureLimit);
        // 固定点颜色附件读回使用确定的通道量化。
        api->disable(dither);
        api->Check();
        if (textureLimit <= 0) {
            throw Failure{ErrorCode::BackendUnavailable};
        }
        caps.maxGraphicsPipelines = 256;
        caps.maxVertexAttributes = 16;
        caps.maxVertexStrideBytes = 2048;
        caps.maxTextures = 256;
        caps.maxTexture2DSize =
            static_cast<std::uint32_t>(std::min(textureLimit, 4096));
        return {BackendType::OpenGL, std::move(name),
                std::string(desc.debugName), software, false};
    }

    Result<std::unique_ptr<rhi_detail::BufferStorage>>
    CreateStorage(const BufferDesc &desc) override
    {
        return Invoke<std::unique_ptr<rhi_detail::BufferStorage>>(
            "CreateOpenGLBuffer", [&]() {
                auto buffer = std::make_unique<Buffer>(context, *api);
                const Context::Current current(context);
                buffer->Allocate(desc.sizeBytes);
                return Result<std::unique_ptr<rhi_detail::BufferStorage>>::
                    Success(std::move(buffer));
            });
    }

    Result<std::unique_ptr<rhi_detail::PipelineStorage>>
    CreatePipeline(const GraphicsPipelineDesc &desc,
                   rhi_detail::BindingLayoutStorage *layout) override
    {
        using StoragePtr = std::unique_ptr<rhi_detail::PipelineStorage>;
        if (desc.vertexGlsl.empty() || layout || desc.depthTest
            || desc.colorFormat != Format::RGBA8_UNorm
            || desc.blendMode != BlendMode::Opaque
            || desc.cullMode != CullMode::None) {
            return Fail<StoragePtr>(ErrorCode::Unsupported);
        }
        return Invoke<StoragePtr>("CreateOpenGLPipeline", [&]() {
            const Context::Current current(context);
            auto pipeline = std::make_unique<Pipeline>(context, *api, desc);
            pipeline->Link(desc.vertexGlsl, desc.fragmentGlsl);
            return Result<StoragePtr>::Success(std::move(pipeline));
        });
    }

    Result<std::unique_ptr<rhi_detail::TextureStorage>>
    CreateSampledTexture(const Rgba8TextureDesc &) override
    {
        return Fail<std::unique_ptr<rhi_detail::TextureStorage>>(
            ErrorCode::Unsupported);
    }

    Result<std::unique_ptr<rhi_detail::SamplerStorage>>
    CreateSampler(const SamplerDesc &) override
    {
        return Fail<std::unique_ptr<rhi_detail::SamplerStorage>>(
            ErrorCode::Unsupported);
    }

    Result<std::unique_ptr<rhi_detail::BindingLayoutStorage>>
    CreateBindingLayout(const BindingLayoutDesc &) override
    {
        return Fail<std::unique_ptr<rhi_detail::BindingLayoutStorage>>(
            ErrorCode::Unsupported);
    }

    Result<std::unique_ptr<rhi_detail::BindingSetStorage>>
    CreateBindingSet(const rhi_detail::BindingLayoutStorage &,
                     std::span<const rhi_detail::BackendBinding>) override
    {
        return Fail<std::unique_ptr<rhi_detail::BindingSetStorage>>(
            ErrorCode::Unsupported);
    }

    Result<std::unique_ptr<rhi_detail::TextureStorage>>
    CreateTexture(const TextureDesc &desc) override
    {
        using StoragePtr = std::unique_ptr<rhi_detail::TextureStorage>;
        if (desc.format != Format::RGBA8_UNorm) {
            return Fail<StoragePtr>(ErrorCode::Unsupported);
        }
        return Invoke<StoragePtr>("CreateOpenGLColorTarget", [&]() {
            auto texture =
                std::make_unique<ColorTarget>(context, *api, desc.extent);
            const Context::Current current(context);
            texture->Allocate();
            return Result<StoragePtr>::Success(std::move(texture));
        });
    }

    Result<void> ReadTexture(rhi_detail::TextureStorage &storage,
                             MutableByteSpan output) noexcept override
    {
        return Invoke<void>("ReadOpenGLTexture", [&]() {
            const Context::Current current(context);
            static_cast<const ColorTarget &>(storage).Read(output);
            return Result<void>::Success();
        });
    }

    Result<void> Execute(std::span<const rhi_detail::BackendOperation>
                             operations) noexcept override
    {
        // OpenGL 逐条执行，须在原生资源改变前拒绝整个不支持的批次。
        if (!std::ranges::all_of(operations, Supports)) {
            return Fail<void>(ErrorCode::Unsupported);
        }
        return Invoke<void>("SubmitOpenGL", [&]() {
            const Context::Current current(context);
            try {
                DrawState draw(*api);
                for (const auto &operation : operations) {
                    ExecuteOperation(operation, draw);
                }
                api->finish();
                api->Check();
            } catch (const Failure &error) {
                // 已执行的 GL 写入不能回滚，停止后续提交并保留原生错误码。
                throw Failure{ErrorCode::DeviceLost, error.nativeCode};
            }
            return Result<void>::Success();
        });
    }

private:
    using Kind = rhi_detail::OperationKind;

    static bool Supports(const rhi_detail::BackendOperation &op) noexcept
    {
        switch (op.kind) {
            case Kind::Barrier:
            case Kind::TextureBarrier:
            case Kind::Copy:
            case Kind::EndPass:
            case Kind::Pipeline:
            case Kind::Vertex:
            case Kind::Scissor:
                return true;
            case Kind::BeginPass:
                return op.depth == nullptr;
            case Kind::Viewport: {
                const auto &view = op.viewport;
                return view.x == std::trunc(view.x)
                       && view.y == std::trunc(view.y)
                       && view.width == std::trunc(view.width)
                       && view.height == std::trunc(view.height);
            }
            case Kind::Draw:
                return static_cast<std::uint64_t>(op.first) + op.count
                       <= static_cast<unsigned int>(
                           std::numeric_limits<int>::max());
            default:
                return false;
        }
    }

    void ExecuteOperation(const rhi_detail::BackendOperation &op,
                          DrawState &draw)
    {
        switch (op.kind) {
            case Kind::Barrier:
            case Kind::TextureBarrier:
                // 同一上下文按调用顺序执行，资源用途由共享 RHI 跟踪。
                break;
            case Kind::Copy:
                static_cast<const Buffer &>(*op.src).Bind(copyReadBuffer);
                static_cast<const Buffer &>(*op.dst).Bind(copyWriteBuffer);
                api->copyBufferSubData(copyReadBuffer, copyWriteBuffer,
                                       static_cast<GlSize>(op.srcOffset),
                                       static_cast<GlSize>(op.dstOffset),
                                       static_cast<GlSize>(op.size));
                api->Check();
                break;
            case Kind::BeginPass: {
                const auto &target =
                    static_cast<const ColorTarget &>(*op.texture);
                draw.BeginPass(target.Extent().height);
                target.Clear(op.clear);
                break;
            }
            case Kind::EndPass:
                api->bindFramebuffer(drawFramebuffer, 0);
                api->Check();
                break;
            case Kind::Pipeline:
                draw.SetPipeline(static_cast<const Pipeline &>(*op.pipeline));
                break;
            case Kind::Vertex:
                draw.SetVertexBuffer(
                    static_cast<const Buffer &>(*op.src).Name(), op.srcOffset);
                break;
            case Kind::Viewport:
                draw.SetViewport(op.viewport);
                break;
            case Kind::Scissor:
                draw.SetScissor(op.scissor);
                break;
            case Kind::Draw:
                draw.Draw(op.count, op.first);
                break;
            default:
                throw Failure{ErrorCode::InternalError};
        }
    }

    Context context;
    std::optional<Api> api;
};
} // namespace
} // namespace voryx::opengl_backend

namespace voryx::detail
{
Result<std::unique_ptr<IDevice>>
CreateOpenGLDevice(std::uint64_t identity, const DeviceDesc &desc) noexcept
{
    using namespace opengl_backend;
    return Invoke<std::unique_ptr<IDevice>>("CreateDevice", [&]() {
        auto backend = std::make_unique<Backend>();
        DeviceCapabilities caps{};
        auto info = backend->Initialize(desc, caps);
        caps.maxBufferSizeBytes = 64 * 1024 * 1024;
        caps.maxBuffers = 1024;
        caps.maxCommandsPerList = 4096;
        caps.maxCommandListsPerSubmit = 64;
        return rhi_detail::CreateBufferDevice(identity, std::move(info), caps,
                                              std::move(backend));
    });
}
} // namespace voryx::detail

#include <buffer_backend.hpp>
#include <buffer_guard.hpp>
#include <null_factory.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace voryx::null_backend
{
class Storage final : public rhi_detail::BufferStorage
{
public:
    explicit Storage(std::uint64_t size)
        : data(static_cast<std::size_t>(size))
    {
    }
    Result<void> Write(std::uint64_t offset, ByteSpan input) noexcept override
    {
        if (!input.empty()) {
            std::memcpy(data.data() + offset, input.data(), input.size());
        }
        return Result<void>::Success();
    }
    Result<void> Read(std::uint64_t offset,
                      MutableByteSpan output) noexcept override
    {
        if (!output.empty()) {
            std::memcpy(output.data(), data.data() + offset, output.size());
        }
        return Result<void>::Success();
    }
    std::vector<std::byte> data;
};

class Texture final : public rhi_detail::TextureStorage
{
public:
    explicit Texture(const TextureDesc &desc)
        : format(desc.format)
        , data(std::size_t(desc.extent.width) * desc.extent.height * 4)
    {
    }
    Format format;
    std::vector<std::byte> data;
};

class Pipeline final : public rhi_detail::PipelineStorage
{
public:
    explicit Pipeline(const GraphicsPipelineDesc &desc)
        : vertex(desc.vertexSpirv.begin(), desc.vertexSpirv.end())
        , fragment(desc.fragmentSpirv.begin(), desc.fragmentSpirv.end())
        , attributes(desc.vertexAttributes.begin(), desc.vertexAttributes.end())
        , name(desc.debugName)
        , description(desc)
    {
        description.vertexSpirv = vertex;
        description.fragmentSpirv = fragment;
        description.vertexAttributes = attributes;
        description.debugName = name;
    }

private:
    const std::vector<std::uint32_t> vertex;
    const std::vector<std::uint32_t> fragment;
    const std::vector<VertexAttribute> attributes;
    const std::string name;
    GraphicsPipelineDesc description;
};

class Sampler final : public rhi_detail::SamplerStorage
{
public:
    explicit Sampler(const SamplerDesc &desc) noexcept
        : desc(desc)
    {
    }

private:
    const SamplerDesc desc;
};

class BindingLayout final : public rhi_detail::BindingLayoutStorage
{
public:
    explicit BindingLayout(const BindingLayoutDesc &desc)
        : entries(desc.entries.begin(), desc.entries.end())
    {
    }

private:
    const std::vector<BindingLayoutEntry> entries;
};

class BindingSet final : public rhi_detail::BindingSetStorage
{
public:
    explicit BindingSet(std::span<const rhi_detail::BackendBinding> entries)
        : entries(entries.begin(), entries.end())
    {
    }

private:
    const std::vector<rhi_detail::BackendBinding> entries;
};

class Backend final : public rhi_detail::BufferBackend
{
public:
    Result<std::unique_ptr<rhi_detail::BufferStorage>>
    CreateStorage(const BufferDesc &desc) override
    {
        std::unique_ptr<rhi_detail::BufferStorage> storage =
            std::make_unique<Storage>(desc.sizeBytes);
        return Result<std::unique_ptr<rhi_detail::BufferStorage>>::Success(
            std::move(storage));
    }
    Result<std::unique_ptr<rhi_detail::PipelineStorage>>
    CreatePipeline(const GraphicsPipelineDesc &desc,
                   rhi_detail::BindingLayoutStorage *) override
    {
        std::unique_ptr<rhi_detail::PipelineStorage> pipeline =
            std::make_unique<Pipeline>(desc);
        return Result<std::unique_ptr<rhi_detail::PipelineStorage>>::Success(
            std::move(pipeline));
    }
    Result<std::unique_ptr<rhi_detail::TextureStorage>>
    CreateTexture(const TextureDesc &desc) override
    {
        std::unique_ptr<rhi_detail::TextureStorage> texture =
            std::make_unique<Texture>(desc);
        return Result<std::unique_ptr<rhi_detail::TextureStorage>>::Success(
            std::move(texture));
    }
    Result<std::unique_ptr<rhi_detail::TextureStorage>>
    CreateSampledTexture(const Rgba8TextureDesc &desc) override
    {
        auto texture = std::make_unique<Texture>(TextureDesc{
            desc.extent, Format::RGBA8_UNorm, TextureUsage::Sampled, {}});
        const std::size_t rowBytes = std::size_t(desc.extent.width) * 4;
        const auto pitch =
            desc.rowPitchBytes == 0 ? rowBytes : desc.rowPitchBytes;
        for (std::size_t row = 0; row < desc.extent.height; ++row) {
            std::memcpy(texture->data.data() + row * rowBytes,
                        desc.pixels.data() + row * pitch, rowBytes);
        }
        return Result<std::unique_ptr<rhi_detail::TextureStorage>>::Success(
            std::move(texture));
    }
    Result<std::unique_ptr<rhi_detail::SamplerStorage>>
    CreateSampler(const SamplerDesc &desc) override
    {
        return Result<std::unique_ptr<rhi_detail::SamplerStorage>>::Success(
            std::make_unique<Sampler>(desc));
    }
    Result<std::unique_ptr<rhi_detail::BindingLayoutStorage>>
    CreateBindingLayout(const BindingLayoutDesc &desc) override
    {
        return Result<std::unique_ptr<rhi_detail::BindingLayoutStorage>>::
            Success(std::make_unique<BindingLayout>(desc));
    }
    Result<std::unique_ptr<rhi_detail::BindingSetStorage>> CreateBindingSet(
        const rhi_detail::BindingLayoutStorage &,
        std::span<const rhi_detail::BackendBinding> resources) override
    {
        return Result<std::unique_ptr<rhi_detail::BindingSetStorage>>::Success(
            std::make_unique<BindingSet>(resources));
    }
    Result<void> ReadTexture(rhi_detail::TextureStorage &texture,
                             MutableByteSpan output) noexcept override
    {
        const auto &source = static_cast<Texture &>(texture);
        std::memcpy(output.data(), source.data.data(), output.size());
        return Result<void>::Success();
    }
    Result<void> Execute(std::span<const rhi_detail::BackendOperation>
                             operations) noexcept override
    {
        for (const auto &operation : operations) {
            if (operation.kind == rhi_detail::OperationKind::BeginPass) {
                auto &texture = static_cast<Texture &>(*operation.texture);
                const std::array values{
                    operation.clear.red, operation.clear.green,
                    operation.clear.blue, operation.clear.alpha};
                std::array<std::byte, 4> color{};
                for (std::size_t index = 0; index < 4; ++index) {
                    color[index] = static_cast<std::byte>(std::lround(
                        std::clamp(values[index], 0.0f, 1.0f) * 255));
                }
                for (std::size_t offset = 0; offset < texture.data.size();
                     offset += 4) {
                    std::memcpy(texture.data.data() + offset, color.data(), 4);
                }
                if (operation.depth) {
                    auto &depth = static_cast<Texture &>(*operation.depth);
                    for (std::size_t offset = 0; offset < depth.data.size();
                         offset += 4) {
                        std::memcpy(depth.data.data() + offset,
                                    &operation.clearDepth, 4);
                    }
                }
            } else if (operation.kind
                       == rhi_detail::BackendOperation::Kind::Copy) {
                const auto &src = static_cast<Storage &>(*operation.src);
                auto &dst = static_cast<Storage &>(*operation.dst);
                std::memcpy(dst.data.data() + operation.dstOffset,
                            src.data.data() + operation.srcOffset,
                            static_cast<std::size_t>(operation.size));
            }
        }
        return Result<void>::Success();
    }
};
} // namespace voryx::null_backend

namespace voryx::detail
{
Result<std::unique_ptr<IDevice>>
CreateNullDevice(std::uint64_t identity, std::string_view debugName) noexcept
{
    return rhi_detail::Invoke<std::unique_ptr<IDevice>>(
        "CreateNullDevice", [&]() {
            return rhi_detail::CreateBufferDevice(
                identity,
                {BackendType::Null, "Null (CPU validation)",
                 std::string(debugName)},
                {64 * 1024 * 1024, 1024, 4096, 64, 256, 16, 2048, 64, 4096,
                 16 * 1024, 256, 256, 256, 256},
                std::make_unique<null_backend::Backend>());
        });
}
} // namespace voryx::detail

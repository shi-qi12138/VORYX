#include "triangle_spirv.hpp"
#include <array>
#include <atomic>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <voryx/runtime/device_factory.hpp>

namespace
{
template<typename T>
T Take(voryx::Result<T> result)
{
    if (!result) {
        throw std::runtime_error(
            std::string(voryx::GetErrorCodeName(result.GetError().errorCode))
            + " in " + result.GetError().operation);
    }
    return std::move(result).Value();
}
void Take(voryx::Result<void> result)
{
    if (!result) {
        throw std::runtime_error(
            std::string(voryx::GetErrorCodeName(result.GetError().errorCode))
            + " in " + result.GetError().operation);
    }
}
void Run(const voryx::DeviceDesc &deviceDesc)
{
    using namespace voryx;
    auto device = Take(CreateDevice(deviceDesc));
    constexpr std::array slots{
        BindingLayoutEntry{3, BindingType::UniformBuffer,
                           BindingVisibility::Vertex},
        BindingLayoutEntry{7, BindingType::SampledTexture,
                           BindingVisibility::Fragment},
        BindingLayoutEntry{11, BindingType::Sampler,
                           BindingVisibility::Fragment},
    };
    const auto layout = Take(device->CreateBindingLayout({slots}));
    constexpr std::array inputs{VertexAttribute{0, VertexFormat::Float3, 0},
                                VertexAttribute{1, VertexFormat::Float2, 12}};
    GraphicsPipelineDesc desc{};
    desc.vertexSpirv = sample_data::resourceDepthVertexSpirv;
    desc.fragmentSpirv = sample_data::resourceFragmentSpirv;
    desc.vertexStrideBytes = 20;
    desc.vertexAttributes = inputs;
    desc.bindingLayout = layout;
    desc.depthTest = true;
    desc.depthWrite = true;
    const auto pipeline = Take(device->CreateGraphicsPipeline(desc));
    const auto color = Take(device->CreateTexture(
        {{64, 64},
         Format::RGBA8_UNorm,
         TextureUsage::ColorAttachment | TextureUsage::CopySrc,
         "offscreen color"}));
    const auto depth =
        Take(device->CreateTexture({{64, 64},
                                    Format::D32_Float,
                                    TextureUsage::DepthAttachment,
                                    "offscreen depth"}));
    // 第 0 行为黄色，第 1 行含红、绿，两行之间有 3 个填充字节。
    std::array<std::uint8_t, 19> pixels{
        255, 255, 0, 255, 255, 255, 0,   255, 0,   0,
        0,   255, 0, 0,   255, 0,   255, 0,   255,
    };
    const auto texture = Take(device->CreateTexture2D(
        {{2, 2}, 11, std::as_bytes(std::span(pixels))}));
    pixels.fill(0);
    const auto sampler = Take(device->CreateSampler(
        {TextureFilter::Nearest, TextureFilter::Nearest}));
    constexpr std::array nearTransform{0.5f, 0.5f, 0.5f, 0.0f};
    constexpr std::array farTransform{0.5f, 0.5f, 0.0f, 0.0f};
    const auto alignment =
        device->GetCapabilities().uniformBufferOffsetAlignmentBytes;
    const auto farOffset =
        ((sizeof(nearTransform) + alignment - 1) / alignment) * alignment;
    const auto uniform = Take(device->CreateBuffer(
        {farOffset + sizeof(farTransform),
         BufferUsage::Constant | BufferUsage::CopyDst, "transforms"}));
    Take(device->WriteBuffer(uniform, 0,
                             std::as_bytes(std::span(nearTransform))));
    Take(device->WriteBuffer(uniform, farOffset,
                             std::as_bytes(std::span(farTransform))));
    const auto makeSet = [&](std::uint64_t offset) {
        const std::array entries{
            DeviceBindingSetEntry{11, sampler},
            DeviceBindingSetEntry{
                3,
                UniformBufferBinding{uniform, offset, sizeof(nearTransform)}},
            DeviceBindingSetEntry{7, texture},
        };
        return Take(device->CreateBindingSet({layout, entries}));
    };
    const auto nearSet = makeSet(0);
    const auto farSet = makeSet(farOffset);
    constexpr std::array<std::array<float, 5>, 8> vertices{{
        {-1, -1, 0.2f, 0.35f, 0.75f},
        {1, -1, 0.2f, 0.35f, 0.75f},
        {-1, 1, 0.2f, 0.35f, 0.75f},
        {1, 1, 0.2f, 0.35f, 0.75f},
        {-1, -1, 0.8f, 0.65f, 0.75f},
        {1, -1, 0.8f, 0.65f, 0.75f},
        {-1, 1, 0.8f, 0.65f, 0.75f},
        {1, 1, 0.8f, 0.65f, 0.75f},
    }};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 1, 3, 2};
    const auto vb = Take(device->CreateBuffer(
        {sizeof(vertices), BufferUsage::Vertex | BufferUsage::CopyDst,
         "vertices"},
        std::as_bytes(std::span(vertices))));
    const auto ib = Take(device->CreateBuffer(
        {sizeof(indices), BufferUsage::Index | BufferUsage::CopyDst, "indices"},
        std::as_bytes(std::span(indices))));
    auto command = Take(device->CreateCommandList());
    Take(command->Begin());
    const std::array buffers{
        BufferBarrier{vb, BufferUse::Initial, BufferUse::VertexRead},
        BufferBarrier{ib, BufferUse::Initial, BufferUse::IndexRead},
        BufferBarrier{uniform, BufferUse::Initial, BufferUse::ConstantRead}};
    Take(command->Barrier(buffers));
    const std::array attachment{
        TextureBarrier{color, TextureUse::Initial, TextureUse::ColorAttachment},
        TextureBarrier{depth, TextureUse::Initial, TextureUse::DepthWrite}};
    Take(command->Barrier(attachment));
    Take(command->BeginRenderPass({color, {0, 0, 1, 1}, depth, 1}));
    Take(command->SetPipeline(pipeline));
    Take(command->SetVertexBuffer(vb));
    Take(command->SetIndexBuffer(ib, IndexType::UInt32));
    Take(command->SetViewport({0, 0, 64, 64, 0, 1}));
    Take(command->SetScissor({0, 0, 64, 64}));
    Take(command->SetBindingSet(nearSet));
    Take(command->DrawIndexed(6));
    Take(command->SetBindingSet(farSet));
    Take(command->DrawIndexed(6, 0, 4));
    Take(command->EndRenderPass());
    const std::array readback{TextureBarrier{color, TextureUse::ColorAttachment,
                                             TextureUse::CopySrc}};
    Take(command->Barrier(readback));
    Take(command->End());
    const std::array<ICommandList *, 1> lists{command.get()};
    const auto mode = device->GetCapabilities().maxPendingSubmissions > 0
                          ? SubmitMode::Async
                          : SubmitMode::Synchronous;
    const auto submitted = Take(device->Submit({lists, {}, mode}));
    // 句柄和命令包装可以先释放，原生绘制依赖须保留到 fence 完成。
    command.reset();
    Take(device->Destroy(nearSet));
    Take(device->Destroy(farSet));
    Take(device->Destroy(pipeline));
    Take(device->Destroy(depth));
    Take(device->Destroy(vb));
    Take(device->Destroy(ib));
    Take(device->Wait(submitted, std::numeric_limits<TimeoutNs>::max()));
    std::array<std::byte, 64 * 64 * 4> output{};
    Take(device->ReadTexture(color, output));
    const auto channel = [&](std::size_t x, std::size_t y,
                             std::size_t component) {
        return std::to_integer<unsigned>(output[(y * 64 + x) * 4 + component]);
    };
    // Null 执行清除并验证命令契约，实际光栅化像素仅由 Vulkan 检查。
    const auto pixel = [&](std::size_t x, std::size_t y,
                           std::array<unsigned, 4> expected) {
        for (std::size_t component = 0; component < expected.size();
             ++component) {
            if (channel(x, y, component) != expected[component]) {
                throw std::runtime_error("Offscreen pixel mismatch at "
                                         + std::to_string(x) + ", "
                                         + std::to_string(y));
            }
        }
    };
    pixel(8, 32, {0, 0, 255, 255});
    pixel(40, 8, {0, 0, 255, 255});
    if (device->GetInfo().backend != BackendType::Null) {
        pixel(22, 32, {0, 255, 0, 255});
        pixel(40, 32, {255, 0, 0, 255}); // 后画的远处几何不能覆盖近处。
        pixel(56, 32, {255, 0, 0, 255});
    }
    Take(device->Destroy(layout));
    Take(device->Destroy(texture));
    Take(device->Destroy(sampler));
    Take(device->Destroy(uniform));
    Take(device->Destroy(color));
    std::puts("Device bindings and readback verified.");
}
} // namespace
int main(int argc, char **argv)
{
    using namespace voryx;
    DeviceDesc desc{BackendType::Null, "installed device draw"};
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--vulkan") {
            desc.backend = BackendType::Vulkan;
        } else if (argument == "--validation") {
            desc.enableValidation = true;
        } else if (argument == "--allow-software") {
            desc.allowSoftwareAdapter = true;
        } else {
            return 2;
        }
    }
    std::atomic<int> errors{0};
    desc.logUserData = &errors;
    desc.logCallback = [](LogLevel level, std::string_view message,
                          void *data) noexcept {
        if (level == LogLevel::Error) {
            static_cast<std::atomic<int> *>(data)->fetch_add(1);
            std::fwrite(message.data(), 1, message.size(), stderr);
            std::fputc('\n', stderr);
        }
    };
    try {
        Run(desc);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return errors.load() == 0 ? 0 : 1;
}

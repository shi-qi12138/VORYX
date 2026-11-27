#include "triangle_spirv.hpp"

#include <array>
#include <limits>
#include <support/rhi_support.hpp>

namespace voryx::test
{
namespace
{
constexpr std::array slots{
    BindingLayoutEntry{3, BindingType::UniformBuffer,
                       BindingVisibility::Vertex},
    BindingLayoutEntry{7, BindingType::SampledTexture,
                       BindingVisibility::Fragment},
    BindingLayoutEntry{11, BindingType::Sampler, BindingVisibility::Fragment},
};
GraphicsPipelineDesc PipelineDesc(DeviceBindingLayoutHandle layout)
{
    constexpr static std::array inputs{
        VertexAttribute{0, VertexFormat::Float2, 0},
        VertexAttribute{1, VertexFormat::Float2, 8}};
    GraphicsPipelineDesc desc{};
    desc.vertexSpirv = sample_data::resourceVertexSpirv;
    desc.fragmentSpirv = sample_data::resourceFragmentSpirv;
    desc.vertexStrideBytes = 16;
    desc.vertexAttributes = inputs;
    desc.bindingLayout = layout;
    return desc;
}
TextureHandle Sample(IDevice &device)
{
    constexpr std::array<std::byte, 4> white{std::byte{255}, std::byte{255},
                                             std::byte{255}, std::byte{255}};
    return Take(device.CreateTexture2D({{1, 1}, 0, white}));
}
} // namespace

TEST_F(BufferRhiTest, DeviceBindingsValidateLayoutShaderAndResourceRanges)
{
    auto entries = slots;
    entries[1].slot = entries[0].slot;
    ExpectError(device->CreateBindingLayout({entries}),
                ErrorCode::InvalidArgument);
    entries = slots;
    entries[0].visibility = BindingVisibility::Fragment;
    const auto wrongStage = Take(device->CreateBindingLayout({entries}));
    ExpectError(device->CreateGraphicsPipeline(PipelineDesc(wrongStage)),
                ErrorCode::InvalidArgument);
    entries = slots;
    entries[1].type = BindingType::Sampler;
    const auto wrongType = Take(device->CreateBindingLayout({entries}));
    ExpectError(device->CreateGraphicsPipeline(PipelineDesc(wrongType)),
                ErrorCode::InvalidArgument);
    entries[1].slot = 32;
    ExpectError(device->CreateBindingLayout({entries}),
                ErrorCode::InvalidArgument);
    const auto layout = Take(device->CreateBindingLayout({slots}));
    auto foreign = NewDevice("foreign device");
    ExpectError(foreign->CreateGraphicsPipeline(PipelineDesc(layout)),
                ErrorCode::WrongDevice);
    const std::array<std::byte, 4> pixel{};
    ExpectError(device->CreateTexture2D({{2, 1}, 0, pixel}),
                ErrorCode::InvalidArgument);
    ExpectError(device->CreateTexture2D(
                    {{1, 2}, std::numeric_limits<std::size_t>::max(), pixel}),
                ErrorCode::InvalidArgument);
    ExpectError(device->CreateSampler({static_cast<TextureFilter>(255)}),
                ErrorCode::InvalidArgument);

    const auto texture = Sample(*device);
    const auto sampler = Take(device->CreateSampler({}));
    const auto buffer = Buffer(16, BufferUsage::Constant);
    std::array resources{
        DeviceBindingSetEntry{7, texture},
        DeviceBindingSetEntry{11, sampler},
        DeviceBindingSetEntry{3, UniformBufferBinding{buffer, 0, 16}},
    };
    ExpectError(
        device->CreateBindingSet({layout, std::span(resources).first(2)}),
        ErrorCode::InvalidArgument);
    resources[2].slot = 7;
    ExpectError(device->CreateBindingSet({layout, resources}),
                ErrorCode::InvalidArgument);
    resources[2] = {3, texture};
    ExpectError(device->CreateBindingSet({layout, resources}),
                ErrorCode::InvalidArgument);
    if (device->GetCapabilities().uniformBufferOffsetAlignmentBytes > 1) {
        resources[2].resource = UniformBufferBinding{buffer, 1, 8};
        ExpectError(device->CreateBindingSet({layout, resources}),
                    ErrorCode::InvalidArgument);
    }
    resources[2].resource = UniformBufferBinding{buffer, 0, 17};
    ExpectError(device->CreateBindingSet({layout, resources}),
                ErrorCode::InvalidArgument);
    resources[2].resource = UniformBufferBinding{buffer, 0, 0};
    ExpectError(device->CreateBindingSet({layout, resources}),
                ErrorCode::InvalidArgument);
    resources[2].resource = UniformBufferBinding{buffer, 0, 16};
    resources[0].resource = Sample(*foreign);
    ExpectError(device->CreateBindingSet({layout, resources}),
                ErrorCode::WrongDevice);
    resources[0].resource = texture;
    const auto set = Take(device->CreateBindingSet({layout, resources}));
    Take(device->Destroy(set));
    ExpectError(device->Destroy(set), ErrorCode::InvalidHandle);
    Take(device->Destroy(buffer));
    Take(device->Destroy(texture));
    Take(device->Destroy(sampler));
    Take(device->Destroy(layout));
    ExpectError(device->CreateGraphicsPipeline(PipelineDesc(layout)),
                ErrorCode::InvalidHandle);
}

TEST_F(BufferRhiTest,
       DeviceBindingsRetainDependenciesAndRetryAfterInvalidSubmission)
{
    const auto layout = Take(device->CreateBindingLayout({slots}));
    const auto otherLayout = Take(device->CreateBindingLayout({slots}));
    const auto pipeline =
        Take(device->CreateGraphicsPipeline(PipelineDesc(layout)));
    const auto texture = Sample(*device);
    const auto sampler = Take(device->CreateSampler({}));
    constexpr std::array transform{1.0f, 1.0f, 0.0f, 0.0f};
    const auto uniform =
        Buffer(sizeof(transform), BufferUsage::Constant | BufferUsage::CopyDst,
               std::as_bytes(std::span(transform)));
    const std::array resources{
        DeviceBindingSetEntry{
            3, UniformBufferBinding{uniform, 0, sizeof(transform)}},
        DeviceBindingSetEntry{7, texture},
        DeviceBindingSetEntry{11, sampler},
    };
    const auto set = Take(device->CreateBindingSet({layout, resources}));
    const auto otherSet =
        Take(device->CreateBindingSet({otherLayout, resources}));
    ExpectError(device->Destroy(layout), ErrorCode::ResourceBusy);
    ExpectError(device->Destroy(texture), ErrorCode::ResourceBusy);
    ExpectError(device->Destroy(uniform), ErrorCode::ResourceBusy);
    ExpectError(device->Destroy(sampler), ErrorCode::ResourceBusy);
    Take(device->WriteBuffer(uniform, 0, std::as_bytes(std::span(transform))));

    const auto color = Take(device->CreateTexture(
        {{8, 8}, Format::RGBA8_UNorm, TextureUsage::ColorAttachment, {}}));
    constexpr std::array<float, 12> vertices{-1, -1, 0, 0, 1, -1,
                                             1,  0,  0, 1, 0, 1};
    const auto vb =
        Buffer(sizeof(vertices), BufferUsage::Vertex | BufferUsage::CopyDst,
               std::as_bytes(std::span(vertices)));
    auto command = Command();
    Take(command->Begin());
    const std::array attachment{TextureBarrier{color, TextureUse::Initial,
                                               TextureUse::ColorAttachment}};
    const std::array geometry{
        BufferBarrier{vb, BufferUse::Initial, BufferUse::VertexRead}};
    Take(command->Barrier(attachment));
    Take(command->Barrier(geometry));
    Take(command->BeginRenderPass({color, {}, {}, 1}));
    ExpectError(command->SetBindingSet(set), ErrorCode::InvalidState);
    Take(command->SetPipeline(pipeline));
    Take(command->SetVertexBuffer(vb));
    Take(command->SetViewport({0, 0, 8, 8, 0, 1}));
    Take(command->SetScissor({0, 0, 8, 8}));
    ExpectError(command->SetBindingSet(otherSet), ErrorCode::InvalidArgument);
    ExpectError(command->Draw(3), ErrorCode::InvalidState);
    Take(command->SetBindingSet(set));
    Take(command->SetPipeline(pipeline));
    ExpectError(command->Draw(3), ErrorCode::InvalidState);
    Take(command->SetBindingSet(set));
    Take(command->Draw(3));
    Take(command->EndRenderPass());
    Take(command->End());
    const std::array<ICommandList *, 1> incomplete{command.get()};
    ExpectError(device->Submit({incomplete}), ErrorCode::InvalidState);
    EXPECT_EQ(Take(command->GetState()), CommandState::Executable);
    ExpectError(device->Destroy(set), ErrorCode::ResourceBusy);
    ExpectError(
        device->WriteBuffer(uniform, 0, std::as_bytes(std::span(transform))),
        ErrorCode::ResourceBusy);
    auto transition = Command();
    Take(transition->Begin());
    const std::array constants{
        BufferBarrier{uniform, BufferUse::Initial, BufferUse::ConstantRead}};
    Take(transition->Barrier(constants));
    Take(transition->End());
    const std::array<ICommandList *, 2> batch{transition.get(), command.get()};
    Take(device->Submit({batch}));
    Take(device->WriteBuffer(uniform, 0, std::as_bytes(std::span(transform))));
    Take(device->Destroy(set));
    Take(device->Destroy(otherSet));
    ExpectError(device->Destroy(layout), ErrorCode::ResourceBusy);
    Take(device->Destroy(pipeline));
    Take(device->Destroy(layout));
    Take(device->Destroy(otherLayout));
    Take(device->Destroy(texture));
    Take(device->Destroy(sampler));
    Take(device->Destroy(uniform));
}
} // namespace voryx::test

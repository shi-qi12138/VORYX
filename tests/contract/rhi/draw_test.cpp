#include "triangle_spirv.hpp"
#include <array>
#include <limits>
#include <support/rhi_support.hpp>
#include <thread>
#include <vector>

namespace voryx::test
{
namespace
{
constexpr Extent2D size{64, 64};
using Pixels = std::array<std::byte, 64 * 64 * 4>;
constexpr std::array attributes{
    VertexAttribute{0, VertexFormat::Float2, 0},
    VertexAttribute{1, VertexFormat::Float3, 8},
};
constexpr std::array<std::array<float, 5>, 4> vertices{{
    {-0.8f, -0.8f, 1, 0, 0},
    {0.8f, -0.8f, 1, 0, 0},
    {-0.8f, 0.8f, 1, 0, 0},
    {0.8f, 0.8f, 1, 0, 0},
}};
TextureHandle Color(IDevice &device)
{
    return Take(device.CreateTexture(
        {size, Format::RGBA8_UNorm,
         TextureUsage::ColorAttachment | TextureUsage::CopySrc, "test color"}));
}
PipelineHandle Pipeline(IDevice &device)
{
    GraphicsPipelineDesc desc{};
    desc.vertexSpirv = sample_data::triangleVertexSpirv;
    desc.fragmentSpirv = sample_data::triangleFragmentSpirv;
    desc.vertexStrideBytes = 20;
    desc.vertexAttributes = attributes;
    return Take(device.CreateGraphicsPipeline(desc));
}
void Transition(ICommandList &command, TextureHandle color, TextureUse before,
                TextureUse after)
{
    const std::array barriers{TextureBarrier{color, before, after}};
    Take(command.Barrier(barriers));
}
void Pixel(const Pixels &pixels, std::size_t x, std::size_t y,
           std::array<unsigned, 4> expected)
{
    for (std::size_t channel = 0; channel < 4; ++channel) {
        EXPECT_EQ(std::to_integer<unsigned>(pixels[(y * 64 + x) * 4 + channel]),
                  expected[channel]);
    }
}
void Dynamic(ICommandList &command)
{
    Take(command.SetViewport({0, 0, 64, 64, 0, 1}));
    Take(command.SetScissor({0, 0, 64, 64}));
}
} // namespace

TEST_F(BufferRhiTest, DevicePassClearsRepeatedAttachmentsAndReadsExactBytes)
{
    const auto color = Color(*device);
    Pixels pixels{};
    ExpectError(device->ReadTexture(color, pixels), ErrorCode::InvalidState);
    auto command = Command();
    Take(command->Begin());
    Transition(*command, color, TextureUse::Initial,
               TextureUse::ColorAttachment);
    Take(command->BeginRenderPass({color, {1, 0, 0, 1}, {}, 1}));
    Take(command->EndRenderPass());
    // 同一用途下连续清除，验证 Pass 间的附件同步。
    Take(command->BeginRenderPass({color, {0, 1, 0, 1}, {}, 1}));
    Take(command->EndRenderPass());
    Transition(*command, color, TextureUse::ColorAttachment,
               TextureUse::CopySrc);
    Take(command->End());
    Submit(*command);
    ExpectError(device->ReadTexture(color, std::span(pixels).first(3)),
                ErrorCode::InvalidArgument);
    Take(device->ReadTexture(color, pixels));
    Pixel(pixels, 0, 0, {0, 255, 0, 255});
    Pixel(pixels, 63, 63, {0, 255, 0, 255});
    Take(device->Destroy(color));
}

TEST_F(BufferRhiTest, DeviceDrawValidatesOrderBoundsAndRecoversWithinPass)
{
    const auto color = Color(*device);
    const auto pipeline = Pipeline(*device);
    const auto vb =
        Buffer(sizeof(vertices), BufferUsage::Vertex | BufferUsage::CopyDst,
               std::as_bytes(std::span(vertices)));
    auto command = Command();
    ExpectError(command->Draw(3), ErrorCode::InvalidState);
    Take(command->Begin());
    Transition(*command, color, TextureUse::Initial,
               TextureUse::ColorAttachment);
    const std::array barriers{
        BufferBarrier{vb, BufferUse::Initial, BufferUse::VertexRead}};
    Take(command->Barrier(barriers));
    Take(command->BeginRenderPass({color, {}, {}, 1}));
    ExpectError(command->BeginRenderPass({color, {}, {}, 1}),
                ErrorCode::InvalidState);
    ExpectError(command->End(), ErrorCode::InvalidState);
    ExpectError(command->CopyBuffer({}), ErrorCode::InvalidState);
    ExpectError(command->Barrier(barriers), ErrorCode::InvalidState);
    ExpectError(command->Draw(3), ErrorCode::InvalidState);
    Take(command->SetPipeline(pipeline));
    ExpectError(command->SetVertexBuffer(vb, 1), ErrorCode::InvalidArgument);
    Take(command->SetVertexBuffer(vb));
    ExpectError(command->SetViewport(
                    {0, 0, std::numeric_limits<float>::quiet_NaN(), 64}),
                ErrorCode::InvalidArgument);
    ExpectError(command->SetScissor({63, 0, 2, 64}),
                ErrorCode::InvalidArgument);
    Dynamic(*command);
    ExpectError(command->Draw(0), ErrorCode::InvalidArgument);
    ExpectError(command->Draw(6), ErrorCode::InvalidArgument);
    ExpectError(command->Draw(3, std::numeric_limits<std::uint32_t>::max()),
                ErrorCode::InvalidArgument);
    ExpectError(command->DrawIndexed(3), ErrorCode::InvalidState);
    Take(command->Draw(3));
    Take(command->EndRenderPass());
    Take(command->End());
    Submit(*command);
    Take(command->Reset());
    Take(command->Begin());
    Take(command->BeginRenderPass({color, {}, {}, 1}));
    // 新 Pass 必须重新设置管线和动态状态。
    ExpectError(command->Draw(3), ErrorCode::InvalidState);
    Take(command->Reset());
}

TEST_F(BufferRhiTest, DeviceDrawPinsAllResourcesUntilResetOrCompletedSubmit)
{
    const auto color = Color(*device);
    const auto pipeline = Pipeline(*device);
    const auto vb =
        Buffer(sizeof(vertices), BufferUsage::Vertex | BufferUsage::CopyDst,
               std::as_bytes(std::span(vertices)));
    const std::array<std::uint16_t, 3> indices{0, 1, 2};
    const auto ib =
        Buffer(sizeof(indices), BufferUsage::Index | BufferUsage::CopyDst,
               std::as_bytes(std::span(indices)));
    auto command = Command();
    Take(command->Begin());
    Transition(*command, color, TextureUse::Initial,
               TextureUse::ColorAttachment);
    const std::array barriers{
        BufferBarrier{vb, BufferUse::Initial, BufferUse::VertexRead},
        BufferBarrier{ib, BufferUse::Initial, BufferUse::IndexRead}};
    Take(command->Barrier(barriers));
    Take(command->BeginRenderPass({color, {}, {}, 1}));
    Take(command->SetPipeline(pipeline));
    Take(command->SetVertexBuffer(vb));
    ExpectError(command->SetIndexBuffer(ib, IndexType::UInt16, 1),
                ErrorCode::InvalidArgument);
    Take(command->SetIndexBuffer(ib, IndexType::UInt16));
    Dynamic(*command);
    ExpectError(command->DrawIndexed(6), ErrorCode::InvalidArgument);
    Take(command->DrawIndexed(3));
    Take(command->EndRenderPass());
    Take(command->End());
    ExpectError(device->Destroy(color), ErrorCode::ResourceBusy);
    ExpectError(device->Destroy(pipeline), ErrorCode::ResourceBusy);
    ExpectError(device->Destroy(vb), ErrorCode::ResourceBusy);
    ExpectError(device->Destroy(ib), ErrorCode::ResourceBusy);
    ExpectError(device->WriteBuffer(vb, 0, {}), ErrorCode::ResourceBusy);
    Pixels pixels{};
    ExpectError(device->ReadTexture(color, pixels), ErrorCode::ResourceBusy);
    Submit(*command);
    Take(device->WriteBuffer(vb, 0, {}));
    Take(device->Destroy(pipeline));
    Take(device->Destroy(vb));
    Take(device->Destroy(ib));
    Take(device->Destroy(color));
}

TEST_F(BufferRhiTest, DevicePassBatchFailurePreservesStatesAndClearData)
{
    const auto color = Color(*device);
    auto first = Command();
    Take(first->Begin());
    Transition(*first, color, TextureUse::Initial, TextureUse::ColorAttachment);
    Take(first->BeginRenderPass({color, {1, 0, 0, 1}, {}, 1}));
    Take(first->EndRenderPass());
    Transition(*first, color, TextureUse::ColorAttachment, TextureUse::CopySrc);
    Take(first->End());
    auto second = Command();
    Take(second->Begin());
    Transition(*second, color, TextureUse::Initial,
               TextureUse::ColorAttachment);
    Take(second->End());
    const std::array<ICommandList *, 2> lists{first.get(), second.get()};
    ExpectError(device->Submit({lists}), ErrorCode::InvalidState);
    EXPECT_EQ(Take(first->GetState()), CommandState::Executable);
    Take(second->Reset());
    Submit(*first);
    Pixels pixels{};
    Take(device->ReadTexture(color, pixels));
    Pixel(pixels, 32, 32, {255, 0, 0, 255});
    // 同一附件在另一列表继续使用，提交时按整批顺序预测状态。
    Take(first->Reset());
    Take(first->Begin());
    Transition(*first, color, TextureUse::CopySrc, TextureUse::ColorAttachment);
    Take(first->End());
    Take(second->Begin());
    Take(second->BeginRenderPass({color, {0, 0, 1, 1}, {}, 1}));
    Take(second->EndRenderPass());
    Transition(*second, color, TextureUse::ColorAttachment,
               TextureUse::CopySrc);
    Take(second->End());
    Take(device->Submit({lists}));
    Take(device->ReadTexture(color, pixels));
    Pixel(pixels, 32, 32, {0, 0, 255, 255});
}

TEST_F(BufferRhiTest,
       DeviceTexturesValidateDescriptorsOwnershipGenerationAndThread)
{
    ExpectError(device->CreateTexture({}), ErrorCode::InvalidArgument);
    ExpectError(device->CreateTexture(
                    {{1, 1}, Format::RGBA8_UNorm, TextureUsage::Sampled, {}}),
                ErrorCode::Unsupported);
    ExpectError(device->CreateTexture({{1, 1},
                                       Format::RGBA8_UNorm,
                                       TextureUsage::ColorAttachment
                                           | TextureUsage::DepthAttachment,
                                       {}}),
                ErrorCode::InvalidArgument);
    ExpectError(device->CreateTexture(
                    {{device->GetCapabilities().maxTexture2DSize + 1, 1},
                     Format::RGBA8_UNorm,
                     TextureUsage::ColorAttachment,
                     {}}),
                ErrorCode::Unsupported);
    const auto first = Color(*device);
    auto other = Take(CreateDevice({BackendType::Null, "foreign texture"}));
    ExpectError(other->Destroy(first), ErrorCode::WrongDevice);
    std::thread worker([&]() {
        ExpectError(device->Destroy(first), ErrorCode::WrongThread);
    });
    worker.join();
    Take(device->Destroy(first));
    const auto second = Color(*device);
    EXPECT_NE(first, second);
    ExpectError(device->Destroy(first), ErrorCode::InvalidHandle);
    Take(device->Destroy(second));
}

TEST_F(BufferRhiTest,
       DevicePassRejectsDepthMismatchAndReleasesUnfinishedCommands)
{
    const auto color = Color(*device);
    const auto pipeline = Pipeline(*device);
    const auto depth = Take(device->CreateTexture(
        {size, Format::D32_Float, TextureUsage::DepthAttachment, {}}));
    const auto wrong = Take(device->CreateTexture(
        {{32, 32}, Format::D32_Float, TextureUsage::DepthAttachment, {}}));
    auto command = Command();
    Take(command->Begin());
    ExpectError(command->BeginRenderPass({color, {}, wrong, 1}),
                ErrorCode::InvalidArgument);
    Take(command->BeginRenderPass({color, {}, depth, 1}));
    ExpectError(command->SetPipeline(pipeline), ErrorCode::InvalidArgument);
    Take(command->Reset());
    Take(device->Destroy(color));
    Take(device->Destroy(depth));
    Take(device->Destroy(wrong));
    Take(command->Begin());
    const auto late = Color(*device);
    Take(command->BeginRenderPass({late, {}, {}, 1}));
    device.reset();
    ExpectError(command->EndRenderPass(), ErrorCode::InvalidState);
    command.reset();
}

TEST_F(BufferRhiTest, DeviceOffscreenAttachmentsRejectPresentState)
{
    const auto color = Color(*device);
    auto command = Command();
    Take(command->Begin());
    const std::array invalid{
        TextureBarrier{color, TextureUse::Initial, TextureUse::Present}};
    ExpectError(command->Barrier(invalid), ErrorCode::InvalidArgument);
    Transition(*command, color, TextureUse::Initial,
               TextureUse::ColorAttachment);
    Take(command->BeginRenderPass({color, {0, 1, 0, 1}, {}, 1}));
    Take(command->EndRenderPass());
    Transition(*command, color, TextureUse::ColorAttachment,
               TextureUse::CopySrc);
    Take(command->End());
    Take(device->Wait(Submit(*command), 0));
    Pixels pixels{};
    Take(device->ReadTexture(color, pixels));
    Pixel(pixels, 32, 32, {0, 255, 0, 255});
}

#if VORYX_TEST_VULKAN
TEST_F(BufferRhiTest, DeviceIndexedDrawUsesBothIndexWidthsOffsetsAndScissor)
{
    const auto color = Color(*device);
    const auto pipeline = Pipeline(*device);
    const auto vb =
        Buffer(sizeof(vertices), BufferUsage::Vertex | BufferUsage::CopyDst,
               std::as_bytes(std::span(vertices)));
    const std::array<std::uint16_t, 8> small{0, 0, 1, 2, 3, 2, 4, 3};
    const std::array<std::uint32_t, 8> large{0, 0, 1, 2, 3, 2, 4, 3};
    TextureUse previous = TextureUse::Initial;
    for (const auto type : {IndexType::UInt16, IndexType::UInt32}) {
        const auto bytes = type == IndexType::UInt16
                               ? ByteSpan(std::as_bytes(std::span(small)))
                               : ByteSpan(std::as_bytes(std::span(large)));
        const auto ib = Buffer(
            bytes.size(), BufferUsage::Index | BufferUsage::CopyDst, bytes);
        auto command = Command();
        Take(command->Begin());
        Transition(*command, color, previous, TextureUse::ColorAttachment);
        std::vector<BufferBarrier> barriers{
            {ib, BufferUse::Initial, BufferUse::IndexRead}};
        if (previous == TextureUse::Initial) {
            barriers.push_back({vb, BufferUse::Initial, BufferUse::VertexRead});
        }
        Take(command->Barrier(barriers));
        Take(command->BeginRenderPass({color, {0, 0, 1, 1}, {}, 1}));
        Take(command->SetPipeline(pipeline));
        Take(command->SetVertexBuffer(vb));
        Take(command->SetIndexBuffer(ib, type,
                                     type == IndexType::UInt16 ? 2 : 4));
        Dynamic(*command);
        Take(command->SetScissor({0, 0, 32, 64}));
        Take(command->DrawIndexed(6, 1, -1));
        Take(command->EndRenderPass());
        Transition(*command, color, TextureUse::ColorAttachment,
                   TextureUse::CopySrc);
        Take(command->End());
        Submit(*command);
        Pixels pixels{};
        Take(device->ReadTexture(color, pixels));
        Pixel(pixels, 24, 32, {255, 0, 0, 255});
        Pixel(pixels, 40, 32, {0, 0, 255, 255});
        Pixel(pixels, 2, 2, {0, 0, 255, 255});
        Take(device->Destroy(ib));
        previous = TextureUse::CopySrc;
    }
}

TEST_F(BufferRhiTest,
       DeviceNonIndexedDrawReusesPipelineAndDepthAcrossSubmissions)
{
    const auto color = Color(*device);
    const auto depth = Take(device->CreateTexture(
        {size, Format::D32_Float, TextureUsage::DepthAttachment, {}}));
    const std::array depthAttributes{
        VertexAttribute{0, VertexFormat::Float3, 0},
        VertexAttribute{1, VertexFormat::Float3, 12}};
    GraphicsPipelineDesc desc{};
    desc.vertexSpirv = sample_data::depthVertexSpirv;
    desc.fragmentSpirv = sample_data::triangleFragmentSpirv;
    desc.vertexStrideBytes = 24;
    desc.vertexAttributes = depthAttributes;
    desc.depthTest = true;
    desc.depthWrite = true;
    desc.depthCompare = DepthCompare::Less;
    const auto pipeline = Take(device->CreateGraphicsPipeline(desc));
    const std::array<std::array<float, 6>, 6> geometry{{
        {-0.8f, -0.8f, 0.2f, 1, 0, 0},
        {0.8f, -0.8f, 0.2f, 1, 0, 0},
        {0, 0.8f, 0.2f, 1, 0, 0},
        {-0.8f, -0.8f, 0.8f, 0, 1, 0},
        {0.8f, -0.8f, 0.8f, 0, 1, 0},
        {0, 0.8f, 0.8f, 0, 1, 0},
    }};
    const auto vb =
        Buffer(sizeof(geometry), BufferUsage::Vertex | BufferUsage::CopyDst,
               std::as_bytes(std::span(geometry)));
    for (int order = 0; order < 2; ++order) {
        auto command = Command();
        Take(command->Begin());
        Transition(*command, color,
                   order == 0 ? TextureUse::Initial : TextureUse::CopySrc,
                   TextureUse::ColorAttachment);
        Transition(*command, depth,
                   order == 0 ? TextureUse::Initial : TextureUse::DepthWrite,
                   TextureUse::DepthWrite);
        if (order == 0) {
            const std::array barriers{
                BufferBarrier{vb, BufferUse::Initial, BufferUse::VertexRead}};
            Take(command->Barrier(barriers));
        }
        Take(command->BeginRenderPass({color, {0, 0, 1, 1}, depth, 1}));
        Take(command->SetPipeline(pipeline));
        Take(command->SetVertexBuffer(vb));
        Dynamic(*command);
        Take(command->Draw(3, order == 0 ? 0 : 3));
        Take(command->Draw(3, order == 0 ? 3 : 0));
        Take(command->EndRenderPass());
        Transition(*command, color, TextureUse::ColorAttachment,
                   TextureUse::CopySrc);
        Take(command->End());
        Submit(*command);
        Pixels pixels{};
        Take(device->ReadTexture(color, pixels));
        Pixel(pixels, 32, 32, {255, 0, 0, 255});
        Pixel(pixels, 2, 2, {0, 0, 255, 255});
    }
}
#endif
} // namespace voryx::test

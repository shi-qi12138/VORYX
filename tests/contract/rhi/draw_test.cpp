#include "triangle_spirv.hpp"
#include <array>
#include <limits>
#include <support/attachment_support.hpp>
#include <vector>

namespace voryx::test
{
namespace
{
using namespace attachment;
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
PipelineHandle Pipeline(IDevice &device)
{
    GraphicsPipelineDesc desc{};
    desc.vertexSpirv = sample_data::triangleVertexSpirv;
    desc.fragmentSpirv = sample_data::triangleFragmentSpirv;
    desc.vertexStrideBytes = 20;
    desc.vertexAttributes = attributes;
    return Take(device.CreateGraphicsPipeline(desc));
}
void Dynamic(ICommandList &command)
{
    Take(command.SetViewport({0, 0, 64, 64, 0, 1}));
    Take(command.SetScissor({0, 0, 64, 64}));
}
} // namespace

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

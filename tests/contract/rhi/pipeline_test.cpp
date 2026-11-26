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
constexpr std::array attributes{
    VertexAttribute{0, VertexFormat::Float2, 0},
    VertexAttribute{1, VertexFormat::Float3, 8},
};
GraphicsPipelineDesc Description()
{
    GraphicsPipelineDesc desc{};
    desc.vertexSpirv = sample_data::triangleVertexSpirv;
    desc.fragmentSpirv = sample_data::triangleFragmentSpirv;
    desc.vertexStrideBytes = 20;
    desc.vertexAttributes = attributes;
    return desc;
}
} // namespace
TEST_F(BufferRhiTest, PipelineOwnsInputsAndRejectsStaleAndForeignHandles)
{
    auto foreign =
        Take(CreateDevice({BackendType::Null, "pipeline validation"}));
    auto words =
        std::vector<std::uint32_t>(std::begin(sample_data::triangleVertexSpirv),
                                   std::end(sample_data::triangleVertexSpirv));
    auto inputs = attributes;
    auto desc = Description();
    desc.vertexSpirv = words;
    desc.vertexAttributes = inputs;
    const auto first = Take(device->CreateGraphicsPipeline(desc));
    words.clear();
    inputs[0].offsetBytes = 100;
    ExpectError(foreign->Destroy(first), ErrorCode::WrongDevice);
    Take(device->Destroy(first));
    const auto second = Take(device->CreateGraphicsPipeline(Description()));
    EXPECT_NE(first, second);
    ExpectError(device->Destroy(first), ErrorCode::InvalidHandle);
    ExpectError(device->Destroy(PipelineHandle{}), ErrorCode::InvalidHandle);
    Take(device->Destroy(second));
    ExpectError(device->Destroy(second), ErrorCode::InvalidHandle);
}
TEST_F(BufferRhiTest, PipelineValidatesLayoutsAndRecoversAfterFailure)
{
    auto desc = Description();
    auto inputs = attributes;
    desc.vertexAttributes = inputs;
    inputs[1].location = 0;
    ExpectError(device->CreateGraphicsPipeline(desc),
                ErrorCode::InvalidArgument);
    inputs = attributes;
    inputs[1].offsetBytes = std::numeric_limits<std::uint32_t>::max();
    ExpectError(device->CreateGraphicsPipeline(desc),
                ErrorCode::InvalidArgument);
    inputs = attributes;
    inputs[1].format = static_cast<VertexFormat>(255);
    ExpectError(device->CreateGraphicsPipeline(desc),
                ErrorCode::InvalidArgument);
    inputs = attributes;
    desc.vertexStrideBytes = 0;
    ExpectError(device->CreateGraphicsPipeline(desc),
                ErrorCode::InvalidArgument);
    desc = Description();
    desc.depthWrite = true;
    ExpectError(device->CreateGraphicsPipeline(desc),
                ErrorCode::InvalidArgument);
    desc = Description();
    desc.blendMode = static_cast<BlendMode>(255);
    ExpectError(device->CreateGraphicsPipeline(desc),
                ErrorCode::InvalidArgument);
    Take(device->Destroy(Take(device->CreateGraphicsPipeline(Description()))));
}
TEST_F(BufferRhiTest, PipelineRejectsMalformedStagesAndResourceShaders)
{
    auto desc = Description();
    desc.vertexSpirv = sample_data::triangleFragmentSpirv;
    ExpectError(device->CreateGraphicsPipeline(desc),
                ErrorCode::InvalidArgument);
    std::vector words(std::begin(sample_data::triangleVertexSpirv),
                      std::end(sample_data::triangleVertexSpirv));
    words[5] = 0;
    desc.vertexSpirv = words;
    ExpectError(device->CreateGraphicsPipeline(desc),
                ErrorCode::InvalidArgument);
    desc.vertexSpirv = std::span(sample_data::triangleVertexSpirv).first(6);
    ExpectError(device->CreateGraphicsPipeline(desc),
                ErrorCode::InvalidArgument);
    desc.vertexSpirv = sample_data::resourceVertexSpirv;
    ExpectError(device->CreateGraphicsPipeline(desc),
                ErrorCode::InvalidArgument);
    Take(device->Destroy(Take(device->CreateGraphicsPipeline(Description()))));
}
TEST_F(BufferRhiTest, PipelineChecksThreadBeforeMutatingResources)
{
    const auto pipeline = Take(device->CreateGraphicsPipeline(Description()));
    std::thread worker([&]() {
        ExpectError(device->CreateGraphicsPipeline(Description()),
                    ErrorCode::WrongThread);
        ExpectError(device->Destroy(pipeline), ErrorCode::WrongThread);
    });
    worker.join();
    Take(device->Destroy(pipeline));
}
TEST_F(BufferRhiTest, PipelineCreatesDepthBlendAndCullStates)
{
    auto desc = Description();
    desc.depthTest = true;
    desc.depthWrite = true;
    desc.depthCompare = DepthCompare::Less;
    desc.cullMode = CullMode::Back;
    desc.frontFace = FrontFace::Clockwise;
    desc.blendMode = BlendMode::Alpha;
    const auto pipeline = Take(device->CreateGraphicsPipeline(desc));
    Take(device->Destroy(pipeline));
    desc.blendMode = BlendMode::PremultipliedAlpha;
    desc.colorFormat = PipelineColorFormat::BGRA8_UNorm;
    Take(device->Destroy(Take(device->CreateGraphicsPipeline(desc))));
}
} // namespace voryx::test

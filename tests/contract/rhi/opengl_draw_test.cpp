#include "triangle_glsl.hpp"
#include "triangle_spirv.hpp"

#include <support/attachment_support.hpp>

#include <array>
#include <string>
#include <utility>

namespace voryx::test
{
namespace
{
using namespace attachment;

constexpr std::array attributes{
    VertexAttribute{1, VertexFormat::Float3, 0},
    VertexAttribute{0, VertexFormat::Float2, 16},
};
using Vertex = std::array<float, 6>;
constexpr std::array<Vertex, 7> vertices{{
    {0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, -0.75f, -0.75f},
    {1, 0, 0, 0, 0.75f, -0.75f},
    {1, 0, 0, 0, -0.75f, 0.75f},
    {0, 1, 0, 0, -0.75f, -0.75f},
    {0, 1, 0, 0, 0.75f, -0.75f},
    {0, 1, 0, 0, -0.75f, 0.75f},
}};

GraphicsPipelineDesc Description()
{
    GraphicsPipelineDesc desc{};
    desc.vertexGlsl = sample_data::triangleVertexGlsl;
    desc.fragmentGlsl = sample_data::triangleFragmentGlsl;
    desc.vertexAttributes = attributes;
    desc.vertexStrideBytes = sizeof(Vertex);
    return desc;
}
} // namespace

TEST_F(BufferRhiTest, OpenGLValidatesShaderProgramsAndRecovers)
{
    auto desc = Description();
    desc.vertexGlsl = {};
    ExpectError(device->CreateGraphicsPipeline(desc),
                ErrorCode::InvalidArgument);
    desc.fragmentGlsl = {};
    desc.vertexSpirv = sample_data::triangleVertexSpirv;
    desc.fragmentSpirv = sample_data::triangleFragmentSpirv;
    ExpectError(device->CreateGraphicsPipeline(desc), ErrorCode::Unsupported);
    desc.vertexGlsl = std::string_view("shader\0text", 11);
    desc.fragmentGlsl = sample_data::triangleFragmentGlsl;
    ExpectError(device->CreateGraphicsPipeline(desc),
                ErrorCode::InvalidArgument);

    desc = Description();
    desc.vertexGlsl = "#version 330 core\nvoid main() { invalid_glsl; }";
    const auto invalid = device->CreateGraphicsPipeline(desc);
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.GetError().errorCode, ErrorCode::InvalidArgument);
    EXPECT_TRUE(
        invalid.GetError().message.starts_with("GLSL vertex compilation: "));
    desc = Description();
    desc.fragmentGlsl = R"glsl(#version 330 core
in vec4 vertexColor;
layout(location = 0) out vec4 fragmentColor;
void main() { fragmentColor = vertexColor; }
)glsl";
    const auto mismatch = device->CreateGraphicsPipeline(desc);
    ASSERT_FALSE(mismatch);
    EXPECT_EQ(mismatch.GetError().errorCode, ErrorCode::InvalidArgument);
    EXPECT_TRUE(mismatch.GetError().message.starts_with("GLSL program link: "));
    desc.fragmentGlsl = R"glsl(#version 330 core
uniform vec4 tint;
layout(location = 0) out vec4 fragmentColor;
void main() { fragmentColor = tint; }
)glsl";
    ExpectError(device->CreateGraphicsPipeline(desc), ErrorCode::Unsupported);

    desc = Description();
    desc.blendMode = BlendMode::Alpha;
    ExpectError(device->CreateGraphicsPipeline(desc), ErrorCode::Unsupported);
    desc = Description();
    desc.vertexSpirv = sample_data::triangleVertexSpirv;
    desc.fragmentSpirv = sample_data::triangleFragmentSpirv;
    Take(device->Destroy(Take(device->CreateGraphicsPipeline(desc))));
}

TEST_F(BufferRhiTest, OpenGLDrawsWithOffsetViewportAndTopLeftScissor)
{
    auto layout = attributes;
    auto desc = Description();
    auto source = std::string(desc.vertexGlsl);
    desc.vertexGlsl = source;
    desc.vertexAttributes = layout;
    const auto pipeline = Take(device->CreateGraphicsPipeline(desc));
    source.assign("invalid shader");
    layout[0].location = 15;

    // 红色三角形使用绑定字节偏移，绿色三角形使用非零首顶点。
    const auto vertex =
        Buffer(sizeof(vertices), BufferUsage::Vertex | BufferUsage::CopyDst,
               std::as_bytes(std::span(vertices)));
    const auto color = Color(*device);
    auto command = Command();
    Take(command->Begin());
    Transition(*command, color, TextureUse::Initial,
               TextureUse::ColorAttachment);
    const std::array toVertex{
        BufferBarrier{vertex, BufferUse::Initial, BufferUse::VertexRead}};
    Take(command->Barrier(toVertex));
    Take(command->BeginRenderPass({color, {0, 0, 1, 1}, {}, 1}));
    Take(command->SetVertexBuffer(vertex, sizeof(Vertex)));
    Take(command->SetPipeline(pipeline));
    Take(command->SetViewport({8, 4, 48, 48, 0, 1}));
    Take(command->SetScissor({16, 12, 28, 20}));
    Take(command->Draw(3));
    Take(command->SetVertexBuffer(vertex));
    Take(command->SetScissor({16, 26, 28, 6}));
    Take(command->Draw(3, 4));
    Take(command->EndRenderPass());
    Transition(*command, color, TextureUse::ColorAttachment,
               TextureUse::CopySrc);
    Take(command->End());
    ExpectError(device->Destroy(pipeline), ErrorCode::ResourceBusy);
    ExpectError(device->Destroy(vertex), ErrorCode::ResourceBusy);
    EXPECT_TRUE(Take(device->IsComplete(Submit(*command))));
    Pixels pixels{};
    Take(device->ReadTexture(color, pixels));
    Pixel(pixels, 22, 18, {255, 0, 0, 255});
    Pixel(pixels, 18, 30, {0, 255, 0, 255});
    Pixel(pixels, 18, 33, {0, 0, 255, 255});
    Pixel(pixels, 22, 45, {0, 0, 255, 255});
    Pixel(pixels, 15, 18, {0, 0, 255, 255});
    Pixel(pixels, 63, 63, {0, 0, 255, 255});

    // 另一 Pass 的全图清除必须忽略前一批提交的裁剪。
    Take(command->Reset());
    Take(command->Begin());
    Transition(*command, color, TextureUse::CopySrc,
               TextureUse::ColorAttachment);
    Take(command->BeginRenderPass({color, {0, 1, 0, 1}, {}, 1}));
    Take(command->EndRenderPass());
    Transition(*command, color, TextureUse::ColorAttachment,
               TextureUse::CopySrc);
    Take(command->End());
    Submit(*command);
    Take(device->ReadTexture(color, pixels));
    Pixel(pixels, 22, 18, {0, 255, 0, 255});
    Pixel(pixels, 63, 63, {0, 255, 0, 255});
    Take(device->Destroy(vertex));
    Take(device->Destroy(pipeline));
    Take(device->Destroy(color));
}

TEST_F(BufferRhiTest, OpenGLRejectsFractionalViewportBeforeChangingPixels)
{
    const auto color = Color(*device);
    auto command = Command();
    Take(command->Begin());
    Transition(*command, color, TextureUse::Initial,
               TextureUse::ColorAttachment);
    Take(command->BeginRenderPass({color, {1, 0, 0, 1}, {}, 1}));
    Take(command->EndRenderPass());
    Transition(*command, color, TextureUse::ColorAttachment,
               TextureUse::CopySrc);
    Take(command->End());
    Submit(*command);

    Take(command->Reset());
    Take(command->Begin());
    Transition(*command, color, TextureUse::CopySrc,
               TextureUse::ColorAttachment);
    Take(command->BeginRenderPass({color, {0, 1, 0, 1}, {}, 1}));
    Take(command->SetViewport({0.5f, 0, 63, 64, 0, 1}));
    Take(command->EndRenderPass());
    Transition(*command, color, TextureUse::ColorAttachment,
               TextureUse::CopySrc);
    Take(command->End());
    const std::array list{command.get()};
    ExpectError(device->Submit({list}), ErrorCode::Unsupported);
    EXPECT_EQ(Take(command->GetState()), CommandState::Executable);
    Take(command->Reset());
    Pixels pixels{};
    Take(device->ReadTexture(color, pixels));
    Pixel(pixels, 0, 0, {255, 0, 0, 255});
    Pixel(pixels, 63, 63, {255, 0, 0, 255});
    Take(device->Destroy(color));
}
} // namespace voryx::test

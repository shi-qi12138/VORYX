#include "opengl_sample.hpp"
#include "triangle_glsl.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <stdexcept>

namespace
{
using voryx::sample::Take;

void Run(voryx::IDevice &device)
{
    using namespace voryx;
    constexpr Extent2D extent{64, 64};

    constexpr std::array<std::array<float, 5>, 3> vertices{{
        {-0.75f, -0.75f, 1, 0, 0},
        {0.75f, -0.75f, 1, 0, 0},
        {-0.75f, 0.75f, 1, 0, 0},
    }};
    constexpr std::array attributes{
        VertexAttribute{0, VertexFormat::Float2, 0},
        VertexAttribute{1, VertexFormat::Float3, 2 * sizeof(float)},
    };
    GraphicsPipelineDesc desc{};
    desc.vertexGlsl = sample_data::triangleVertexGlsl;
    desc.fragmentGlsl = sample_data::triangleFragmentGlsl;
    desc.vertexStrideBytes = sizeof(vertices[0]);
    desc.vertexAttributes = attributes;
    const auto pipeline = Take(device.CreateGraphicsPipeline(desc));
    const auto vertex = Take(device.CreateBuffer(
        {sizeof(vertices), BufferUsage::Vertex | BufferUsage::CopyDst,
         "vertices"},
        std::as_bytes(std::span(vertices))));
    const auto color = Take(device.CreateTexture(
        {extent, Format::RGBA8_UNorm,
         TextureUsage::ColorAttachment | TextureUsage::CopySrc, "color"}));

    auto command = Take(device.CreateCommandList());
    Take(command->Begin());
    const std::array toVertex{
        BufferBarrier{vertex, BufferUse::Initial, BufferUse::VertexRead}};
    const std::array toAttachment{TextureBarrier{color, TextureUse::Initial,
                                                 TextureUse::ColorAttachment}};
    Take(command->Barrier(toVertex));
    Take(command->Barrier(toAttachment));
    Take(command->BeginRenderPass({color, {0, 0, 1, 1}, {}, 1}));
    Take(command->SetPipeline(pipeline));
    Take(command->SetVertexBuffer(vertex));
    Take(command->SetViewport({0, 0, 64, 64, 0, 1}));
    Take(command->SetScissor({0, 0, extent.width, extent.height}));
    Take(command->Draw(3));
    Take(command->EndRenderPass());
    const std::array toReadback{TextureBarrier{
        color, TextureUse::ColorAttachment, TextureUse::CopySrc}};
    Take(command->Barrier(toReadback));
    Take(command->End());
    const std::array list{command.get()};
    Take(device.Submit({list}));

    // 在远离三角形边缘的位置检查颜色和上下方向。
    std::array<std::byte, extent.width * extent.height * 4> pixels{};
    Take(device.ReadTexture(color, pixels));
    const auto pixel = [&](std::uint32_t x, std::uint32_t y) {
        const auto offset = (y * extent.width + x) * 4;
        return std::array{pixels[offset], pixels[offset + 1],
                          pixels[offset + 2], pixels[offset + 3]};
    };
    constexpr std::array red{std::byte{255}, std::byte{0}, std::byte{0},
                             std::byte{255}};
    constexpr std::array blue{std::byte{0}, std::byte{0}, std::byte{255},
                              std::byte{255}};
    if (pixel(24, 18) != red || pixel(24, 45) != blue || pixel(60, 2) != blue) {
        throw std::runtime_error("Unexpected OpenGL triangle pixels");
    }
    Take(device.Destroy(color));
    Take(device.Destroy(vertex));
    Take(device.Destroy(pipeline));
    const auto &info = device.GetInfo();
    std::printf("OpenGL offscreen triangle verified on %s (software=%s)\n",
                info.name.c_str(), info.softwareAdapter ? "true" : "false");
}
} // namespace

int main(int argc, char **argv)
{
    return voryx::sample::RunOpenGL(argc, argv, "opengl_triangle",
                                    "offscreen triangle", Run);
}

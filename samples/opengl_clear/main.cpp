#include "opengl_sample.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace
{
using voryx::sample::Take;

void Run(voryx::IDevice &device)
{
    using namespace voryx;
    constexpr Extent2D extent{64, 64};
    const auto color = Take(device.CreateTexture(
        {extent, Format::RGBA8_UNorm,
         TextureUsage::ColorAttachment | TextureUsage::CopySrc,
         "red attachment"}));

    auto command = Take(device.CreateCommandList());
    Take(command->Begin());
    const std::array toAttachment{TextureBarrier{color, TextureUse::Initial,
                                                 TextureUse::ColorAttachment}};
    Take(command->Barrier(toAttachment));
    Take(command->BeginRenderPass({color, {1, 0, 0, 1}, {}, 1}));
    Take(command->EndRenderPass());
    const std::array toReadback{TextureBarrier{
        color, TextureUse::ColorAttachment, TextureUse::CopySrc}};
    Take(command->Barrier(toReadback));
    Take(command->End());
    const std::array list{command.get()};
    Take(device.Submit({list}));

    // 默认提交为同步，返回后可以直接读回紧密排列的 RGBA 像素。
    std::array<std::byte, extent.width * extent.height * 4> pixels{};
    Take(device.ReadTexture(color, pixels));
    constexpr std::array red{std::byte{255}, std::byte{0}, std::byte{0},
                             std::byte{255}};
    for (std::size_t index = 0; index < pixels.size(); index += red.size()) {
        if (!std::equal(red.begin(), red.end(), pixels.data() + index)) {
            throw std::runtime_error("Pixel mismatch at index "
                                     + std::to_string(index / red.size()));
        }
    }
    Take(device.Destroy(color));
    const auto &info = device.GetInfo();
    std::printf(
        "OpenGL offscreen clear verified: %zu red pixels on %s (software=%s)\n",
        pixels.size() / red.size(), info.name.c_str(),
        info.softwareAdapter ? "true" : "false");
}
} // namespace

int main(int argc, char **argv)
{
    return voryx::sample::RunOpenGL(argc, argv, "opengl_clear",
                                    "offscreen clear", Run);
}

#pragma once

#include <support/rhi_support.hpp>

#include <array>

namespace voryx::test::attachment
{
constexpr Extent2D size{64, 64};
using Pixels = std::array<std::byte, size.width * size.height * 4>;
inline TextureHandle Color(IDevice &device)
{
    return Take(device.CreateTexture(
        {size, Format::RGBA8_UNorm,
         TextureUsage::ColorAttachment | TextureUsage::CopySrc, "test color"}));
}
inline void Transition(ICommandList &command, TextureHandle color,
                       TextureUse before, TextureUse after)
{
    const std::array barriers{TextureBarrier{color, before, after}};
    Take(command.Barrier(barriers));
}
inline void Pixel(const Pixels &pixels, std::size_t x, std::size_t y,
                  std::array<unsigned, 4> expected)
{
    for (std::size_t channel = 0; channel < 4; ++channel) {
        EXPECT_EQ(std::to_integer<unsigned>(
                      pixels[(y * size.width + x) * 4 + channel]),
                  expected[channel]);
    }
}
} // namespace voryx::test::attachment

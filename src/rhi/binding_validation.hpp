#pragma once

#include <voryx/rhi/binding.hpp>

namespace voryx::rhi_detail
{
inline constexpr std::size_t maxBindingSlots = 8;

inline bool ValidBindingLayout(const BindingLayoutDesc &desc) noexcept
{
    if (desc.entries.empty() || desc.entries.size() > maxBindingSlots) {
        return false;
    }
    std::uint32_t slots = 0;
    for (const auto &entry : desc.entries) {
        const auto visibility = static_cast<std::uint8_t>(entry.visibility);
        if (entry.slot >= 32 || entry.type > BindingType::Sampler
            || visibility == 0 || visibility > 3) {
            return false;
        }
        const auto slot = 1u << entry.slot;
        if ((slots & slot) != 0) {
            return false;
        }
        slots |= slot;
    }
    return true;
}

inline bool ValidSampler(const SamplerDesc &desc) noexcept
{
    return desc.minFilter <= TextureFilter::Linear
           && desc.magFilter <= TextureFilter::Linear
           && desc.addressU <= TextureAddressMode::Repeat
           && desc.addressV <= TextureAddressMode::Repeat;
}

inline bool ValidSampledTexture(const Rgba8TextureDesc &desc) noexcept
{
    if (desc.extent.width == 0 || desc.extent.height == 0
        || desc.extent.width > 4096 || desc.extent.height > 4096) {
        return false;
    }
    const std::size_t rowBytes = std::size_t(desc.extent.width) * 4;
    const auto pitch = desc.rowPitchBytes == 0 ? rowBytes : desc.rowPitchBytes;
    // 用除法判断最后一行是否完整，避开极端行距的乘法溢出。
    return pitch >= rowBytes && desc.pixels.size() >= rowBytes
           && desc.extent.height - 1 <= (desc.pixels.size() - rowBytes) / pitch;
}
} // namespace voryx::rhi_detail

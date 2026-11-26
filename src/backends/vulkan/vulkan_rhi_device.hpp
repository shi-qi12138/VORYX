#pragma once

#include "vulkan_context.hpp"

#include <voryx/rhi/device.hpp>

namespace voryx::vulkan_backend
{
inline constexpr std::uint32_t windowFrameSlots = 2;
Result<std::unique_ptr<IDevice>>
CreateVulkanRhiDevice(std::uint64_t identity, const DeviceDesc &desc,
                      std::shared_ptr<Context> context) noexcept;
} // namespace voryx::vulkan_backend

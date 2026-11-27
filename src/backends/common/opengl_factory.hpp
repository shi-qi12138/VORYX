#pragma once

#include <voryx/rhi/device.hpp>

namespace voryx::detail
{
Result<std::unique_ptr<IDevice>>
CreateOpenGLDevice(std::uint64_t identity, const DeviceDesc &desc) noexcept;
} // namespace voryx::detail

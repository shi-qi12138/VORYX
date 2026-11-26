#pragma once

#include <voryx/core/result.hpp>
#include <voryx/rhi/device.hpp>

#include <memory>

namespace voryx
{
/// 在调用线程创建设备；Null 必须显式选择，Auto 不会回退到 Null。
[[nodiscard]] Result<std::unique_ptr<IDevice>>
CreateDevice(const DeviceDesc &desc) noexcept;
} // namespace voryx

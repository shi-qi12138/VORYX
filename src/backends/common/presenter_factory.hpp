#pragma once

#include <voryx/platform/native_window.hpp>
#include <voryx/rhi/presenter.hpp>
#include <voryx/rhi/types.hpp>

#include <memory>

namespace voryx::detail
{
Result<std::unique_ptr<IPresenter>>
CreateWin32VulkanPresenter(std::uint64_t identity, const DeviceDesc &device,
                           NativeWindowDesc window) noexcept;
Result<std::unique_ptr<IPresenter>>
CreateXlibVulkanPresenter(std::uint64_t identity, const DeviceDesc &device,
                          NativeWindowDesc window) noexcept;
Result<std::unique_ptr<IPresenter>>
CreateWaylandVulkanPresenter(std::uint64_t identity, const DeviceDesc &device,
                             NativeWindowDesc window) noexcept;
} // namespace voryx::detail

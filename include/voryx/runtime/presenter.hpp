#pragma once

#include <voryx/platform/native_window.hpp>
#include <voryx/rhi/presenter.hpp>
#include <voryx/rhi/types.hpp>

#include <memory>

namespace voryx
{
struct PresenterDesc
{
    DeviceDesc device;
    NativeWindowDesc window;
};

/// 通用窗口入口。只借用窗口，始终在窗口线程创建、调用和销毁。
Result<std::unique_ptr<IFramePresenter>>
CreateFramePresenter(const PresenterDesc &desc) noexcept;

/// Vulkan 固定布局示例入口。必须先于窗口释放。
Result<std::unique_ptr<IPresenter>>
CreatePresenter(const PresenterDesc &desc) noexcept;
} // namespace voryx

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

/// 只借用窗口。必须在窗口线程创建、调用和销毁，且先于窗口释放。
Result<std::unique_ptr<IPresenter>>
CreatePresenter(const PresenterDesc &desc) noexcept;
} // namespace voryx

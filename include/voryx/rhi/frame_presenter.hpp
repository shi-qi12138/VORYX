#pragma once

#include <voryx/core/result.hpp>
#include <voryx/rhi/texture.hpp>

namespace voryx
{
class IDevice;

/// 提供宿主窗口的通用帧流程，绘制通过 IDevice 的命令列表完成。
class IFramePresenter
{
public:
    virtual ~IFramePresenter() = default;
    IFramePresenter(const IFramePresenter &) = delete;
    IFramePresenter &operator=(const IFramePresenter &) = delete;

    /// 借用窗口设备。设备随 Presenter 释放，操作限窗口线程。
    virtual IDevice &GetDevice() noexcept = 0;
    /// 停止窗口工作并按纳秒预算排空在途工作。超时或资源忙后可以重试。
    virtual Result<void> Shutdown(TimeoutNs timeoutNs) noexcept = 0;
    /// 获取活动窗口帧。零预算仅轮询 GPU 与呈现等待。
    /// 颜色图像逻辑用途为 Present，内容未定义。
    virtual Result<FrameInfo>
    AcquireFrame(TimeoutNs timeoutNs = 50'000'000) noexcept = 0;
    /// 呈现和消费已提交的活动帧，提交令牌须与该帧匹配。
    /// 异步帧成功返回时呈现请求已入队。
    virtual Result<void> Present(FrameToken frame,
                                 SubmissionToken submission) noexcept = 0;
    /// 取消尚未提交的帧。先重置或释放引用本帧图像的命令列表。
    virtual Result<void> CancelFrame(FrameToken frame) noexcept = 0;
    /// Wayland 由宿主报告物理像素尺寸。任一维为零时暂停。
    /// Win32 与 X11 自动查询尺寸，显式设置返回 Unsupported。
    virtual Result<void> SetFramebufferExtent(Extent2D extent) noexcept = 0;

protected:
    IFramePresenter() = default;
};

} // namespace voryx

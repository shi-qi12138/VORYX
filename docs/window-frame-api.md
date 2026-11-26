# 设备绘制与窗口帧

Win32、X11 和原生 Wayland 可将设备 Pass 直接绘制到 Vulkan 交换链图像。窗口继续由宿主创建并持有。应用通过已有的 CreatePresenter 创建窗口渲染入口，再从同一个 Presenter 借用 IDevice。应用包含 `<voryx/runtime/presenter.hpp>` 和 `<voryx/rhi/device.hpp>`，链接 `Voryx::runtime`。 [Win32 接入](win32-clear.md) · [X11 接入](xlib-clear.md) · [Wayland 接入](wayland-clear.md)

## 一帧怎样绘制

1. 通过 CreatePresenter 创建 Presenter，再用 GetDevice 借用设备。使用当前设备创建缓冲、纹理、绑定与管线。
2. 调用 AcquireFrame 获取令牌、颜色图像、实际像素尺寸和颜色格式。用 colorFormat 配置兼容管线，使用实际尺寸设置视口、裁剪和可选深度附件。
3. 在 Pass 外将图像由 Present 转为 ColorAttachment。随后用借用颜色句柄创建 Pass，按需绑定管线、几何和设备资源，调用 Draw 或 DrawIndexed。
4. 结束 Pass，将图像由 ColorAttachment 转为 Present，再 End。用 SubmitDesc.frame 随命令列表提交本帧。
5. 将 FrameToken 和本帧返回的 SubmissionToken 传给 Presenter::Present。下一帧重新获取窗口图像，可以复用非窗口资源。

以下示例给出核心顺序。每个 Take 都检查 Result 并返回成功值。完整程序包含管线、顶点、索引、纹理、常量、深度附件与错误出口。 [完整可编译示例](../samples/common/window_frame_sample.hpp)

```cpp
auto &device = presenter->GetDevice();
const auto frame = Take(presenter->AcquireFrame());
auto list = Take(device.CreateCommandList());
Take(list->Begin());
const std::array before{
    voryx::TextureBarrier{frame.color, voryx::TextureUse::Present,
                          voryx::TextureUse::ColorAttachment},
};
Take(list->Barrier(before));
Take(list->BeginRenderPass({frame.color, {0.1f, 0.15f, 0.3f, 1}, {}, 1}));
// 这里可以设置管线、顶点、视口、裁剪和绑定，再录制绘制。
Take(list->EndRenderPass());
const std::array after{
    voryx::TextureBarrier{frame.color, voryx::TextureUse::ColorAttachment,
                          voryx::TextureUse::Present},
};
Take(list->Barrier(after));
Take(list->End());
const std::array<voryx::ICommandList *, 1> lists{list.get()};
const auto submitted = Take(device.Submit({lists, frame.token}));
Take(presenter->Present(frame.token, submitted));
```

## 异步窗口帧

窗口设备的 `GetCapabilities().maxPendingWindowFrames` 给出帧槽容量。Vulkan Presenter 提供两个槽，无窗口设备为 0。应用仍按获取、提交、呈现的顺序处理一个活动帧。已呈现入队的帧可以留在队列中，应用可以获取下一帧。

在上例中替换最后两行即可按能力选择模式：

```cpp
const auto mode = device.GetCapabilities().maxPendingWindowFrames > 0
                      ? voryx::SubmitMode::Async
                      : voryx::SubmitMode::Synchronous;
const auto submitted = Take(device.Submit({lists, frame.token, mode}));
Take(presenter->Present(frame.token, submitted));
```

异步提交成功表示 GPU 队列已接受工作。可以直接调用 Present，把等待渲染完成的依赖交给 Vulkan。异步 Present 成功表示呈现请求已入队。要重置命令列表或从主机访问该帧使用的资源，先用提交令牌确认 GPU 执行完成。Presenter 用独立的呈现 fence 确认原生呈现资源可以复用。 [提交令牌与资源寿命](rhi-async-submit.md) · [Vulkan 呈现 fence](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainPresentFenceInfoKHR.html)

应用可以轮换两套命令列表和每帧更新的缓冲，也可以在提交成功后销毁命令包装，下帧重新创建。实际同时在途的帧数受可用交换链图像限制。切换尺寸或绘制入口，以及析构 Presenter 时，现有路径会等待相关旧帧完成。退出时可使用下文的显式限时关闭。

## 窗口图像和帧状态

| 项目 | 规则 |
| --- | --- |
| GetDevice | 借用与 Presenter 共用同一 Vulkan 设备的 IDevice。设备由 Presenter 销毁，命令应先于 Presenter 析构。 |
| AcquireFrame | 同一 Presenter 只允许一个活动帧。默认等待 50 毫秒，先等帧槽再用剩余时间获取图像。0 用于查询可用性，暂不可用返回 NotReady。有限等待可能返回 Timeout，耗尽预算后的最后一次查询也可能返回 NotReady。取得图像后仍需完成原有获取同步。拒绝无穷获取等待。 |
| FrameInfo | color 为借用图像，只可用于该帧的颜色附件。format 以实际取得的 colorFormat 为准，取值为 BGRA8_UNorm 或 RGBA8_UNorm。 |
| 图像内容 | Acquire 后逻辑状态为 Present。首次或再次获取时内容均视为未定义，当前 Pass 清除全附件。 |
| Submit | 必须包含引用帧图像的 Pass，最终用途必须为 Present。每帧仅一次带令牌的成功提交。图像转回 Present 后，本批内禁止再次引用该颜色图像。离屏列表仍可独立提交。 |
| Present | 校验帧令牌、该帧返回的提交令牌和录制引用。呈现请求入队后消费帧，原令牌与借用图像失效。原生入队前内存不足时可以重试同一帧。 |
| CancelFrame | 取消尚未成功提交的帧。先 Reset 或析构引用帧的命令，再取消。成功后原图像和令牌立即失效。 |

借用颜色图像不能直接 Destroy、ReadTexture、采样或用于其他设备。关闭 Presenter 会释放交换链，未消费的图像到此结束。同步 Submit 成功时 GPU 命令已经完成，Present 使用交换链呈现 fence 回收其关联资源。该 fence 表示资源可回收，不提供屏幕扫描时间。 [Vulkan 窗口集成](https://docs.vulkan.org/spec/latest/chapters/VK_KHR_surface/wsi.html) · [呈现 fence](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainPresentFenceInfoEXT.html)

## 错误与恢复

- 未提交的命令仍引用窗口图像时，CancelFrame 与 Present 返回 ResourceBusy。先释放这些录制引用再重试。
- 提交预校验失败保留命令和活动帧，应用可以重置相关列表后重新录制，或取消此帧。未携带帧令牌的提交不能引用交换链图像。
- 已提交的帧应调用 Present。CancelFrame 返回 InvalidState。同设备的其他提交令牌也不能代替本帧令牌。
- 外部设备令牌返回 WrongDevice。空或失效的帧令牌在呈现和取消时返回 InvalidHandle。提交中的空 FrameToken 表示离屏工作。预校验错误保留活动帧。原生呈现或取消发生致命错误时回收本次帧，丢失设备需重建 Presenter。
- 窗口无可用尺寸时获取返回 SurfaceUnavailable。Win32 和 X11 自动查询可见窗口像素尺寸。Wayland 由宿主在 configure 后调用 SetFramebufferExtent，零尺寸暂停。
- 若 Acquire 或 Present 返回 SwapchainOutOfDate，宿主可处理窗口事件并重试 Acquire。特别是 Present 返回该错误时旧帧已经消费，下一轮使用新令牌。
- 窗口所有操作在宿主窗口线程执行。活动设备帧期间 ClearAndPresent 和 DrawAndPresent 返回 InvalidState。两种入口可以在帧之间切换。

默认提交模式仍为同步。异步队列达到设备容量且最早的批次尚未完成时，Submit 返回 ResourceBusy，并保留活动帧和可执行的命令。等待已有提交完成后可重试。DeviceCapabilities 新增字段，宿主升级时应与同版本 SDK 重新编译。 [离屏绘制](rhi-draw-api.md) · [资源绑定](rhi-binding-api.md)

## 显式限时关闭

退出窗口时，先停止录制并释放设备的所有命令列表包装，再在窗口线程调用 `IPresenter::Shutdown(timeoutNs)`。参数单位为纳秒，`0` 仅查询完成状态，`std::numeric_limits<TimeoutNs>::max()` 表示不限时等待。GPU 提交、图像获取和呈现的 fence 使用同一次调用的预算。驱动清理与系统调度没有硬实时保证。

| 返回结果 | 调用方处理 |
| --- | --- |
| 成功 | GPU 和呈现工作已排空，窗口交换链、Vulkan 表面、设备及其资源已释放。可销毁 Presenter 与宿主窗口。再次调用成功。 |
| ResourceBusy | 仍持有命令列表包装。可以查询或重置列表，释放全部包装后再次调用。 |
| Timeout | 设备仍在关闭中。保留 Presenter 和宿主窗口，处理应用事件后用新的预算重试。 |
| DeviceLost | 关闭期间原生等待确认设备丢失，相关资源已回收。需要继续渲染时重新创建 Presenter。 |
| WrongThread | 在创建 Presenter 的窗口线程调用。此次调用不改变 Presenter 状态。 |

首次有效调用进入关闭中，窗口获取、呈现、取消和设备创建、读写、录制、提交会返回 `InvalidState`。设备健康时，关闭中保留令牌查询与等待、资源销毁和命令列表重置。所有命令列表包装释放后，引擎会处理仍未消费的活动帧。已提交但未呈现的帧在 GPU 完成后随交换链释放。

关闭成功或报告上述 DeviceLost 后，原有帧和提交令牌失效，借用设备只保留 `GetInfo` 和 `GetCapabilities` 的缓存结果，直到 Presenter 析构。未显式关闭时，析构仍会排空在途工作，等待时间不设上限。升级时需用同版本 SDK 重新编译应用。 [功能规格](features/window-shutdown.md) · [Vulkan 交换链销毁契约](https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroySwapchainKHR.html)

## 验证

Windows 可以在生成的 Visual Studio 工程中将 voryx_win32_clear 设为启动项目，并传入 `--verify`。自动化入口用相应的 Vulkan Debug 预设执行。Linux 的软件验收配置分别使用 X11 和原生 Wayland 环境。 [复现命令](environment.md)

三个窗口样例的 --verify 会在原有绘制之外，验证借用图像的生命周期、取消后继续获取、无令牌与错误令牌提交、纹理索引绘制和窗口暂停恢复，并验证两次异步窗口重建和应用主动关闭。迁移安装后的窗口程序执行同一套公开 API。 [功能规格](features/window-frame.md) · [环境报告](environment.md)

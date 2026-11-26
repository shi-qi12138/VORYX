# Win32 Vulkan 窗口清屏

本阶段提供可嵌入宿主程序的窗口呈现入口 `voryx::CreatePresenter`。Win32 示例使用该入口创建交换链，在同一个宿主窗口中清屏、缩放并恢复呈现。它对应 F05/F12 的首批窗口功能。

## 构建与运行

Windows x64 配置启用 Vulkan 时默认启用 `VORYX_ENABLE_WIN32_WSI`。构建要求 Vulkan SDK 头文件 1.3.280 或更新版本。设备仍请求 Vulkan 1.2，窗口路径要求同一队列支持图形和呈现，驱动提供 `VK_KHR_swapchain` 和 `VK_EXT_swapchain_maintenance1` 的呈现 fence，加载器提供 Win32 surface、`VK_KHR_get_surface_capabilities2` 和 `VK_EXT_surface_maintenance1`。若只用缓冲 API，可在配置时设 `VORYX_ENABLE_WIN32_WSI=OFF`，继续使用原有的 Vulkan 1.2 开发包基线。

```powershell
python tools/environment/build.py --preset windows-vulkan-debug
.\build\windows-vulkan-debug\samples\win32_clear\voryx_win32_clear.exe
```

示例会显示蓝色客户区。可以拖动边框缩放，点击最小化再恢复，关闭窗口退出。`--verify` 执行有限帧数的自动化流程，启用 Khronos 验证和同步验证，遇到验证错误或流程失败时返回非零。Windows 测试使用项目指定的隐式层搜索目录，相关环境说明见 [Vulkan 后端](vulkan-backend.md)。

安装 SDK 后，复制 `share/Voryx/examples/win32_clear` 到外部工程。用 `CMAKE_PREFIX_PATH` 指向安装目录，配置并编译 `win32_clear`。接入工程链接 `Voryx::runtime`，示例创建 Win32 窗口因此也链接 `user32`。安装包通过 `Voryx_WITH_WIN32_WSI` 报告是否包含窗口支持。见 [SDK 接入](sdk-integration.md)。

## 调用契约

公共工厂位于 `<voryx/runtime/presenter.hpp>`。在窗口线程持有有效的 `HWND` 和该窗口的 `HINSTANCE`，将它们转换成 `std::uintptr_t` 后填写 `PresenterDesc`。

```cpp
voryx::PresenterDesc desc{};
desc.window = voryx::NativeWindowDesc::ForWindows(
    reinterpret_cast<std::uintptr_t>(hwnd),
    reinterpret_cast<std::uintptr_t>(instance));
desc.device.backend = voryx::BackendType::Vulkan;
auto created = voryx::CreatePresenter(desc);
```

检查 `created`，成功后移出 `std::unique_ptr<voryx::IPresenter>`。每轮先分发宿主窗口消息，再调用 `ClearAndPresent({r, g, b, a})`。颜色是 `[0, 1]` 范围内的有限浮点数。`ClearColor` 默认黑色，alpha 默认为 1。当前交换链选择 BGRA8/RGBA8 UNORM 和 sRGB 非线性色彩空间，数值直接写入 UNORM。全流程示例包含失败判断和退出顺序。见 [示例源码](../samples/win32_clear/main.cpp)。

`PresenterDesc::device` 使用已有的 `DeviceDesc`。`Auto` 选择当前的窗口后端 Vulkan。`enableValidation`、`allowSoftwareAdapter` 和日志回调沿用 Vulkan 的设备选项。日志上下文须活到 Presenter 析构完成。回调须可并发调用，禁止在回调中重入引擎。此入口拥有独立的 Vulkan 上下文，窗口始终归宿主管理。宿主须在同一线程创建、调用和销毁 Presenter，并在销毁窗口前释放 Presenter。同一 HWND 同时只对应一个 Presenter。销毁线程错误会终止进程。

| 返回结果 | 宿主处理 |
| --- | --- |
| 成功 | 继续分发消息和呈现下一帧 |
| `SurfaceUnavailable` | 处理窗口消息，等恢复后重试。最小化或零客户区时会暂停 |
| `SwapchainOutOfDate` | 下次调用时重建交换链。子最优帧可能已呈现 |
| `Timeout` | 获取图像最多等待 50 ms，超时后可重试 |
| `InvalidArgument` / `WrongThread` | 修正参数或回到创建线程再调用 |
| `BackendNotBuilt` / `BackendUnavailable` / `Unsupported` | 检查包选项、驱动能力或后端选择 |
| `SurfaceIncompatible` / `DeviceLost` / `InvalidState` | 释放 Presenter，检查窗口或重建设备 |

创建阶段校验 HWND 归属进程、线程和实例。非 Windows 构建收到有效的 Win32 描述时返回 `SurfaceIncompatible`。驱动错误保留原生 `VkResult`。其他原生帧错误可能停用 Presenter，此后返回 `InvalidState`。若收到内存不足，应避免紧循环并检查对象状态。

## 执行与验收范围

首批实现是单队列、同步 FIFO 呈现。每帧获取图像后录制传输清屏和布局转换，用信号量连接清屏提交与呈现，再等待提交 fence 和呈现 fence。成功意味着呈现请求已提交且同步对象可安全复用。屏幕实际扫描时机由系统决定。提交和呈现完成等待可能阻塞宿主线程，50 ms 限制只用于图像获取。客户区尺寸变化时重建交换链。最小化或零尺寸时释放旧交换链，恢复后按需创建。

自动验收覆盖成功呈现多帧、无效颜色、错误线程、缩放、最小化、恢复、销毁及迁移安装包的独立运行。颜色通路使用原生 `vkCmdClearColorImage`，自动用例检查呈现结果与验证层诊断。像素比较、驱动设备丢失注入、Linux X11/Wayland 窗口、三角形与图形管线属于后续工作。呈现 fence 的对象寿命规则见 [Khronos Vulkan 规范](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainPresentFenceInfoKHR.html)。

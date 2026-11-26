# X11 Vulkan 窗口清屏

VORYX 可接入外部 Linux C++ 工程已经创建的 Xlib 窗口。宿主负责连接、窗口和事件循环。SDK 负责 Vulkan 表面、交换链、清屏和呈现。与 Win32 共用 `CreatePresenter` 和 `IPresenter::ClearAndPresent`。

## 构建与运行

Linux 配置启用 `VORYX_ENABLE_VULKAN` 和 `VORYX_ENABLE_X11`。需要可访问的 X11 显示会话、Xlib 开发包（Ubuntu 为 `libx11-dev`）、Vulkan 开发包 1.3.280 或更新版本。运行需兼容 Vulkan 1.2 的驱动，以及 `VK_KHR_xlib_surface` 和共享呈现路径要求的呈现 fence 支持。验证测试还需 Khronos 验证层。

在原生 Linux 硬件环境运行：

```sh
python3 tools/environment/build.py --preset linux-vulkan-x11-debug
./build/linux-vulkan-x11-debug/samples/xlib_clear/voryx_xlib_clear
```

在只有软件驱动的 WSLg 或其他 X11 环境，显式允许软件设备：

```sh
python3 tools/environment/build.py --preset linux-vulkan-x11-software-debug
./build/linux-vulkan-x11-software-debug/samples/xlib_clear/voryx_xlib_clear --allow-software
```

示例显示蓝色客户区。正常运行时可用窗口管理器缩放、最小化、恢复和关闭。`--verify` 在真实 X11 显示中直接控制窗口映射，检查多帧呈现、尺寸变化、取消映射后暂停以及重新映射后恢复。它会启用 Khronos 同步验证，缺少可用显示、驱动或验证层时以非零退出。见 [环境记录](environment.md)。

## 宿主接口

在 Xlib 的任何其他调用之前执行 `XInitThreads()` 并检查成功，再打开连接和创建窗口。宿主须传入来自同一有效 Xlib 连接的真实 `Display*` 和窗口 ID。保留两者直到 Presenter 释放，随后销毁窗口并关闭连接。VORYX 的头文件放在 Xlib 头文件之前，以免 Xlib 的 `Success`、`None` 等宏干扰 C++ API 名称。完整的窗口和事件循环写法见 [示例源码](../samples/xlib_clear/main.cpp)。

```cpp
voryx::PresenterDesc desc{};
desc.device.backend = voryx::BackendType::Vulkan;
desc.window = voryx::NativeWindowDesc::ForXlib(
    reinterpret_cast<std::uintptr_t>(display), window);
auto created = voryx::CreatePresenter(desc);
```

检查返回结果，在同一宿主线程分发窗口事件并调用 `ClearAndPresent`。Xlib 连接和窗口借用自宿主。悬空地址、过期窗口和断开的连接违反前置条件，可能触发应用配置的 Xlib 错误处理器。X11 适配器读取客户区像素尺寸，并在窗口不可见映射（`map_state != IsViewable`）时暂停。暂停、交换链过期、超时及其他结果处理沿用共享契约。见 [呈现调用契约](win32-clear.md#调用契约)。

启用 X11 的安装包会报告 `Voryx_WITH_X11`，并从 `find_package(Voryx)` 恢复静态链接所需的 Xlib 依赖。复制 `share/Voryx/examples/xlib_clear`，设置 `CMAKE_PREFIX_PATH` 指向 SDK 根目录，再构建 `xlib_clear`。用户只用到公共头和 `Voryx::runtime`。示例因为还自行调用 Xlib，显式链接 `X11::X11`。应用和 SDK 应使用同一次构建提供的公共头与静态库。见 [SDK 接入](sdk-integration.md)。

X11 使用相同的单队列、同步 FIFO 与提交、呈现 fence 路径。Xlib 表面、显示连接与线程使用要求见 [Khronos WSI 规范](https://docs.vulkan.org/spec/latest/chapters/VK_KHR_surface/wsi.html#_xlib_platform)。

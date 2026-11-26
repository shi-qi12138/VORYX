# 原生 Wayland Vulkan 窗口清屏

VORYX 可在宿主创建的 `wl_surface` 中清屏并呈现。宿主拥有 Wayland 连接、表面、窗口角色和事件循环。SDK 使用 `VK_KHR_wayland_surface` 和共享 Vulkan 交换链。此路径使用 Wayland socket，可在没有 X11 `DISPLAY` 的环境运行。

## 构建和运行

Linux 需启用 `VORYX_ENABLE_VULKAN` 和 `VORYX_ENABLE_WAYLAND`。库构建需要 Vulkan 开发包 1.3.280 或更新版本。运行需要可用的 Wayland compositor、兼容 Vulkan 1.2 的驱动及呈现 fence 支持。示例另外需要 C 编译器、`pkg-config`、`libwayland-dev`、`wayland-scanner` 和 `wayland-protocols`。它从稳定 xdg-shell XML 生成协议代码。只编译 SDK 时可关闭 `VORYX_BUILD_SAMPLES`，Wayland 适配器仅使用 Vulkan SDK。

在有硬件 Vulkan ICD 的 Wayland 会话中执行：

```sh
python3 tools/environment/build.py --preset linux-vulkan-wayland-debug
./build/linux-vulkan-wayland-debug/samples/wayland_clear/voryx_wayland_clear
```

在 WSLg 软件驱动环境中显式允许软件设备：

```sh
env -u DISPLAY python3 tools/environment/build.py --preset linux-vulkan-wayland-software-debug
./build/linux-vulkan-wayland-software-debug/samples/wayland_clear/voryx_wayland_clear --allow-software
```

样例打开一个蓝色的 xdg-shell 顶层窗口。`--verify` 开启 Khronos 同步验证，并检查首次配置、多帧呈现、最大化引发的尺寸变化、显式暂停和恢复。此验收需要 compositor 接受最大化并下发不同的正尺寸。没有可用会话、驱动或验证层时命令失败。

## 宿主调用契约

1. 宿主创建 `wl_surface` 和所需角色。xdg-shell 窗口先提交无缓冲表面，等待首次 `xdg_surface.configure` 并 `ack_configure`。
2. 使用同一连接的 `wl_display*` 和已配置 `wl_surface*` 调用 `CreatePresenter`。保持这些对象及其角色到 Presenter 释放之后。
3. 宿主继续分发窗口事件。在处理并确认配置后，调用 `SetFramebufferExtent({width, height})` 提供**实际缓冲像素尺寸**。逻辑尺寸、整数或分数缩放均由宿主转换。示例使用缩放 1。
4. 调用 `ClearAndPresent(color)`。当尺寸发生变化时，下次呈现按新尺寸重建交换链。任一维为零表示宿主请求暂停，呈现返回 `SurfaceUnavailable`。重新设置正尺寸即可恢复。
5. 在创建线程先销毁 Presenter，再销毁角色和表面，最后断开连接。

```cpp
voryx::PresenterDesc desc{};
desc.device.backend = voryx::BackendType::Vulkan;
desc.window = voryx::NativeWindowDesc::ForWayland(
    reinterpret_cast<std::uintptr_t>(display),
    reinterpret_cast<std::uintptr_t>(surface));
auto created = voryx::CreatePresenter(desc);
```

成功后从结果移出 Presenter。`SetFramebufferExtent` 只保存期望尺寸。`ClearAndPresent` 负责重建并同步呈现。还未提供正尺寸时，Presenter 处于暂停状态。调用前需确认上一轮配置，宿主在 Presenter 使用表面期间避免自行附加或提交其他缓冲，也不要并行操作同一表面。Win32 与 X11 仍自动查询客户区，调用 `SetFramebufferExtent` 返回 `Unsupported`。关闭 Wayland 构建的 Linux SDK 返回 `BackendNotBuilt`，其他平台收到 Wayland 描述返回 `SurfaceIncompatible`。空指针返回 `InvalidArgument`。

Wayland 的暂停验收使用宿主显式尺寸信号。合成器隐藏或最小化通知、分数缩放和多屏变更需要宿主另行处理。本阶段验收覆盖单窗口、同步 FIFO 和缩放 1。其余错误和恢复沿用共享呈现契约。 [呈现调用契约](win32-clear.md#调用契约) · [Khronos WSI 规范](https://docs.vulkan.org/spec/latest/chapters/VK_KHR_surface/wsi.html#_wayland_platform) · [xdg-shell 协议](https://gitlab.freedesktop.org/wayland/wayland-protocols/-/blob/main/stable/xdg-shell/xdg-shell.xml)

## 安装包接入

启用 Wayland 的包报告 `Voryx_WITH_WAYLAND`，并附带 `share/Voryx/examples/wayland_clear`。复制示例到独立工程，将 `CMAKE_PREFIX_PATH` 指向已安装 SDK，即可通过 `find_package(Voryx)` 和 `Voryx::runtime` 构建。示例因创建窗口而自行链接 Wayland 客户端。更新 SDK 后，请用同一版头文件重编译宿主。 [示例源码](../samples/wayland_clear/main.cpp) · [SDK 安装指南](sdk-integration.md)

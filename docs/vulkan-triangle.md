# Vulkan 彩色三角形

SDK 现可通过 `IPresenter` 绘制非索引彩色三角形列表。窗口沿用 `CreatePresenter`，Win32、X11 和 Wayland 共享顶点缓冲、图形管线和绘制路径。窗口和事件循环仍归宿主所有。

## 运行示例

在 Visual Studio 中，将 `voryx_win32_clear` 设为启动项目，在项目的调试命令参数中填入 `--triangle`，然后按 F5。这个目标需要在 CMake 中开启 `VORYX_ENABLE_VULKAN` 和 Win32 WSI。也可从项目根目录运行：

```powershell
python tools/environment/build.py --preset windows-vulkan-debug
.\build\windows-vulkan-debug\samples\win32_clear\voryx_win32_clear.exe --triangle
```

Linux 的 `voryx_xlib_clear` 和 `voryx_wayland_clear` 接受同一参数。只有软件驱动时再传 `--allow-software`。各平台的构建和窗口接入步骤见 [Win32](win32-clear.md) · [X11](xlib-clear.md) · [Wayland](wayland-clear.md)。

## 公共 C++ API

在宿主线程创建设备和 Presenter 后，准备一组顶点和两个已编译的 Shader：

```cpp
const std::array vertices{
    voryx::ColorVertex{0.0f, -0.7f, 1.0f, 0.0f, 0.0f},
    voryx::ColorVertex{0.75f, 0.6f, 0.0f, 1.0f, 0.0f},
    voryx::ColorVertex{-0.75f, 0.6f, 0.0f, 0.0f, 1.0f},
};
const auto setup = presenter->SetTriangleList({vertices, vertexSpirv, fragmentSpirv});
if (!setup) {
    return 1;
}
// 宿主分发窗口事件后提交一帧。Wayland 先报告当前缓冲像素尺寸。
const auto frame = presenter->DrawAndPresent({0.15f, 0.35f, 0.8f, 1.0f});
```

`vertexSpirv` 和 `fragmentSpirv` 是有效 Vulkan 1.2 SPIR-V 模块的 `std::span<const std::uint32_t>`。两阶段入口名均为 `main`。顶点 Shader 的位置 0 为 `vec2` 坐标，位置 1 为 `vec3` 颜色。片元 Shader 向位置 0 输出 `vec4`。两阶段的中间变量必须匹配。当前管线布局为空，Shader 只能使用这一输入布局，不能声明依赖绑定的资源或推送常量。官方 Shader 模块输入要求见 [Vulkan 规范](https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateShaderModule.html)。

`SetTriangleList` 接收正数且为 3 的倍数的顶点，最多 300 万顶点。坐标须为有限数，RGB 须在 `[0, 1]`。默认样例 Shader 直接将坐标写入裁剪空间。SPIR-V 大小和头部先检查，模块语义须由可信离线编译工具和开发期验证层确认。调用成功后，顶点已复制到 SDK 管理的映射缓冲，Shader 模块已用于创建管线，调用方可释放输入。参数错误或可恢复的创建失败保留原来有效的三角形。设备或表面丢失时需释放 Presenter 并按窗口契约重建。

`DrawAndPresent` 在一帧中清屏、绑定管线和顶点、执行 `vkCmdDraw` 并呈现。未配置三角形时返回 `InvalidState`。`ClearAndPresent` 仍可单独清屏。缩放重建图像视图与帧缓冲，并更新 viewport/scissor，已有顶点与管线继续复用。交换链格式和图像用途由窗口支持决定，当前绘制路径要求 UNORM 颜色附件与传输目标用途。暂停和过期结果沿用各窗口的清屏契约。

## Shader 资产与安装 SDK

项目附带 GLSL 源码及供示例和测试共用的 SPIR-V 夹具。修改 Shader 后，从项目根目录执行：

```text
python tools/shader_compiler/build_triangle.py --glslc glslc
```

工具将两个阶段编译到 Vulkan 1.2 目标，并更新 `assets/shaders/generated/triangle_spirv.hpp`。SDK 的窗口示例附带该夹具，独立编译样例无需再安装 Shader 编译器。实际接入示例见 [公共 API 调用](../samples/common/triangle_sample.hpp) · [SDK 接入](sdk-integration.md)。

这一阶段只有固定布局的非索引彩色三角形列表。单张 RGBA8 纹理采样有独立的公共入口。彩色深度测试已有单独入口。通用 Shader 反射、独立管线资源仍待实现。固定彩色索引绘制已有独立入口。 [深度指南](vulkan-depth.md) · [索引指南](vulkan-indexed.md) [纹理指南](vulkan-texture.md)窗口测试验证真实呈现和恢复。离屏像素测试使用同一 Vulkan 管线与绘制命令，读回并分别检查三角形内部与背景。

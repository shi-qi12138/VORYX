# Vulkan 索引绘制

VORYX 可以让多个三角形引用同一组顶点。四个顶点配合六个索引即可组成一个矩形。Win32、X11 和 Wayland 窗口使用同一 `IPresenter` 接口和 Vulkan 管线。

## 运行示例

在 Visual Studio 中将 `voryx_win32_clear` 设为启动项目，调试参数填 `--indexed`，按 F5。需要在 CMake 中启用 `VORYX_ENABLE_VULKAN`。Linux 的 `voryx_xlib_clear` 和 `voryx_wayland_clear` 同样接受 `--indexed`，软件驱动再加 `--allow-software`。示例应显示由两个三角形组成的彩色矩形。 [构建与启动步骤](vulkan-triangle.md)

## C++ 接口

在宿主窗口线程创建 Presenter，然后配置顶点、索引和 Shader。

```cpp
const std::array vertices{
    voryx::ColorVertex{-0.75f, -0.65f, 1.0f, 0.0f, 0.0f},
    voryx::ColorVertex{ 0.75f, -0.65f, 0.0f, 1.0f, 0.0f},
    voryx::ColorVertex{-0.75f,  0.65f, 0.0f, 0.0f, 1.0f},
    voryx::ColorVertex{ 0.75f,  0.65f, 1.0f, 1.0f, 0.0f},
};
const std::array<std::uint32_t, 6> indices{0, 1, 2, 2, 1, 3};
const auto setup = presenter->SetIndexedTriangleList({
    vertices, indices, vertexSpirv, fragmentSpirv});
if (!setup) {
    return 1;
}
const auto frame = presenter->DrawAndPresent({0.15f, 0.35f, 0.8f, 1.0f});
```

- `IndexedTriangleListDesc::vertices` 使用 `ColorVertex`。至少一个顶点，最多 300 万，坐标须有限且 RGB 在 `[0, 1]`。
- `indices` 采用 `std::uint32_t`，从零开始。索引数须为正且是 3 的倍数，最多 300 万。每个索引须小于顶点数，可重复引用同一顶点。顶点数无需为 3 的倍数。
- Shader 布局与彩色三角形一致。两个阶段入口为 `main`，顶点位置 0 是 `vec2` 坐标，位置 1 是 `vec3` RGB。示例与安装包复用同一组 SPIR-V。
- `SetIndexedTriangleList` 在分配 Vulkan 资源前检查索引。越界或非法数量返回 `InvalidArgument`，保留前一组有效绘制。配置成功后，调用方可以释放顶点、索引和 Shader 输入。
- SDK 用原生索引缓冲执行 `vkCmdDrawIndexed`。缩放和暂停恢复后继续复用顶点与索引。当前索引入口采用固定彩色布局，纹理与深度组合及索引子范围由后续通用绘制接口提供。 [Vulkan 索引命令规范](https://docs.vulkan.org/spec/latest/chapters/drawing.html)

升级后用同一版头文件和静态库重新编译宿主。三个窗口示例随安装包提供。自动验证读回两个三角形内部、共享边与矩形外的像素，并检查拒绝无效索引后仍可呈现。 [SDK 接入](sdk-integration.md) · [示例源码](../samples/common/triangle_sample.hpp)

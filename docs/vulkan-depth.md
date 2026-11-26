# Vulkan 深度测试

VORYX 可在同一窗口帧中绘制有深度的彩色三角形。宿主提供顶点与 Shader，SDK 在交换链对应尺寸下创建深度附件并执行测试。较近的片元会遮挡远处片元，即使近处三角形排在顶点列表前面。

## 运行示例

在 Visual Studio 中将 `voryx_win32_clear` 设为启动项目，调试命令参数填入 `--depth`，按 F5。该目标需要 CMake 启用 Vulkan 和 Win32 WSI。Linux 的 X11 与 Wayland 窗口示例接受同一参数。软件驱动再传 `--allow-software`。示例绘制部分重叠的红、绿两个三角形，重叠区域为近处的红色。 [窗口示例与构建](vulkan-triangle.md)

## 调用契约

通过 `CreatePresenter` 接入宿主窗口。在创建线程调用 `SetDepthTriangleList`，然后用 `DrawAndPresent` 逐帧绘制。

```cpp
const auto setup = presenter->SetDepthTriangleList({
    vertices, depthVertexSpirv, colorFragmentSpirv});
if (!setup) {
    return 1;
}
const auto frame = presenter->DrawAndPresent({0.15f, 0.35f, 0.8f, 1.0f});
```

| 项目 | 契约 |
| --- | --- |
| `DepthVertex` | 依次提供 `x, y, z, red, green, blue`。x/y 有限，z 与 RGB 在 `[0, 1]`。 |
| 顶点列表 | 正数且为 3 的倍数，最多 300 万。沿用彩色非索引列表规则。 |
| Shader | 有效 Vulkan 1.2 SPIR-V，两个入口都叫 `main`。顶点位置 0 是 `vec3` 坐标，位置 1 是 `vec3` 颜色。片元位置 0 输出 `vec4`。 |
| 深度规则 | 深度清为 1.0。启用深度读写和严格小于测试。深度等于当前值时保留已有片元，等于清除值 1.0 的片元不会通过。 |
| 坐标约定 | 样例直接将输入 xyz 写入 `gl_Position`，w 为 1。自定义 Shader 仍须遵循 Vulkan 的裁剪空间约定。 |

设置成功后输入可释放。无效 z 或描述返回 `InvalidArgument` 并保留上一组绘制配置。深度格式优先 `D32_SFLOAT`，其次 `D16_UNORM`。每个同步 Presenter 的交换链共享一个深度附件，上一帧完成后才复用。缩放或恢复时按实际像素尺寸重建附件。Clear、彩色、纹理与深度配置可以替换，当前深度模式面向彩色三角形。纹理与深度的组合、可配置比较和模板测试属于后续管线扩展。 [Vulkan 深度约定](https://docs.vulkan.org/guide/latest/depth.html)

SDK 的独立窗口示例包含深度顶点 Shader 及编译产物。更新 GLSL 后运行 `python tools/shader_compiler/build_triangle.py --glslc glslc`。升级后用同一版头文件与静态库重新编译应用。 [深度 Shader](../assets/shaders/depth.vert) · [公共 API 示例](../samples/common/triangle_sample.hpp)

离屏 GPU 用例分别执行先远后近和先近后远。它检查重叠区域、两侧各自可见的区域与背景的实际像素。窗口验收同时覆盖非法深度、窗口缩放、暂停恢复和安装包独立接入。

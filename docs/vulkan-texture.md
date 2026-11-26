# Vulkan RGBA8 纹理采样

外部工程可以把一张 RGBA8 图像贴到非索引三角形上。三种窗口路径使用同一 `IPresenter` 接口。彩色三角形和清屏仍使用原有调用。

## 运行窗口示例

在 Visual Studio 中选择 `voryx_win32_clear` 作为启动项目，在调试参数中填 `--texture` 并按 F5。需要先通过 CMake 启用 Vulkan。Linux 的 `voryx_xlib_clear` 与 `voryx_wayland_clear` 接受同一参数。软件 Vulkan 另加 `--allow-software`。示例显示一个由两个三角形组成的矩形，四个象限分别使用不同颜色。 [窗口构建步骤](vulkan-triangle.md)

## 公共 API

先按各平台规则创建 `IPresenter`。在宿主线程提供带 UV 的顶点、RGBA8 像素和有效的 Vulkan 1.2 SPIR-V 模块。

```cpp
voryx::TexturedTriangleListDesc desc{};
desc.vertices = vertices;
desc.texture = {{width, height}, rowPitchBytes, std::as_bytes(std::span(pixels))};
desc.vertexSpirv = vertexSpirv;
desc.fragmentSpirv = fragmentSpirv;
const auto setup = presenter->SetTexturedTriangleList(desc);
if (!setup) {
    return 1;
}
const auto frame = presenter->DrawAndPresent({0.15f, 0.35f, 0.8f, 1.0f});
```

| 字段或行为 | 契约 |
| --- | --- |
| `TexturedVertex` | 有限浮点数 `x, y, u, v`。顶点数为正且是 3 的倍数，最多 300 万。 |
| `Rgba8TextureDesc::extent` | 宽高均在 1 到 4096 像素之间，并满足设备限制。单层、单 mip、线性 RGBA8 UNORM。 |
| `rowPitchBytes` | `0` 表示 `width * 4`。显式行距至少为 `width * 4`，可含任意字节的行末填充。 |
| `pixels` | 左上开始，行内从左到右，行间从上到下。每像素四个字节按 R、G、B、A 顺序。数据至少覆盖 `(height - 1) * pitch + width * 4`，最后一行尾部填充可省略。 |
| 顶点 Shader | `main`。位置 0 为 `vec2` 坐标，位置 1 为 `vec2` UV。 |
| 片元 Shader | `main`。`set = 0, binding = 0` 为组合 `sampler2D`，颜色写入位置 0。 |
| 采样 | 最近邻，U/V 边缘钳位。UV `(0, 0)` 对应首行首列。管线无混合，纹理不做 sRGB 转换。 |

`SetTexturedTriangleList` 返回成功后，顶点与像素已经上传，Shader 已参与管线创建，调用者可释放全部输入。当前 Presenter 持有一组绘制配置，可以在彩色与纹理模式之间替换。无效行距或截断数据返回 `InvalidArgument`，保留先前有效的绘制配置。设备或表面丢失时按窗口契约重建。成功的窗口缩放和暂停恢复会继续使用原纹理。

本页的快捷路径随绘制配置创建一张纹理和一个采样器。独立句柄、可配置滤镜和复用方式见绑定指南。纹理可与常量缓冲一起绑定。纹理更新、多 mip、纹理与深度或索引的组合仍待实现。PNG 或 JPEG 解码由应用负责。 [绑定指南](vulkan-bindings.md) · [显式布局](vulkan-binding-layout.md)

## Shader 与验收

更新 GLSL 后运行 `python tools/shader_compiler/build_triangle.py --glslc glslc` 生成 SDK 示例共用的 SPIR-V。三个窗口示例和安装包都包含纹理模式，无需在运行时调用 Shader 编译器。 [GLSL 片元 Shader](../assets/shaders/textured.frag) · [示例 API 调用](../samples/common/triangle_sample.hpp)

离屏用例使用每行 11 字节的 2×2 测试图，最后一行只提供实际像素。它通过同一 Vulkan 绘制管线，断言四个象限的 RGBA 字节精确匹配，覆盖非整像素行距、UV 方向、通道顺序和 alpha。窗口用例另行验证无效替换后的呈现、缩放恢复与迁移安装后的独立运行。图像布局和上传同步参考 [Vulkan 纹理上传指南](https://docs.vulkan.org/tutorial/latest/06_Texture_mapping/00_Images.html)。

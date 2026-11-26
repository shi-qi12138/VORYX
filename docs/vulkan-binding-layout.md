# Vulkan 显式资源绑定

Win32、X11 和原生 Wayland 的 Vulkan Presenter 可以让一次绘制同时读取常量缓冲、纹理和采样器。使用 `<voryx/rhi/binding.hpp>` 声明资源布局和绑定，用 `<voryx/rhi/presenter.hpp>` 配置绘制。外部应用链接 `Voryx::runtime` 即可。 [安装与接入](sdk-integration.md)

## 运行窗口样例

在 Visual Studio 中把 `voryx_win32_clear` 设为启动项目，在调试命令参数填入 `--resources` 后按 F5。X11 的 `voryx_xlib_clear` 和 Wayland 的 `voryx_wayland_clear` 接受相同参数，软件 Vulkan 驱动再加 `--allow-software`。屏幕上会出现一个向右偏移、缩小的四色矩形。几何变换来自常量缓冲，颜色来自片元 Shader 对同一绑定集内纹理的采样。 [窗口构建](vulkan-triangle.md) · [样例源码](../samples/common/triangle_sample.hpp)

## 接入顺序

以下片段在已创建 Presenter 的线程上执行。假设应用已准备好 `Rgba8TextureDesc image`、`TexturedVertex` 顶点数组 `vertices` 和与下表匹配的 SPIR-V。返回错误时可读取 `GetError().errorCode` 与 `GetError().operation` 定位失败操作。示例中的提前返回交由 Presenter 析构清理。

```cpp
using namespace voryx;

constexpr std::array transform{0.5f, 0.5f, 0.35f, 0.0f};
auto uniform = presenter->CreateUniformBuffer(std::as_bytes(std::span(transform)));
auto texture = presenter->CreateTexture2D(image);
auto sampler = presenter->CreateSampler({TextureFilter::Nearest, TextureFilter::Nearest});
if (!uniform || !texture || !sampler) {
    return 1;
}

constexpr std::array slots{
    BindingLayoutEntry{3, BindingType::UniformBuffer, BindingVisibility::Vertex},
    BindingLayoutEntry{7, BindingType::SampledTexture, BindingVisibility::Fragment},
    BindingLayoutEntry{11, BindingType::Sampler, BindingVisibility::Fragment},
};
auto layout = presenter->CreateBindingLayout({slots});
if (!layout) {
    return 1;
}
const std::array entries{
    BindingSetEntry{11, sampler.Value()},
    BindingSetEntry{3, uniform.Value()},
    BindingSetEntry{7, texture.Value()},
};
auto bindings = presenter->CreateBindingSet({layout.Value(), entries});
if (!bindings) {
    return 1;
}

auto configured = presenter->SetResourceTriangleList({
    vertices, bindings.Value(), vertexSpirv, fragmentSpirv});
if (!configured) {
    return 1;
}
auto frame = presenter->DrawAndPresent({0.15f, 0.35f, 0.8f, 1.0f});
if (!frame) {
    return 1;
}
```

## 数据与 Shader 契约

| 输入 | 规则 |
| --- | --- |
| `CreateUniformBuffer(bytes)` | 同步复制非空字节，绑定整个缓冲，偏移为 0。上限由 Vulkan 设备的 `maxUniformBufferRange` 决定。创建后内容固定。 |
| `CreateBindingLayout({slots})` | 创建 `set = 0` 的布局。含 1 至 8 个不同槽位，槽号为 0 至 31，允许有间隔。可见阶段为 `Vertex`、`Fragment` 或按位组合。每槽对应一个资源。 |
| `CreateBindingSet({layout, entries})` | 每个布局槽必须恰有一项，顺序任意。`BindingResource` 使用强类型句柄表示常量缓冲、纹理或采样器。绑定集创建后不可改写。 |
| `SetResourceTriangleList(desc)` | 用给定集合和 SPIR-V 配置一组非索引三角形。顶点输入 `location 0` 为 `vec2` 位置，`location 1` 为 `vec2` UV。顶点数为非零的 3 的倍数。 |
| `DrawAndPresent(color)` | 用指定背景色清屏并绘制当前配置，缩放和恢复沿用各窗口的 Presenter 规则。 |

示例顶点 Shader 在 `set 0 / binding 3` 声明 `std140` 块，块中一个 `vec4` 保存水平和垂直缩放、水平和垂直偏移。片元 Shader 在槽 7 声明 `texture2D`，在槽 11 声明 `sampler`，用 `texture(sampler2D(colorImage, colorSampler), uv)` 采样。应用负责确保 Shader 的槽号、资源类型、可见阶段和常量字节布局一致。调用返回后可释放上传源数据、布局和集合的条目数组、顶点和 Shader 数据。 [示例顶点 Shader](../assets/shaders/resource.vert) · [示例片元 Shader](../assets/shaders/resource.frag) · [Vulkan 描述符](https://docs.vulkan.org/spec/latest/chapters/descriptors.html) · [Vulkan 设备限制](https://docs.vulkan.org/spec/latest/chapters/limits.html)

## 错误与释放

- 空常量缓冲、无效布局、槽位缺失或资源类型不匹配返回 `InvalidArgument`。常量缓冲超出设备范围返回 `Unsupported`。
- 空句柄或已销毁的句柄返回 `InvalidHandle`，其他 Presenter 的句柄返回 `WrongDevice`。`IsValid()` 只检查句柄结构。
- 当前绘制持有绑定集，绑定集持有布局和资源。销毁仍被引用的对象返回 `ResourceBusy`。
- 显式释放时先调用 `ResetDraw()` 或替换绘制，再销毁绑定集，最后销毁布局、常量缓冲、纹理和采样器。Presenter 析构会按依赖顺序释放剩余对象。
- SDK 使用静态库。升级后用同版本头文件和库重新编译宿主。

当前范围为一个活动绘制配置、一个绑定集、不可变 RGBA8 纹理和常量数据。Shader 反射、动态更新、动态偏移、多个绑定集及与索引或深度路径的组合待实现。原有固定单槽 `TextureBindingHandle` 接口仍可使用。 [单槽纹理绑定](vulkan-bindings.md) · [功能规格](features/vulkan-binding-layout.md)

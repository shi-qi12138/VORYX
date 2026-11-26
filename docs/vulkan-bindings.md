# 可复用纹理绑定

VORYX 的窗口 Presenter 可以分别创建 RGBA8 纹理、采样器与不可变纹理绑定。两个绘制配置可以引用同一个绑定，或者为同一张纹理创建使用不同采样器的绑定。替换几何时可以复用已上传的像素。Win32、X11 和 Wayland 使用同一套 C++ 接口。

## 运行示例

在 Visual Studio 中将 `voryx_win32_clear` 设为启动项目，调试命令参数填 `--binding`，按 F5。此目标需启用 `VORYX_ENABLE_VULKAN`。Linux 的 X11 和 Wayland 示例也接受这个参数，软件驱动另加 `--allow-software`。示例把同一张四色纹理铺成重复图案，并用同一绑定配置两组几何。 [窗口构建步骤](vulkan-triangle.md)

## C++ 接口

这些操作由创建 Presenter 的线程调用。`image` 是已准备好的 `Rgba8TextureDesc`，像素和行距规则沿用纹理上传接口。`vertices` 为带 UV 的三角形列表。

```cpp
auto texture = presenter->CreateTexture2D(image);
if (!texture) {
    return 1;
}
auto sampler = presenter->CreateSampler({});
if (!sampler) {
    return 1;
}
auto binding = presenter->CreateTextureBinding({texture.Value(), sampler.Value()});
if (!binding) {
    return 1;
}
auto setup = presenter->SetBoundTexturedTriangleList({
    vertices, binding.Value(), vertexSpirv, fragmentSpirv});
if (!setup) {
    return 1;
}
auto frame = presenter->DrawAndPresent({0.15f, 0.35f, 0.8f, 1.0f});
```

| 调用 | 行为 |
| --- | --- |
| `CreateTexture2D` | 创建不可变 RGBA8 图像。成功后宿主可释放像素，宽高、行距与数据长度规则同原纹理路径。 |
| `CreateSampler` | 独立指定 min/mag 的 `Nearest` 或 `Linear`。U/V 分别支持 `ClampToEdge` 或 `Repeat`。默认线性过滤、边缘钳位。 |
| `CreateTextureBinding` | 组合两个同属当前 Presenter 的句柄。绑定在创建后不可变，可被多组绘制配置复用。 |
| `SetBoundTexturedTriangleList` | 用既有绑定配置新的几何和管线。不会重新上传图像或创建采样器。成功后可释放顶点和 Shader 输入。 |
| `ResetDraw` | 清除当前绘制配置，解除对绑定的依赖。之后可调用 `ClearAndPresent`，再次绘制前需配置场景。 |
| `Destroy` | 对纹理、采样器和绑定分别重载。仍有依赖时返回 `ResourceBusy`，成功后该句柄立即失效。 |

句柄强类型且可以复制。`IsValid()` 只检查结构。空句柄或已销毁的句柄返回 `InvalidHandle`，外部 Presenter 的句柄返回 `WrongDevice`。当绘制配置持有绑定时，先 `ResetDraw` 或替换绘制配置，再销毁绑定，最后销毁纹理和采样器。Presenter 析构时会自动清理其资源。 [纹理布局与 Shader](vulkan-texture.md)

片元 Shader 仍使用 `set 0 / binding 0` 的一个 `sampler2D`，坐标与输出沿用纹理路径。线性滤镜能力不满足时返回 `Unsupported`。旧 `SetTexturedTriangleList` 仍按最近邻和边缘钳位运行。当前 Presenter 同时持有一组活动绘制。需要多槽位与常量缓冲时，可使用显式布局路径。 [显式绑定布局](vulkan-binding-layout.md) [Vulkan 描述符规范](https://docs.vulkan.org/spec/latest/chapters/descriptors.html)

## 验收与安装

安装包的窗口示例包含 `--binding`。需要用同一版头文件和静态库重编译宿主。离屏用例用同一张两列纹理和同一个越界 U 坐标分别测试钳位和重复，检查绿色和红色像素。另验证外部句柄、销毁依赖和失效句柄。窗口用例验证 `ResetDraw` 后可复用原绑定，且缩放和暂停恢复可继续呈现。 [SDK 安装](sdk-integration.md) · [示例源码](../samples/common/triangle_sample.hpp)

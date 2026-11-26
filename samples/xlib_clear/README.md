# X11 Vulkan 清屏示例

示例从公共 C++ API 接入宿主 Xlib 窗口。支持手动缩放、最小化、恢复和关闭。`--verify` 执行有限帧窗口验收。仅在明确允许软件 ICD 时传 `--allow-software`。

构建和独立接入见 [X11 清屏指南](../../docs/xlib-clear.md)。

传入 `--triangle` 可显示彩色三角形。自动验收也会在缩放和恢复后绘制。 [三角形接入指南](../../docs/vulkan-triangle.md)

传入 `--texture` 可显示四色纹理矩形。 [纹理接入指南](../../docs/vulkan-texture.md)

传入 `--depth` 可显示两个重叠三角形的遮挡效果。 [深度指南](../../docs/vulkan-depth.md)

传入 `--indexed` 可显示由 4 个顶点与 6 个索引组成的彩色矩形。 [索引指南](../../docs/vulkan-indexed.md)

传入 `--binding` 可查看独立纹理和重复采样器组成的绑定。 [绑定指南](../../docs/vulkan-bindings.md)

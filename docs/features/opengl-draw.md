# OpenGL 离屏三角形

关联 F07/F09/F10/F11/F18，契约版本 `0.1.0-draft.24`。

- 在 `GraphicsPipelineDesc` 末尾提供一组成对的 GLSL 源码字段。应用可同时填写一组 SPIR-V 与一组 GLSL，后端选用对应载荷。未提供任何完整 Shader 对、部分填写一个阶段、GLSL 内嵌 NUL 或单阶段超过 1 MiB 时返回 `InvalidArgument`。Null 检查输入契约，OpenGL 编译和链接 GLSL，Vulkan 消费 SPIR-V。
- 支持 OpenGL 3.3 Core、单流顶点布局、非索引 TriangleList、RGBA8 不透明离屏颜色绘制、完整清屏、动态整数视口和裁剪。片元 Shader 使用 location 0 输出。GLSL 接收原生裁剪空间。沿用 RHI 坐标时，Shader 或生成该载荷的工具负责 Y 与 Z 转换。视口、裁剪和读回坐标统一从左上角起算。
- OpenGL 通过共享设备契约维护管线与缓冲句柄的所有权和录制引用。编译或链接失败携带驱动诊断，管线创建失败不占用句柄。缺少后端载荷、深度、混合、剔除或声明的绑定布局，以及程序中的活动 Uniform，返回 `Unsupported`。不支持的提交在整批执行前拒绝，例如分数视口和索引绘制。
- Windows WGL 与 Linux 无窗口 EGL 验证顶点偏移、非零起点、视口与裁剪、像素方向、清屏状态恢复、Shader 失败恢复和迁移安装后的独立三角形示例。

[RHI 管线指南](../rhi-pipeline-api.md) · [绘制接口](../rhi-draw-api.md)

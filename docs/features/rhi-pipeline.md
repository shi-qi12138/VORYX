# F07/F10 图形管线首批

本批交付设备级不可变管线与显式单流顶点布局，目标后端为 Null 和 Vulkan。公开类型为 `GraphicsPipelineDesc`、`VertexAttribute`、`VertexFormat` 与 `PipelineHandle`，新增设备创建和销毁重载。

- 公共描述不包含图形 SDK 类型。设备提供归属、代数、线程、容量与输入校验。
- Null 拥有描述副本。Vulkan 拥有真实管线与其 RenderPass 和 PipelineLayout，创建失败完整回收部分原生资源。
- 旧窗口绘制复用原生管线实现。设备 Pass 与绘制命令已接入，设备资源布局仍在后续批次。 [设备绘制规格](rhi-draw.md)
- T42 覆盖 Null/Vulkan 共享生命周期、布局与阶段校验、SDK 独立消费，并通过自定义顶点排列、两份几何复用、剔除与混合像素读回验证真实行为。
- 当前不接收设备 Shader 句柄或反射产物。完整目标接口仍按架构推进。具体实现边界以本批接口指南为准。

[实际接口与限制](../rhi-pipeline-api.md) · [测试矩阵](../validation.md)

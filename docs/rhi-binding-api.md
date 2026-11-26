# RHI 设备资源绑定

Null 和 Vulkan 的设备 API 支持在一个离屏 Pass 内组合常量缓冲、RGBA8 采样纹理和独立采样器。应用可在同一 Pass 切换绑定集，通过 Draw 或 DrawIndexed 绘制不同资源的几何。从窗口 Presenter 借用设备时，同一套绑定和绘制接口也可用于窗口交换链图像。 [窗口帧](window-frame-api.md) [设备绘制](rhi-draw-api.md) · [窗口绑定](vulkan-binding-layout.md)

## 接入流程

1. 用 CreateTexture2D 同步上传 RGBA8 采样纹理，再用 CreateSampler 创建采样器。
2. 创建带 Constant 用途的 Buffer。使用初始数据或 WriteBuffer 时同时声明 CopyDst。
3. 创建布局，并把返回的设备布局句柄交给 GraphicsPipelineDesc.bindingLayout。
4. 用匹配布局的资源创建不可变绑定集。在 Pass 外将常量缓冲转换到 ConstantRead。上传成功的采样纹理已可供着色器读取，直接绑定即可。
5. 进入 Pass 后调用 SetPipeline、SetBindingSet 和已有的几何及动态状态接口，再绘制。一个 Pass 中可以为不同 Draw 调用切换绑定集。
6. EndRenderPass 后结束录制和提交。需要读回时先把颜色附件转换到 CopySrc，再调用 ReadTexture。

下面的片段假设已有 device、uniform、texture、sampler 和相应的 SPIR-V。完整示例包含上传、顶点及索引、深度附件、逐步错误处理和像素检查。 [可编译的独立 SDK 示例](../tests/integration/sdk_consumer/pipeline.cpp)

```cpp
using namespace voryx;

constexpr std::array slots{
    BindingLayoutEntry{3, BindingType::UniformBuffer, BindingVisibility::Vertex},
    BindingLayoutEntry{7, BindingType::SampledTexture, BindingVisibility::Fragment},
    BindingLayoutEntry{11, BindingType::Sampler, BindingVisibility::Fragment},
};
auto layout = device->CreateBindingLayout({slots});
if (!layout) {
    return 1;
}
const std::array entries{
    DeviceBindingSetEntry{7, texture},
    DeviceBindingSetEntry{3, UniformBufferBinding{uniform, 0, 16}},
    DeviceBindingSetEntry{11, sampler},
};
auto set = device->CreateBindingSet({layout.Value(), entries});
if (!set) {
    return 1;
}
// 创建管线时设置 desc.bindingLayout = layout.Value()。
// BeginRenderPass 后先 SetPipeline，再 SetBindingSet(set.Value())。
```

## 参数与状态

| 操作 | 当前契约 |
| --- | --- |
| CreateTexture2D | 创建不可变、单 mip、单层、单采样的 RGBA8_UNorm 纹理，每维为 1 至 4096 像素，并遵循设备尺寸上限。首行左上，0 行距表示紧密排列，允许额外行填充。每行至少为 width × 4 字节，最后一行的像素必须完整。返回前上传完成，逻辑状态为 ShaderRead。 |
| CreateSampler | 支持 Nearest、Linear，以及 U/V 各自配置 ClampToEdge 或 Repeat。默认线性过滤及边缘钳位。 |
| CreateBindingLayout | 仅 set 0，包含 1 至 8 个不重复槽位，槽号 0 至 31。每槽有一种资源，顶点和片元可见性可组合。 |
| CreateBindingSet | 每个布局槽必须恰有一项，传入顺序任意。创建完成后无需保留描述数组。 |
| UniformBufferBinding | 缓冲需有 Constant 用途。范围非空且在缓冲内，偏移遵循 GetCapabilities().uniformBufferOffsetAlignmentBytes，大小受 maxUniformBufferRangeBytes 限制。应用负责常量块的字节布局和完整大小。 |
| SetBindingSet | 仅可在活动 Pass 内、SetPipeline 后调用，布局句柄必须为创建当前管线时指定的同一个句柄。每次 SetPipeline 清除已绑定的集合，BeginRenderPass 清除全部绑定。 |
| Draw / DrawIndexed | 如果管线声明了绑定布局，每次绘制前必须有已绑定的集合。提交按整批命令顺序校验 ConstantRead 和 ShaderRead。 |

能力查询还提供 maxSamplers、maxBindingLayouts 与 maxBindingSets。采样纹理与附件共享 maxTextures 数量上限。当前提供上传即进入 ShaderRead 的静态采样纹理，以及上一批的颜色/深度附件。 [Vulkan 描述符与对齐](https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html)

## Shader 契约

当前设备管线接收 SPIR-V 顶点与片元模块。资源必须使用显式的 DescriptorSet 0 和 Binding 装饰。引擎检查资源声明的槽号、实际类型和可见阶段。支持 Uniform Block、独立的浮点非数组二维采样图像、独立采样器。应用在离线工具中验证完整 SPIR-V、着色器输入输出以及常量块布局。现阶段支持固定的单个绑定集，存储资源、资源数组、组合图像采样器、动态偏移和 PushConstant 超出当前范围。 [SPIR-V 规范](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html)

## 失败与资源寿命

- 绑定集保留布局、缓冲、采样纹理和采样器。管线也保留其布局。销毁仍被资源依赖的对象返回 ResourceBusy。
- 录制命令保留绑定集，阻止销毁该集合和写入其常量缓冲。Reset、命令析构或同步提交成功释放录制引用。集合创建后、录制引用产生前，可以写入拥有 CopyDst 用途的常量缓冲。离屏异步提交把资源持有责任移交给提交记录，在完成确认前继续阻止主机写入。 [异步资源寿命](rhi-async-submit.md#资源何时可释放)
- 录制中已知的缓冲状态不匹配、漏绑定、提交前资源状态不匹配均返回 InvalidState。失败的绑定操作不改变当前绑定。整批预校验失败保留已录制命令，可先提交相应转换，或将转换列表放在同一批前面后重试。
- 布局不同、参数和范围错误返回 InvalidArgument。空或已销毁的句柄返回 InvalidHandle，外部设备句柄返回 WrongDevice。超出原生常量范围或后端能力返回 Unsupported。
- 可先 Reset 或完成提交，然后销毁绑定集、管线，最后释放其依赖资源和布局。设备与命令接口增加了虚函数，宿主升级时使用同版本头文件和静态库重新编译。

组合样例用同一常量缓冲的两个对齐区间驱动两个 DrawIndexed。在同一深度 Pass 内，较近网格先绘制。读回检查较远区域为绿色、遮挡处仍为红色、变换边界外为蓝色。SDK 安装消费用例在 Null 路径验证清除与契约，在 Vulkan 路径验证实际光栅结果。 [功能规格](features/rhi-binding.md) · [验证记录](environment.md)

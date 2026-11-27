# RHI 设备绘制与离屏附件

当前版本为 `0.1.0-draft.24`。设备级绘制使用同一套 IDevice/ICommandList 接口管理附件、管线、顶点、索引、录制和提交。Vulkan 执行真实离屏光栅化，Null 执行资源与命令契约校验及附件清除。OpenGL 支持同步离屏非索引颜色绘制、清屏和像素读回。窗口呈现沿用 Presenter 入口。 [设备管线](rhi-pipeline-api.md) · [窗口接入](vulkan-triangle.md)

只清除颜色附件时，按顺序执行 `CreateTexture`、颜色用途转换、`BeginRenderPass`、`EndRenderPass`、转为 `CopySrc`、同步提交和 `ReadTexture`。OpenGL 使用空深度句柄，省去管线与几何绑定。 [OpenGL 示例与接入](opengl-backend.md#离屏清屏和读回)

## 绘制调用流程

OpenGL 使用 GLSL 管线、`Draw`、空深度句柄以及整数像素视口。批次包含 OpenGL 暂未支持的操作时，`Submit` 在任何颜色写入之前返回 `Unsupported`。下面包含索引绘制的分支适用于 Null/Vulkan。 [OpenGL 版本及样例](opengl-backend.md#离屏三角形)

1. 通过 `CreateDevice` 选择后端，通过 `CreateGraphicsPipeline` 创建管线。
2. 创建颜色附件，可选创建同尺寸深度附件。顶点与索引使用已有缓冲接口上传。
3. Begin 后用 Barrier 转换顶点、索引和附件用途。
4. BeginRenderPass 后设置管线、顶点、可选索引、视口和裁剪，再调用 Draw 或 DrawIndexed。
5. EndRenderPass 后将颜色附件转到 CopySrc，End 并 Submit。
6. 提交成功后使用 ReadTexture 同步读回，最后释放资源。

以下片段假设 `device`、`pipeline`、`vertexBuffer` 和 `indexBuffer` 已创建，索引为覆盖四顶点矩形的六个 UInt32 值。完整程序处理每个 Result 并检查像素。 [完整独立消费示例](../tests/integration/sdk_consumer/pipeline.cpp)

```cpp
using namespace voryx;
auto texture = device->CreateTexture({
    {64, 64}, Format::RGBA8_UNorm,
    TextureUsage::ColorAttachment | TextureUsage::CopySrc, "color"});
if (!texture) {
    return 1;
}
auto created = device->CreateCommandList();
if (!created) {
    return 1;
}
auto command = std::move(created).Value();
// 下列操作依次检查 Result。任一录制失败时可修正输入或 Reset。
auto begin = command->Begin();
```

| 录制步骤 | 参数示例 |
| --- | --- |
| 缓冲转换 | `BufferBarrier{vertexBuffer, BufferUse::Initial, BufferUse::VertexRead}`，索引转 `IndexRead` |
| 附件转换 | `TextureBarrier{texture.Value(), TextureUse::Initial, TextureUse::ColorAttachment}` |
| 开始 Pass | `BeginRenderPass({texture.Value(), {0, 0, 1, 1}, {}, 1})` |
| 绑定管线 | `SetPipeline(pipeline)` |
| 绑定几何 | `SetVertexBuffer(vertexBuffer)` 和 `SetIndexBuffer(indexBuffer, IndexType::UInt32)` |
| 动态状态 | `SetViewport({0, 0, 64, 64, 0, 1})` 和 `SetScissor({0, 0, 64, 64})` |
| 绘制 | `DrawIndexed(6)` 或使用非索引几何调用 `Draw(vertexCount, firstVertex)` |
| 结束 Pass | `EndRenderPass()` |
| 准备读回 | 将附件从 `ColorAttachment` 转到 `CopySrc` |
| 完成录制 | `End()` 后用非空命令列表数组调用 Submit |
| 同步读取 | `ReadTexture(texture.Value(), output)`，64×64 RGBA8 的输出长度必须为 16384 字节 |

## 附件资源契约

`<voryx/rhi/texture.hpp>` 提供 TextureHandle、TextureDesc、TextureBarrier、RenderPassDesc、Viewport、Rect2D 和 IndexType。颜色、尺寸与通用格式位于 types.hpp。PipelineColorFormat 是 Format 的兼容别名，原有枚举值继续保留。

| 输入 | 本批已实现规则 |
| --- | --- |
| TextureDesc.extent | 两维均大于零，且不超过 GetCapabilities().maxTexture2DSize，最高为 4096，OpenGL 由驱动上限进一步约束 |
| 颜色附件 | Format::RGBA8_UNorm，usage 为 ColorAttachment 或 ColorAttachment 与 CopySrc 的组合 |
| 深度附件 | Format::D32_Float，usage 为 DepthAttachment，适用于 Null/Vulkan |
| 资源数量 | 使用 GetCapabilities().maxTextures 查询，原生内存不足可提前失败 |
| 纹理形态 | 单 mip、单层、单采样，不接受初始像素 |
| ReadTexture | 读取完整 RGBA8，要求 CopySrc 用途和逻辑状态、已提交的有效内容、没有未提交引用，输出长度恰好等于宽×高×4 |

ReadTexture 输出按行紧密排列，每像素为 RGBA 四字节，首行对应附件左上角的 y=0。函数在 GPU 工作与主机复制完成后返回，不借用调用方输出存储。其他声明的纹理用途与格式组合仍待逐步实现，已知但本批未支持的组合返回 Unsupported。

## Pass 与管线

RenderPassDesc 的 color 必填，depth 在 Null/Vulkan 可选，OpenGL 需要空深度句柄。存在深度时尺寸必须与颜色附件相同。clearColor 的每个分量必须有限，clearDepth 必须有限且位于 [0,1]。每次开始 Pass 都清除完整附件，颜色与深度结果保存。Load/DontCare 选项在后续批次扩展。

管线颜色格式须与附件一致。本批 Pass 的深度附件存在性必须与管线的 depthTest 一致，深度使用 D32_Float。每次 BeginRenderPass 清空之前的管线、几何和动态状态，Draw 前必须显式设置 Pipeline、Viewport 与 Scissor。Shader 使用顶点属性时还须设置顶点缓冲。

Barrier 和 CopyBuffer 只能在 Pass 外录制，End 之前须先 EndRenderPass。Reset 可以放弃未完成的 Pass，并释放本列表的所有资源引用。

## 绘制范围

- SetVertexBuffer 的缓冲必须具有 Vertex 用途，偏移为 4 的倍数且位于缓冲内。
- SetIndexBuffer 要求 Index 用途，支持 UInt16/UInt32，偏移按对应宽度对齐且位于缓冲内。
- Draw 的数量必须大于零且为 3 的倍数。对使用顶点输入的管线，按 firstVertex、数量、stride 和绑定偏移检查完整字节范围。
- DrawIndexed 的数量同样为正的 3 的倍数，按 firstIndex、数量、索引宽度和绑定偏移检查索引字节范围。
- DrawIndexed 的 vertexOffset 为有符号顶点偏移。应用负责保证每个索引加偏移后的顶点访问有效，接口不会为检查而同步读回索引内容。
- 视口必须为附件内的正尺寸有限范围，深度范围满足 0≤minDepth≤maxDepth≤1。裁剪为附件内的正尺寸整数矩形。

Vulkan 的索引偏移应用顺序与公共契约一致。 [Vulkan 索引绘制](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdDrawIndexed.html)

## 状态、错误与生命周期

BufferBarrier 与 TextureBarrier 分别接受对应类型的 span。空屏障数组须明确 span 类型，以避免 Barrier 重载歧义。

新纹理从 Initial 开始，本批支持颜色附件、深度写入和复制源状态。转换目标不能为 Initial。录制时检查已知的局部状态，提交时按整个列表批次再次验证资源状态。一个列表可承接前一列表预测的最终状态。

录制失败不追加该条命令或改变当前绑定。整批提交在后端执行前完成验证，状态校验失败不会修改附件内容、逻辑用途、提交序号和命令状态，可以修正或重置后重试。默认同步提交，成功返回时命令为 Completed，引用已释放。Vulkan 离屏绘制可选择异步提交；在主机读回前先确认提交完成。 [离屏异步提交](rhi-async-submit.md)

录制中和可执行的命令持有附件、缓冲与管线。资源存在未提交引用时 Destroy 或主机读写返回 ResourceBusy。Reset、命令析构或成功提交释放本列表持有的引用。设备先行销毁后，仍存活的命令包装对象会拒绝后续操作，析构可回收其保留资源。

| 情况 | 错误 |
| --- | --- |
| 空、已销毁或代数失配句柄 | InvalidHandle |
| 其他设备的句柄 | WrongDevice |
| 其他线程调用 | WrongThread |
| 非法尺寸、参数、范围、格式匹配或用途 | InvalidArgument |
| 缺失 Pass、管线或动态状态，提交用途不匹配，读回内容尚未有效 | InvalidState |
| 仍被录制命令引用 | ResourceBusy |
| 超出支持的尺寸或命令容量，未支持的用途组合 | Unsupported |
| 槽位或原生内存耗尽 | OutOfMemory |

Vulkan 使用显式图像屏障和外部子通道依赖同步附件转换与连续 Pass，读回使用复制缓冲及主机可见性屏障。 [Vulkan 同步示例](https://docs.vulkan.org/guide/latest/synchronization_examples.html)

## 本批边界与验证

设备纹理采样与常量缓冲已通过 SetBindingSet 接入。窗口帧已通过 Presenter 接入同步绘制。异步多帧提交继续推进。 [窗口帧](window-frame-api.md) [设备绑定](rhi-binding-api.md)现有 Presenter 入口继续提供窗口绘制。Null 对绘制调用执行契约校验，真实光栅化结果由 Vulkan 用例读回验证。

三后端共享附件契约覆盖连续清除、批次失败后的内容保持、纹理归属和 Present 用途拒绝。OpenGL 的安装示例验证全图清屏及非对称三角形像素。Null/Vulkan 的绘制契约覆盖状态顺序、越界、错误恢复、资源忙和设备先行销毁。Vulkan 像素用例另外覆盖两种索引宽度、非零起点、有符号顶点偏移、裁剪、非索引绘制、深度遮挡与跨提交复用。安装消费程序在独立工程中通过公共接口完成索引绘制与读回。 [功能规格](features/rhi-draw.md) · [环境证据](environment.md)

# 用户接入与调用流程

状态：窗口渲染设计指南。Core、缓冲 RHI 与 Null/Vulkan 缓冲已可用，见 [缓冲 API](rhi-buffer-api.md) 和 [SDK 接入](sdk-integration.md)；Win32、X11 与 Wayland Vulkan 清屏已有可运行示例。 [Win32](win32-clear.md) · [X11](xlib-clear.md) · [Wayland](wayland-clear.md)。可运行的绘制入口见三角形和显式绑定指南。本页后续的通用窗口、Shader 与命令接口为目标规格。 [Vulkan 三角形](vulkan-triangle.md) · [显式绑定布局](vulkan-binding-layout.md)

设备级离屏绘制现已支持显式 Pass、管线和几何绑定、Draw/DrawIndexed 及读回，具体可用签名见绘制指南。Vulkan 离屏任务可连续异步提交，提交后用令牌查询、等待和管理资源。 [设备绘制](rhi-draw-api.md) · [离屏异步提交](rhi-async-submit.md)

通过 CreatePresenter 和 GetDevice 可以获取窗口帧并使用设备命令绘制。窗口也可显式选择双帧异步提交，完整可用示例见窗口帧指南。 [窗口帧](window-frame-api.md)

## 1. 当前能做什么

安装 SDK 包含 Core、Null/Vulkan 缓冲和 Vulkan 窗口 Presenter。窗口应用可以运行彩色、纹理、深度和索引示例，也可以用 `--resources` 验证常量缓冲与纹理组合。实际使用步骤和验收范围见对应指南。 [Vulkan 后端](vulkan-backend.md) · [SDK 接入](sdk-integration.md) · [显式绑定布局](vulkan-binding-layout.md)

## 2. 接入前准备

先按 [跨平台矩阵](cross-platform.md) 选择系统/窗口系统/后端。Windows 可选 Vulkan、DX12、OpenGL；Linux 选择 Vulkan/OpenGL，并使用 ForXlib 或 ForWayland 构造原生描述，不能传 Windows 窗口值。Linux DX12 返回 Unsupported。

Linux 的显示连接和窗口必须持续有效；Wayland surface 完成宿主所需配置后再创建 Runtime。各 WSI 初始接入采用事件与渲染同线程的验收基线，独立线程配置须单独确认宿主并发规则。

宿主负责创建并保持窗口、运行消息循环、提供物理像素尺寸。渲染线程负责所有 Runtime 与 RHI 操作；最简单的集成是将宿主与渲染放在同一个线程。

首次真实渲染建议显式选择 Vulkan / DX12 / OpenGL，便于诊断；需要自动选择时使用 Auto，并打印 GetInfo 中的实际后端。Null 只适合契约测试，不会显示画面。

暂不共享宿主已经创建的 OpenGL 上下文；如需试用多个后端，为各次创建提供新的兼容窗口，避免平台像素格式等配置残留。

## 3. 最小清屏流程

此流程不需要 Shader、顶点缓冲或 Pipeline，适合作为第一个真实样例。以下命令名都对应 [API 文档](api-reference.md)。

1. 创建宿主窗口，读取客户区物理像素尺寸；若为零，先处理窗口事件，延迟 Runtime 创建。
2. 填写 RuntimeDesc：显式后端、有效 NativeWindowDesc、正 extent，其他字段可使用默认值。需要验证时设 enableValidation=true；缺少验证组件会创建失败。
3. Runtime::Create，先检查 Result。成功后取得借用的 IDevice，记录实际后端、格式、尺寸。
4. CreateCommandList，创建一个命令列表和一个可选的“上次 SubmissionToken”存储。
5. 每帧先处理宿主消息，再决定是否需要 Resize 或暂停；不在活动帧期间 Resize。
6. 重用命令前，若有上次提交，Wait 或 IsComplete 确认完成，再 Reset。
7. AcquireFrame，示例可使用有限等待（如 100,000,000 ns）。成功后保存 FrameInfo；失败按第 6 节处理。
8. Begin；Barrier 将 FrameInfo.color 从 Present 转到 ColorAttachment。
9. BeginRenderPass：color 使用本帧图像，colorLoad=Clear，clearColor=(0.05,0.10,0.20,1)。不设置深度附件。
10. EndRenderPass；Barrier 从 ColorAttachment 转回 Present；End。
11. Submit：commandLists 只包含本命令，frame 使用本次 FrameToken。成功后保存 SubmissionToken。
12. Present(FrameToken, SubmissionToken)。不必先 CPU 等待；成功或 SwapchainOutOfDate 都消费本帧 token。
13. 循环下一帧；退出时按第 8 节清理。

每一步返回 Result 的调用都必须检查。录制失败不能直接继续 End/Submit；提交失败也不能拿不存在的 SubmissionToken 调用 Present。

这个样例每帧等待重用单个命令列表，优先保证清晰与正确。后续可以使用多个命令列表与各自 token 轮换，但仍不得忽略图像与呈现的独立生命周期。

## 4. 在清屏基础上画三角形

前提是 Shader 产物格式 Q03 已冻结、工具能为目标后端生成兼容产物；目前不提供虚构的编译命令或字节码。

| 准备步骤 | 明确输入与顺序 |
| --- | --- |
| 顶点数据 | 三个 Float3 位置：(-0.5,-0.5,0.5)、(0.5,-0.5,0.5)、(0,0.5,0.5)；共 36 字节，stride=12 |
| 顶点缓冲 | sizeBytes=36，usage=Vertex\|CopyDst，初始数据长度正好 36；创建返回后源数据可释放 |
| Shader | 加载目标后端的 Vertex 与 Fragment 产物；顶点位置 location=0，片元输出固定不透明颜色 |
| Pipeline | colorFormat 使用 GetInfo 的实际格式；location=0、Float3、offset=0、stride=12；空绑定布局，无深度，默认不剔除 |
| 首次用途转换 | 在第一次引用顶点的命令中，Barrier：Initial→VertexRead；后续帧保持 VertexRead，无需重复 Initial |
| 绘制状态 | Pass 内依序 SetPipeline、SetVertexBuffer(offset=0)、SetViewport、SetScissor，后两者覆盖完整 FrameInfo.extent |
| 绘制 | Draw(vertexCount=3, firstVertex=0)，然后照常结束 Pass、转 Present、提交、呈现 |

窗口重建后若交换链格式改变，需要按新格式重建 Pipeline；尺寸改变时始终更新 Viewport 和 Scissor。不允许假设请求格式等于实际格式。

如果首次录制未成功提交，顶点仍为 Initial；不能仅凭“录制过 Barrier”更新应用记录。只有 Submit 成功后才更新已提交状态。

## 5. 带纹理与深度的调用差异

- 创建 RGBA8_UNorm 或 RGBA8_sRGB 的 Texture，usage=Sampled|CopyDst，输入从左上开始逐行排列；rowPitchBytes 与总长度按接口公式填写。
- 创建 Sampler；创建 BindingLayout，片元可见的 slot0=SampledTexture、slot1=Sampler；再创建匹配的 BindingSet。
- Shader 反射必须与此布局一致；Pipeline 指定同一个 BindingLayout，Pass 内在 SetPipeline 后 SetBindingSet。
- 首次使用纹理前在 Pass 外执行 Initial→ShaderRead 的 Barrier，不能在 Pass 内补屏障。
- 深度 Texture 的 extent 与本帧颜色相同，format=D32_Float、usage=DepthAttachment，无初始数据；首次 Initial→DepthWrite，后续保持 DepthWrite。
- Pipeline 设置 depthFormat=D32_Float、depthTest=true、depthWrite=true；RenderPass 使用此 depth，清为 1。
- 缩放后等待旧深度资源使用完成，销毁旧 BindingSet/附件依赖，再创建新尺寸深度图。先完成待重用命令的 Reset，避免 ResourceBusy。

v0.1 不自动创建纹理 mip、不自动生成相机矩阵、不自动填充常量缓冲。超出首版功能范围的需求先看能力与版本说明。

## 6. 每帧错误处理

| 出现位置 / 结果 | 用户处理 |
| --- | --- |
| Acquire：NotReady / Timeout | 没有活动 token，处理消息后稍后重试 |
| Acquire：SurfaceUnavailable | 暂停绘制，避免忙循环；等待正尺寸事件 |
| Acquire：SwapchainOutOfDate | 没有活动 token，获取最新尺寸并 Resize |
| Acquire：成功且 suboptimal | 先完成或取消当前帧，再 Resize |
| 录制失败 / Submit 验证失败 | 不呈现；Reset/销毁引用本帧的命令，再 CancelFrame |
| CancelFrame：ResourceBusy | 仍有未提交命令引用图像，找到并重置后重试 |
| Present：SwapchainOutOfDate | 本帧 token 已消费；Resize，不再用该 token Cancel/Present |
| Present：参数错误 | token 未消费，纠正配对的提交/帧或状态；不能再次获取覆盖它 |
| 任意 GPU 操作：DeviceLost | 停止录制与提交，关闭 Runtime，重建并重传全部资源 |
| Shutdown：Timeout | 保持 Draining，允许有限次等待/重试；不重新进入渲染循环 |

所有诊断记录操作名、错误码、实际后端、对象标签与原始后端错误。设备重建后，旧 handle/token 一律无效。

## 7. 缩放、最小化和多线程接入

宿主线程只传递新的物理像素尺寸事件，不直接从另一线程调用 Resize。渲染线程在帧边界取最新值，先解决活动帧，再调用 Resize。

Resize(0,0) 或任一维为零使交换链暂停；此时仍处理窗口事件，Acquire 返回 SurfaceUnavailable。恢复正尺寸后再次 Resize。窗口变化发生在正在录制时，可先结束并呈现或取消当前帧，再处理更新。

后台资源加载只能准备 CPU 字节与 Shader 产物，然后移交给渲染线程创建。初期 Create/Write 上传是同步调用，不能在后台线程直接访问设备。

## 8. 正常退出顺序

1. 停止生成新帧和新的资源请求，保持窗口存活。
2. 尚未提交的帧：Reset/销毁引用命令，再 CancelFrame；已经提交的帧：Present 消费 token，或交由 Shutdown 收尾。
3. 释放所有命令列表包装对象；GPU 在途存储由设备保护，不必因此提前释放原生对象。
4. 可选择显式按依赖逆序销毁 Pipeline、BindingSet、Shader、BindingLayout、Buffer/Texture/Sampler。遗漏的资源由设备关闭统一清理。
5. Shutdown 使用有限超时并检查结果；ResourceBusy 表示仍有命令包装对象，Timeout 可保持窗口并重试。
6. Shutdown 成功后不再访问借用的 Device；释放 Runtime，再销毁宿主窗口，最后释放日志 userData。

如果希望销毁日志输出或窗口，必须保证 Runtime 已结束，避免回调与平台访问悬空。

## 9. 常见误用

| 现象 | 首先检查 |
| --- | --- |
| Null 创建成功但没有画面 | Null 不显示图像，选择真实后端 |
| 切换后端后资源句柄报错 | 句柄归属旧 Device，必须重新创建 |
| ResourceBusy | 未提交命令、BindingSet/Pipeline 的 CPU 依赖、或 WriteBuffer 的在途使用 |
| Draw 返回 InvalidState | Pass、Pipeline、Viewport、Scissor、资源状态是否齐全 |
| 颜色/纹理上下颠倒 | 数据是否按统一左上原点；不要另加某后端翻转 |
| 画面在缩放后错误 | 新尺寸、深度附件、Viewport/Scissor、Pipeline 实际格式 |
| Present 后 Wait 已完成却仍有显示延迟 | GPU 完成不等于显示扫描完成 |
| 编译找不到头文件或 target | 当前仍为设计阶段；正式发布后按实际支持矩阵和安装命令排查 |

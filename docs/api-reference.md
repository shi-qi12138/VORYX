# C++ 接口调用契约

版本：0.1.0-draft.21。Core 见 [Core API](core-api.md)，已实现的缓冲 RHI/Null/Vulkan/低层工厂见 [缓冲 RHI](rhi-buffer-api.md)，安装见 [SDK 接入](sdk-integration.md)。设备级管线与离屏绘制首批的准确签名与限制见 [管线 API](rhi-pipeline-api.md) 和 [绘制 API](rhi-draw-api.md)。本页为完整渲染目标规格；设备纹理、绘制和绑定的首批范围以专题接口为准。Vulkan Presenter 提供默认同步提交、显式双帧异步提交和 `Shutdown(TimeoutNs)`，Vulkan 设备也支持离屏异步。完整 Runtime 仍以目标规格描述。 [离屏异步提交](rhi-async-submit.md) [绑定 API](rhi-binding-api.md) · [窗口帧 API](window-frame-api.md)

已实现的窗口清屏签名和错误语义见 [Win32](win32-clear.md) 、[X11](xlib-clear.md) 与 [Wayland](wayland-clear.md)。三角形的实际 Shader 布局、参数和错误语义见 [Vulkan 三角形](vulkan-triangle.md)。实际 RGBA8 上传和采样契约见 [Vulkan 纹理](vulkan-texture.md)。彩色三角形深度测试的实际接口见 [Vulkan 深度测试](vulkan-depth.md)。索引绘制的实际输入与校验见 [Vulkan 索引绘制](vulkan-indexed.md)。已实现的 Presenter 单槽纹理绑定与多槽常量缓冲组合见 [纹理绑定](vulkan-bindings.md) 和 [显式绑定布局](vulkan-binding-layout.md)。当前只有已实现参考页明确列出的签名可直接使用；本页其他方法为后续规格。命名空间统一为 voryx，文中省略此前缀。除非另行注明，参数中的描述和 span 都在调用返回前读取或复制，不保留调用方临时内存。

## 1. 全局约定

### 1.1 结果与基本类型

| 类型 | 契约 |
| --- | --- |
| Result<T> | 成功持有 T，失败持有 Error，二者互斥；支持移动型结果 |
| Result<void> | 表达成功或失败，不附带无意义值 |
| HasValue() / Value() / GetError() | 先检查 HasValue；仅在对应状态访问 Value 或 GetError；错误访问属于调用方契约违反 |
| Error | errorCode、operation、message、可选 backendCode；字符串由结果自身拥有 |
| Extent2D | width / height：uint32，默认均为 0，单位物理像素，不是逻辑窗口尺寸 |
| ByteSpan | std::span<const std::byte>，单位字节；空数据使用空 span |
| TimeoutNs | uint64，单位纳秒；0 为不等待，UINT64_MAX 为无限等待；用户关闭流程建议有限超时 |
| 各资源 Handle | 不可混用的强类型；默认值无效；内部含设备归属、槽位与代数，布局不对用户承诺 |
| SubmissionToken | 设备归属 + 单图形队列序号；只能交给产生它的设备 |
| FrameToken | 标识一次交换链获取；复制 token 不增加获取次数或生命周期 |

首期所有有设备状态的操作必须在创建 Runtime 的线程调用，此线程即渲染线程。设备销毁后，包括查询在内的所有引用均失效。

内存分配或后端失败使用 Result；公开操作不以抛异常报告可恢复错误。销毁不抛异常。错误诊断尽可能保留原始原因，不能因格式化诊断失败覆盖原始错误。

### 1.2 错误码与恢复

| ErrorCode | 含义 | 调用方处理 |
| --- | --- | --- |
| InvalidArgument | 字段、范围、长度、用途或布局不合法 | 修改参数，不原样重试 |
| InvalidHandle | 默认、过期、已销毁或非预期类型句柄 | 修复生命周期；不得再次使用 |
| WrongDevice | 句柄或 token 来自其他设备 | 使用同设备对象 |
| WrongThread | 调用线程不符合约定 | 调度到渲染线程 |
| InvalidState | 命令或帧状态不允许该操作 | 按状态机恢复 |
| ResourceBusy | 对象仍被 CPU 依赖引用或 GPU 占用 | 释放依赖、重置命令或等待后重试 |
| BackendNotBuilt | 本平台目标内的后端未编入当前库 | 重建启用后端或显式改选 |
| BackendUnavailable | 已编入但驱动/设备不满足创建要求 | 检查环境或显式改选 |
| SurfaceIncompatible | 窗口与后端不兼容 | 改用新窗口或其他后端 |
| Unsupported | 合法请求不在设备/当前版本能力内 | 查询能力后选择可用方案 |
| OutOfMemory | CPU 或 GPU 内存不足 | 降低资源需求，必要时重建；不紧循环重试 |
| NotReady | 非阻塞查询暂未完成 | 稍后继续，不视作设备故障 |
| Timeout | 在时间内未完成 | 保持原有状态，稍后重试或退出 |
| SurfaceUnavailable | 零尺寸、最小化等暂时不能获取图像 | 暂停绘制，处理窗口事件后恢复 |
| SwapchainOutOfDate | 交换链必须重建 | 按获取/呈现规则处理 token，再 Resize |
| DeviceLost | 设备失效，原资源不再可用 | 停止提交，清理并重建全部设备资源 |
| InternalError | 引擎或后端无法分类的失败 | 保存诊断，停止本帧；无法确认健康状态时关闭 Runtime |

同一调用可能有多个问题时，先检查线程与设备健康，再检查句柄与参数；不要求依赖具体错误优先级编写业务逻辑。

## 2. Runtime 与创建

### 2.1 RuntimeDesc

| 字段 | 类型 | 默认值 | 约束与含义 |
| --- | --- | --- | --- |
| backend | BackendType | Auto | Auto / Null / OpenGL / Vulkan / DX12；Null 必须显式选择 |
| window | NativeWindowDesc | 无效 | 真实后端必填，Null 可省略；不转移窗口所有权 |
| extent | Extent2D | 0×0 | 真实后端创建须为正；零尺寸窗口应延迟创建 |
| presentMode | PresentMode | Fifo | v0.1 只保证 Fifo；其他模式不属于本版枚举 |
| colorFormat | Format | Auto | 仅交换链允许 Auto；按 BGRA8_UNorm、RGBA8_UNorm 顺序选择受支持格式 |
| enableValidation | bool | false | true 且所需验证组件不可用时创建失败，不静默关闭 |
| logCallback | LogCallback | 空 | 空表示禁用用户回调；错误仍通过 Result 返回 |
| logUserData | void* | nullptr | 由调用方持有，至少存活到 Runtime 析构完成 |
| debugName | string_view | 空 | 创建时复制，用于诊断 |

NativeWindowDesc 是带标签的值变体，默认无效；以下值工厂构造描述，Runtime::Create 再验证。工厂本身不创建、引用计数或销毁原生对象，不包含系统 SDK 类型。

| 值工厂签名 | 字段与有效期 |
| --- | --- |
| ForWindows(uintptr_t windowValue, uintptr_t instanceValue) → NativeWindowDesc | Win32 窗口与实例的原生值；两者非零 |
| ForXlib(uintptr_t displayValue, uint64 windowValue) → NativeWindowDesc | 非零 Xlib Display 地址与 Window ID；不能传 xcb 连接或 Wayland 对象 |
| ForWayland(uintptr_t displayValue, uintptr_t surfaceValue) → NativeWindowDesc | 非零 wl_display 与已配置 wl_surface 地址 |

所有原生对象来自当前进程，由宿主保持到 Runtime 释放对应平台资源之后，不能使用随机整数。错误平台标签或本次构建未启用的 WSI 返回 SurfaceIncompatible。空/缺字段返回 InvalidArgument。Linux 显式选择 DX12 返回 Unsupported；这是平台范围检查，不归类为 BackendNotBuilt。窗口事件、物理尺寸与并发契约见 [跨平台设计](cross-platform.md)。

Null 使用相同逻辑交换链验证帧流程；extent 可为零以测试暂停，不产生窗口、图像显示或 GPU 工作。真实后端不得自动回退到 Null。

日志回调签名为 void(LogLevel, string_view message, void* userData) noexcept，LogLevel 为 Debug / Info / Warning / Error。message 只在回调期间有效；可从产生诊断的任意线程调用，用户负责同步，不抛异常，不重入引擎。Runtime 销毁返回后不再回调的保证仍待 Runtime 实现。

### 2.2 Runtime 操作

当前通过 CreatePresenter 与 IPresenter 使用 GetDevice、AcquireFrame、Present 和 CancelFrame。下表保留完整 Runtime 目标结构。实际签名和同步语义见窗口指南。 [窗口帧](window-frame-api.md)


| 签名 | 前置与返回 | 失败与副作用 |
| --- | --- | --- |
| Runtime::Create(const RuntimeDesc&) → Result<unique_ptr<Runtime>> | 当前线程成为渲染线程；成功拥有设备、Surface、Swapchain | 失败不返回部分 Runtime；不销毁宿主窗口 |
| GetInfo() const → const RuntimeInfo& | 创建成功且 Runtime 未析构；返回只读借用 | RuntimeInfo 含实际后端、设备名、交换链实际格式/尺寸和验证状态 |
| GetDevice() → IDevice& | Runtime 未到 Closed；返回借用，不转移所有权 | Draining/Lost 仅能用于允许的查询与清理；不得独立 delete |
| AcquireFrame(TimeoutNs) → Result<FrameInfo> | Ready、正尺寸、没有尚未消费的 FrameToken | 暂不可用/超时不产生 token；成功产生唯一活动帧 |
| Present(FrameToken, SubmissionToken) → Result<void> | token 有效，提交与该帧匹配，最终用途为 Present | 只安排呈现，不代表显示完成；详细消费规则见第 6 节 |
| CancelFrame(FrameToken) → Result<void> | 尚未提交该帧，且引用该帧的命令均已 Reset 或销毁 | 负责后端释放/归还获取图像，可能等待；成功消费 token |
| Resize(Extent2D) → Result<void> | 无活动帧；须先重置持有旧交换链图像的命令 | 可等待旧交换链工作；零尺寸进入暂停；失败保留待重建状态 |
| Shutdown(TimeoutNs) → Result<void> | 渲染线程，可从 Ready/Draining/Lost 调用 | 停止新提交；成功到 Closed；超时停在 Draining，可重试 |

RuntimeInfo 字段为 backend:BackendType、deviceName:string、extent:Extent2D、colorFormat:Format、validationEnabled:bool。借用内容可能在 Resize 后变化，不能持有其内部字符串/字段地址越过状态变更。需要长期保存时复制。Null 的 Auto 颜色格式固定使用逻辑 RGBA8_UNorm。

Auto 在 Windows 使用 Vulkan → DX12 → OpenGL，Linux 使用 Vulkan → OpenGL；仅因未编译、运行环境不可用、能力不满足或 Surface 不兼容继续尝试。参数错误、错误线程、内存不足、设备丢失、内部错误直接终止。最终错误包含候选诊断，成功后可查询实际后端。改变平台候选顺序属于契约变更，其他平台不因存在枚举而自动获得支持。

Shutdown 首次调用进入 Draining：拒绝新创建、上传、录制、提交和获取；允许完成查询、等待、已有资源的销毁、命令重置/销毁和重试 Shutdown。它处理未提交命令与未消费帧，等待在途工作，再关闭后端。成功后所有设备借用与句柄失效。

Shutdown 发现仍有用户持有的命令列表包装对象时返回 ResourceBusy，保持 Draining，调用者须先销毁它们再重试。Shutdown 前不必手工销毁每个 GPU 句柄，设备会最终清理。析构为遗漏 Shutdown 的兜底，健康设备下可能等待 GPU，不承诺有限耗时；需要可控退出应先显式 Shutdown 并处理超时。Lost 路径不等待永远不能完成的正常 fence。Runtime 析构前必须释放所有借用对象，不可依赖悬空包装对象的析构补救。

Draining 允许为清理而 CancelFrame；已提交但未呈现的帧由 Shutdown 内部收尾，不允许再发起普通渲染。Lost 时缓存能力仍可读取，Destroy 和命令 Reset/析构只做逻辑清理，不等待 GPU；Wait/IsComplete 返回 DeviceLost。Shutdown 成功后上述清理接口也不得再调用。

## 3. 设备、资源描述和依赖

### 3.1 IDevice 操作

全部为渲染线程操作；创建返回新拥有的逻辑句柄，描述引用只读借用到返回。

| 签名 | 成功语义 | 特有失败 / 约束 |
| --- | --- | --- |
| GetCapabilities() const → const DeviceCapabilities& | 返回设备存活期内有效的只读能力 | 不把构建开关当设备能力 |
| GetFormatSupport(Format) const → FormatSupport | 返回 sampled/color/depth/copy 支持 | Auto 无资源格式能力 |
| CreateBuffer(const BufferDesc&, ByteSpan initialData) → Result<BufferHandle> | 数据同步复制/上传，返回后输入可释放 | 非空数据必须正好等于 sizeBytes；无初始数据的内容未定义 |
| WriteBuffer(BufferHandle, uint64 offsetBytes, ByteSpan) → Result<void> | 返回时上传完成，之后提交可读 | 越界或缺少 CopyDst 为 InvalidArgument；有 CPU 依赖或未完成 GPU 使用为 ResourceBusy |
| ReadBuffer(BufferHandle, uint64 offsetBytes, MutableByteSpan) → Result<void> | 同步复制到调用方输出，保留逻辑用途 | 要求 CopySrc；CPU 引用时 ResourceBusy；当前 Null/Vulkan 缓冲子集已实现 |
| CreateTexture(const TextureDesc&, optional<TextureData>) → Result<TextureHandle> | 复制完整 mip0，返回时上传完成 | 深度纹理禁止初始数据；无数据内容未定义 |
| CreateSampler(const SamplerDesc&) → Result<SamplerHandle> | 创建采样状态 | 不支持的能力返回 Unsupported |
| CreateShader(const ShaderDesc&) → Result<ShaderHandle> | 复制并验证目标后端产物与反射元数据 | 不匹配的阶段/后端/产物版本为 InvalidArgument |
| CreateBindingLayout(const BindingLayoutDesc&) → Result<BindingLayoutHandle> | 复制布局 | 槽位重复或超限拒绝 |
| CreateBindingSet(const BindingSetDesc&) → Result<BindingSetHandle> | 创建不可变绑定集合，持有资源依赖 | 类型、数量、范围、对齐不匹配拒绝 |
| CreateGraphicsPipeline(const GraphicsPipelineDesc&) → Result<PipelineHandle> | 创建完整不可变图形状态，持有 Shader/布局依赖 | 与反射或格式不匹配拒绝 |
| CreateCommandList() → Result<unique_ptr<ICommandList>> | 新命令列表为 Initial，借用 Device | 对象必须先于 Device 销毁 |
| Submit(const SubmitDesc&) → Result<SubmissionToken> | 接收一次提交；尚未保证 GPU 完成 | 先完整验证，再提交；见第 5 节 |
| IsComplete(SubmissionToken) → Result<bool> | true 表示此次及此前同队列工作完成 | false 只是尚未完成；WrongDevice / DeviceLost 不转换为 false |
| Wait(SubmissionToken, TimeoutNs) → Result<void> | 返回成功表示 GPU 工作完成 | Timeout 保留 token 和资源状态 |
| Destroy(TypedHandle) → Result<void> | 对各资源类型提供重载；成功立即使句柄失效 | 默认句柄也为 InvalidHandle；仍有 CPU 依赖为 ResourceBusy；不能销毁借用的交换链图像 |

首期上传有意采用同步完成语义，可能阻塞，不适合每帧大量流式更新。WriteBuffer 不改变资源的逻辑用途，后端负责上传前后恢复必要的物理状态；调用方仍须在用途变化时显式 Barrier。异步上传将以新接口和完成标记另行设计，不悄悄改变这些返回语义。

### 3.2 Buffer / Texture / Sampler

| 描述 | 字段、默认值、合法范围 |
| --- | --- |
| BufferDesc | sizeBytes:uint64=0，必须 >0 且不超设备限制；usage:BufferUsage=无，必须显式非空；debugName 为空 |
| BufferUsage | 位集合 Vertex / Index / Constant / CopySrc / CopyDst；初始上传或 WriteBuffer 要求 CopyDst |
| TextureDesc | extent=0×0，必须正；format=Auto，但创建必须显式指定非 Auto；usage:TextureUsage=无，必须显式非空；debugName 为空 |
| TextureUsage | 位集合 Sampled / ColorAttachment / DepthAttachment / CopySrc / CopyDst；深度与颜色用途不得冲突；初始数据要求 CopyDst |
| TextureData | bytes:ByteSpan 必填；rowPitchBytes:uint32 必须至少为 width×4 且为 4 的倍数；bytes 长度为 (height−1)×rowPitchBytes + width×4；所有乘加检查溢出 |
| SamplerDesc | minFilter=Linear、magFilter=Linear；addressU/V=ClampToEdge；可选 Nearest 和 Repeat；无 mip、各向异性或比较采样 |

v0.1 格式枚举为 Auto、RGBA8_UNorm、RGBA8_sRGB、BGRA8_UNorm、D32_Float；颜色格式每像素 4 字节，D32_Float 只作深度且不接受初始化。具体组合须查询 FormatSupport。

DeviceCapabilities 字段为 maxBufferSizeBytes:uint64、maxTexture2DSize:uint32、maxUniformBufferRangeBytes:uint64、uniformBufferOffsetAlignmentBytes:uint64、maxBindingsPerSet:uint32、maxVertexAttributes:uint32，以及 supportedBlendModes / supportedCullModes / supportedDepthCompares 位集合。所有计数均为上限，不保证资源一定有足够内存创建。FormatSupport 包含 sampled / colorAttachment / depthAttachment / copySrc / copyDst 布尔字段；Auto 查询返回全 false。无查询值可用的限制不得由用户猜测。

### 3.3 Shader / Binding

| 描述 | 字段、默认值、合法范围 |
| --- | --- |
| ShaderDesc | stage=Vertex（Vertex/Fragment）；artifact:ByteSpan 必填，包含单入口点、目标后端与反射信息；debugName 为空 |
| BindingLayoutDesc | entries:span<BindingLayoutEntry>，空表示无绑定；创建时复制 |
| BindingLayoutEntry | slot:uint32；type 为 UniformBuffer / SampledTexture / Sampler；visibility 为 Vertex/Fragment 的非空位集合；一个 slot 对应一个资源 |
| BindingSetDesc | layout:BindingLayoutHandle 必填；entries:span<BindingEntry> 必须与布局逐槽完全对应；debugName 为空 |
| BindingEntry | slot + 强类型资源变体；UniformBuffer 额外包含 offsetBytes、sizeBytes；不能同时填写多种资源 |

UniformBuffer 要求 Constant 用途，范围正且在资源内，偏移满足设备对齐，长度不超 maxUniformBufferRangeBytes。SampledTexture 要求 Sampled 用途。BindingSet 创建后不可更新，替换资源须新建集合。

Shader 工具产物的二进制布局由待定项 Q03 冻结，在此之前不得宣称用户能自行生成兼容产物。运行时不接受任意 HLSL 字符串替代已验证产物。

### 3.4 GraphicsPipelineDesc

以下为完整目标描述。首批已安装头文件直接接收 SPIR-V，采用固定 D32 深度格式，尚未接入 Shader 句柄与设备 BindingLayout。实际字段以首批指南为准。 [当前管线 API](rhi-pipeline-api.md)


| 字段 | 默认值 / 规则 |
| --- | --- |
| vertexShader / fragmentShader | 必填，阶段必须匹配 |
| bindingLayout | 无效句柄表示空布局；否则有效并与 Shader 反射一致 |
| vertexStrideBytes | 0；若有顶点输入必须 >0 |
| vertexAttributes | 空；每项 location、format(Float2/Float3/Float4)、offsetBytes；不得重复或超过 stride |
| colorFormat | Auto 无效，必须指定本次颜色附件实际格式 |
| depthFormat | Auto 表示无深度附件；否则必须为支持的深度格式 |
| cullMode / frontFace | None / CounterClockwise |
| blendMode | Opaque；可选 Alpha、PremultipliedAlpha；固定为 Add，Alpha 的 RGB 因子 SrcAlpha/OneMinusSrcAlpha，预乘 RGB 为 One/OneMinusSrcAlpha，两者 Alpha 因子均为 One/OneMinusSrcAlpha |
| depthTest / depthWrite / depthCompare | false / false / LessEqual；比较可选 Less、LessEqual、Always；开启测试或写入要求深度格式；depthWrite 要求 depthTest=true |
| colorWriteMask | RGBA 全写 |
| debugName | 空，创建时复制 |

v0.1 固定 TriangleList、单采样、单颜色附件、单顶点流和单 BindingSet。顶点格式、整数宽度、偏移按元数据校验，不从宿主 C++ 结构体布局推断。

### 3.5 资源状态与 CPU 依赖

资源用途与创建 usage 不同：usage 声明允许的用途集合，Barrier 指定当前用途。Buffer 可取 Initial / VertexRead / IndexRead / ConstantRead / CopySrc / CopyDst；Texture 可取 Initial / ColorAttachment / DepthWrite / ShaderRead / CopySrc / CopyDst / Present。

新资源的逻辑用途为 Initial；同步上传不会破坏数据。Initial 只允许首次使用，不代表底层 Vulkan 的 Undefined，也不能用于丢弃已有数据。交换链图像每次 Acquire 的逻辑用途为 Present，需显式转换到 ColorAttachment 并在提交前转回 Present。

BindingSet 持有布局和绑定资源；Pipeline 持有 Shader 与布局；录制中的或可执行的 CommandList 持有所引用对象。Destroy 被这些 CPU 引用阻止，返回 ResourceBusy。应按依赖逆序销毁，不进行隐式级联销毁。

Submit 成功后，命令引用移交给提交记录，命令不可重复提交。此时可以逻辑销毁没有其他 CPU 依赖的资源，底层存储由提交记录保护到 GPU 完成；绑定集合依赖的底层对象一并保留。提交失败则保留原 CPU 引用。

## 4. ICommandList 录制

### 4.1 操作表

以下包含完整目标方法。当前已实现离屏 Pass、管线和几何绑定、视口、裁剪及两种绘制，附件固定清除并保存。设备 SetBindingSet 已支持当前离屏命令流，窗口帧也使用相同的命令接口。 [设备绑定](rhi-binding-api.md) · [窗口帧](window-frame-api.md) [本批可用接口](rhi-draw-api.md)


| 签名 | 规则 |
| --- | --- |
| Begin() → Result<void> | 只允许 Initial；成功为 Recording |
| Barrier(span<const ResourceBarrier>) → Result<void> | Recording 且不在 RenderPass 内；每项含资源句柄、beforeUse、afterUse；用途必须符合类型和 usage |
| CopyBuffer(const BufferCopyDesc&) → Result<void> | Recording 且在 Pass 外；源/目标为 CopySrc/CopyDst；offset/size 为字节，范围不越界；禁止重叠自复制 |
| BeginRenderPass(const RenderPassDesc&) → Result<void> | Recording、无活动 Pass；附件尺寸相同，状态已转换 |
| SetPipeline(PipelineHandle) → Result<void> | Pass 内；附件格式匹配；使已有 BindingSet 绑定失效，后续需重新绑定 |
| SetBindingSet(BindingSetHandle) → Result<void> | Pass 内且 Pipeline 已绑定；布局句柄须相同；资源必须处于 ShaderRead / ConstantRead |
| SetVertexBuffer(BufferHandle, uint64 offsetBytes) → Result<void> | Pass 内；VertexRead 和 Vertex 用途；首期仅一个流 |
| SetIndexBuffer(BufferHandle, IndexType, uint64 offsetBytes) → Result<void> | Pass 内；IndexRead，IndexType 为 UInt16 / UInt32，偏移按索引宽度对齐 |
| SetViewport(const Viewport&) → Result<void> | Pass 内；x/y/width/height 为像素 float，范围在附件内；minDepth=0、maxDepth=1，深度范围在 [0,1] |
| SetScissor(const Rect2D&) → Result<void> | Pass 内；非负整数 x/y，正整数 width/height，不超过附件 |
| Draw(uint32 vertexCount, uint32 firstVertex=0) → Result<void> | Pass 内；vertexCount>0，完成管线/输入/绑定/视口/裁剪设置 |
| DrawIndexed(uint32 indexCount, uint32 firstIndex=0, int32 vertexOffset=0) → Result<void> | 额外要求索引缓冲；校验已知范围，索引内容的有效顶点范围由调用方保证 |
| EndRenderPass() → Result<void> | 活动 Pass 存在；结束 Pass，不自动转为 Present |
| End() → Result<void> | Recording 且无活动 Pass；成功为 Executable |
| Reset() → Result<void> | 非 Pending；释放 CPU 引用回到 Initial；未完成时 ResourceBusy |

RenderPassDesc：color 为必填 TextureHandle，colorLoad 为 Clear（可选 Load / DontCare），clearColor 默认线性 RGBA=(0,0,0,1)；depth 为可选 TextureHandle，depthLoad 默认 Clear，clearDepth 默认 1，范围 [0,1]。附件结果固定保存，单采样。Load 只可用于已有有效内容；首次交换链图像内容不得假设有效。

ResourceBarrier 为 BufferHandle / TextureHandle 的类型安全变体加 beforeUse / afterUse；两种用途均须显式填写，不默认为 Initial。BufferCopyDesc 包含 src / dst:BufferHandle（必填）、srcOffsetBytes / dstOffsetBytes:uint64=0、sizeBytes:uint64=0（必须正且不越界）。

Viewport 字段 x/y:float=0、width/height:float=0（必须正）、minDepth:float=0、maxDepth:float=1；Rect2D 字段 x/y:uint32=0、width/height:uint32=0（必须正）。Viewport 和清屏颜色/深度的浮点输入必须有限，拒绝 NaN/Infinity；除 depth 外的颜色允许超出 [0,1]，由附件格式决定保存范围。

每次 BeginRenderPass 清空动态状态与绑定。Draw 前必须显式设置 Viewport、Scissor 和 Pipeline；只有 Shader 使用顶点输入或绑定时才要求相应对象。Clear 本身不要求 Pipeline 或 Viewport，覆盖全部附件。

所有录制数据在调用时复制；录制失败不追加该条命令，也不改变当前状态，允许修复或 Reset。Clear / Shader 的颜色输入以线性空间解释，sRGB 附件由后端负责正确转换。

### 4.2 命令状态机

```text
Initial --Begin--> Recording --End--> Executable --Submit--> Pending
   ^                   |                 |                      |
   |                   +----Reset--------+            完成查询/等待
   |                                                            |
   +---------------------------Reset---------------------- Completed
```

Pending 的完成可以由查询/等待或引擎内部轮询发现。Reset 应自行检查实际完成情况，不要求调用者先查询。完成后不能再次提交原命令，必须 Reset 并重新录制。

销毁命令包装对象释放其 CPU 引用；Pending 时的底层命令存储和资源依赖由提交记录延迟释放。仍要求该包装对象在 Device 销毁前析构。

## 5. 提交、屏障与完成

SubmitDesc 字段：commandLists 为非空 span<ICommandList*>，所有对象来自本设备、唯一、不重复、均为 Executable；frame 为可选 FrameToken，默认无；首版每次提交最多绑定一个活动帧；mode 为 SubmitMode，默认 Synchronous。Vulkan 离屏和窗口可选择 Async，能力分别查询 DeviceCapabilities.maxPendingSubmissions 与 maxPendingWindowFrames。

有交换链图像引用时必须传匹配的 frame；传入 frame 时至少一个命令引用其颜色图像，最终用途必须为 Present。首版一帧只允许一次携带 frame 的提交；离屏工作可以独立提交。

同一队列按提交顺序执行。Submit 前以整批命令顺序验证 beforeUse / afterUse；前置状态与前次成功提交的最终状态不一致则 InvalidState，整批不提交。引擎只生成后端对应同步，不替调用方推断缺失的 Barrier。

本次提交的状态与引用更新只有在后端接受后提交；验证失败不改变命令或帧，后端 DeviceLost 则使 Runtime 进入 Lost。无法确认是否接收的内部错误视为不可继续提交，要求关闭，不能盲目重试同一批命令。

SubmissionToken 的完成只保证 GPU 执行完成，不保证画面已经被显示系统扫描输出，也不等价于交换链呈现同步对象可以立即重用。后端独立跟踪呈现生命周期。

## 6. 帧状态与异常出口

FrameInfo 包含 token、color:TextureHandle、extent、format、suboptimal:bool。color 是当前帧借用，只允许作为颜色附件及转换到 Present，不能销毁、缓存到下一次获取或用于采样/复制。

```text
Idle --AcquireFrame成功--> Acquired --Submit(frame)成功--> Submitted
 ^                            |                              |
 |                       CancelFrame成功                 Present消费
 +----------------------------+------------------------------+
```

- Acquire 失败不返回帧资源；NotReady、Timeout、SurfaceUnavailable 均可按原因稍后重试。
- Acquire 返回 SwapchainOutOfDate 时没有 token，Resize 后重新获取。
- Acquire 成功但 suboptimal=true 时，可完成该帧再 Resize，不在活动帧中直接重建。
- 录制/提交验证失败且尚未提交时，Reset/销毁所有引用本帧的命令，再 CancelFrame。它会完成归还图像所需的后端操作，并非简单丢弃 token。
- CancelFrame 有 CPU 引用时为 ResourceBusy；已经提交为 InvalidState，应调用 Present。取消表示结束本次渲染意图，不承诺阻止显示更新；后端可能通过呈现已有或清理后的内容归还图像。
- Present 成功消费 token；返回 SwapchainOutOfDate 也消费 token，随后须 Resize；DeviceLost 使全部帧失效并进入 Lost。
- Present 的参数/状态错误不消费 token，调用方纠正后可再次调用。内部无法确认的后端失败进入不可继续提交状态，关闭 Runtime。
- CancelFrame 后，原帧的借用图像即刻失效；Present 消费 token 后同样失效，即使 GPU 尚未完成。
- Present 不要求 CPU 等待提交完成或 Reset 命令。已提交记录自动保护在途数据；下一次复用命令时才等待完成并 Reset。Resize 前必须 Reset 引用旧图像的未提交命令，已提交记录由后端同步处理。

## 7. 跨后端数值约定

- 世界坐标默认右手系，应用可自行转换；RHI 不读取场景坐标。
- 矩阵采用列向量，变换顺序为 projection × view × model × position，产物与 CPU 上传布局统一按列主序；不得直接假设自定义 C++ 矩阵内存兼容。
- 统一裁剪深度 z∈[0,w]、NDC 深度 [0,1]，近平面 0、远平面 1；第一版不默认 Reversed-Z。
- 图像、视口和采样坐标原点统一左上，纹理 (0,0) 为第一上传行；后端/Shader 工具链适配差异，调用者不按后端翻转。
- 正面默认为从目标图像正面观察的逆时针绕序；视口翻转不得改变公共定义。
- 颜色资源是否 sRGB 由 Format 明确声明；数值数据使用线性格式。交换链首版输出 UNorm，不自动承诺色彩管理或 HDR。

上述约定必须通过方向、绕序、深度与颜色专用样例验证，不以仅显示三角形作为一致性证明。

## 8. 当前窗口关闭与后续接口

`IPresenter::Shutdown(TimeoutNs)` 是当前可用的 Vulkan 窗口关闭入口，依赖设备与呈现完成状态。关闭开始后拒绝获取、取消、提交及呈现。应用释放命令列表包装后可以反复调用关闭，超时保留关闭中状态。完整 Runtime 的关闭契约仍见第 2 节。 [窗口关闭接口](window-frame-api.md#显式限时关闭)

CreateDevice(DeviceDesc) 已提供 Null/Vulkan 无窗口缓冲入口；其他后端的无窗口执行、高层 Renderer、Render Graph、自定义 Pass、原生互操作、异步上传、动态 BindingSet 和多队列仍未交付。SubmitDesc 已提供 frame 字段，用于 Presenter 的同步或异步窗口帧。 [实际窗口流程](window-frame-api.md)

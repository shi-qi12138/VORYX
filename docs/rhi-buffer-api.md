# 已实现的缓冲 RHI 与设备工厂

设备级图形管线与离屏绘制的新增接口单独列在专题指南。 [RHI 管线 API](rhi-pipeline-api.md) · [RHI 设备绘制](rhi-draw-api.md)

版本：0.1.0-draft.22。公开文件位于 include/voryx/rhi 与 include/voryx/runtime，命名空间为 voryx。当前包含 Null CPU 后端、可选 Vulkan 1.2 与 OpenGL 3.3 缓冲后端；本页与实际头文件一致，不把计划中的 Texture/绘制/交换链接口混入可用 API。

## 1. 创建与线程

CreateDevice(const DeviceDesc&) noexcept 返回 Result<unique_ptr<IDevice>>。DeviceDesc.backend 默认 Auto，debugName 默认为空并在成功创建时复制。显式 BackendType::Null 可创建；编入 Vulkan 时可显式选择 Vulkan，Auto 当前只尝试已编入的 Vulkan，永不选择 Null。编入 OpenGL 时可显式选择 OpenGL。OpenGL/Vulkan 或 Windows 上的 DX12 未编译时返回 BackendNotBuilt，Linux DX12 返回 Unsupported，未知枚举返回 InvalidArgument。

DeviceDesc 新增的配置均可省略：

| 字段 | 默认值 | 契约 |
| --- | --- | --- |
| enableValidation | false | Vulkan 为 true 时严格要求 Khronos 验证层、debug utils 和同步验证扩展；缺失返回 BackendUnavailable |
| allowSoftwareAdapter | false | Vulkan 默认排除 CPU 类型软件 ICD；true 仅允许它参与选择，硬件仍优先 |
| logCallback | nullptr | noexcept 日志回调；空则不输出。签名见 Core API |
| logUserData | nullptr | 借用指针，必须覆盖设备及全部命令对象生命周期 |

Null 忽略 Vulkan 验证/适配器配置，DeviceInfo 的两个标记为 false。Vulkan 的 softwareAdapter 来自原生物理设备类型，validationEnabled 表示实际启用验证与同步验证。回调可能由驱动/验证层线程同步调用，调用方负责并发安全；消息视图仅回调期间有效，不允许重入设备/命令操作或抛异常。设备意外先于命令销毁时，私有原生上下文可能延后释放，因此回调上下文也须继续存活。

Vulkan 可用但没有兼容硬件、或显式要求的验证组件缺失时返回 BackendUnavailable；分配失败为 OutOfMemory，原生 VkResult 保存在 backendCode 中。完整构建和诊断步骤见 [Vulkan 后端](vulkan-backend.md)。

Runtime 目前是工厂组装模块，尚无 Runtime::Create、窗口或 Shutdown 对象接口。不要按 [后续渲染草案](api-reference.md) 调用未声明的方法。

所有可失败的设备和命令操作都在创建设备的线程执行；错误线程返回 WrongThread。设备及命令包装对象也须在此线程析构，违反析构线程契约会终止进程。只读 GetInfo/GetCapabilities 为不可变借用，设备存活期间可读取，不能与销毁并发。

推荐先销毁命令再销毁设备。命令意外晚于设备仍可安全析构，其他操作返回 InvalidState；不允许对已销毁 IDevice 的原始引用继续调用。

OpenGL 的平台依赖、软件设备与验证模式见 [OpenGL 后端](opengl-backend.md)。

## 2. 能力与句柄

GetInfo 返回 const DeviceInfo&，包含 backend、name、debugName、softwareAdapter、validationEnabled；Null 名称明确说明 CPU 验证设备，不代表实际 GPU。

GetCapabilities 返回 const DeviceCapabilities&。Null 当前上限：每 Buffer 64 MiB、1024 个槽位、每命令列表 4096 条记录、每提交 64 个列表。调用方查询限制，不把这些值当成其他后端的保证。槽位或内存耗尽返回 OutOfMemory；超出请求上限返回 Unsupported。

BufferHandle 与 SubmissionToken 是不同强类型，默认无效，不公开可写内部字段。IsValid 只检查结构，旧句柄在销毁后仍可能返回 true，实际操作由设备校验归属、槽位和代数。不得序列化其内存或跨设备使用。

## 3. Buffer

| 调用 | 输入与成功语义 |
| --- | --- |
| CreateBuffer(BufferDesc, ByteSpan={}) | sizeBytes>0；usage 显式非空；debugName 复制；返回拥有资源的逻辑句柄 |
| WriteBuffer(handle, offsetBytes, ByteSpan) | 要求 CopyDst，范围不越界，返回前复制完成，保留逻辑用途 |
| ReadBuffer(handle, offsetBytes, MutableByteSpan) | 要求 CopySrc，返回前写入调用方输出，保留逻辑用途 |
| Destroy(handle) | 成功立即失效；默认、旧代数或重复销毁返回 InvalidHandle |

BufferUsage 是 Vertex、Index、Constant、CopySrc、CopyDst 的位集合，可用 | 组合；未知位拒绝。非空初始数据必须长度正好等于 sizeBytes，且含 CopyDst。调用返回后可以释放或修改输入数据。

读写范围按 offset<=size、length<=size-offset 检查，避免整数溢出。合法末尾的空 span 是无操作，但仍校验句柄、用途和引用；越界的空 span 也拒绝。未提交命令引用资源时，Read/Write/Destroy 返回 ResourceBusy。

Buffer 读回会在返回前填充调用方输出。Texture 读回另见绘制指南。当前未提供映射裸地址或异步上传。 [附件读回](rhi-draw-api.md)Null 未初始化存储为零，但跨后端代码必须初始化后再依赖数据内容。

## 4. 命令录制

CreateCommandList 返回 unique_ptr<ICommandList>，状态从 Initial 开始。GetState 返回 Result<CommandState>，其余录制方法返回 Result<void>。

1. Begin：只允许 Initial，进入 Recording。
2. Barrier(span<const BufferBarrier>)：显式给 buffer、beforeUse、afterUse，复制记录；afterUse 不能为 Initial，必须与创建用途匹配。
3. CopyBuffer(BufferCopyDesc)：给 src/dst、srcOffsetBytes/dstOffsetBytes（默认 0）、sizeBytes（必须正）。要求对应复制用途，不能越界；同 Buffer 复制目前返回 Unsupported。
4. End：Recording → Executable。
5. Submit：成功后不可再次提交同一记录；复用须 Reset，再 Begin。

BufferUse 包含 Initial、VertexRead、IndexRead、ConstantRead、CopySrc、CopyDst。新 Buffer 为 Initial；已有资源不能重新伪装为 Initial。记录时检查本列表已知状态，提交时检查整批真实前置状态，缺屏障不能由引擎猜测补全。

一次 Barrier 调用中后续项失败不保留前面记录；失败不增加资源引用。Reset 或命令析构释放本列表未提交引用，不影响其他列表的引用。

## 5. 提交与完成

SubmitDesc 包含 commandLists、frame、mode。commandLists 为 span<ICommandList* const>，必须非空、无空指针、无重复，全部来自本设备且 Executable。无窗口复制省略 frame；mode 默认 Synchronous。空命令列表可提交，空批次不可提交。

Submit 按列表顺序先模拟验证全部资源用途，再执行后端操作和提交逻辑状态。同一 CopyDst 范围被多次写入时，后一次复制覆盖前一次写入的重叠字节。任何提交前契约验证失败都不改变字节、已提交状态或命令状态；修复后可以重试或 Reset。Vulkan 已接受提交后若等待失败，或主机写入后刷新映射内存失败，设备进入 DeviceLost 并停止新的可失败操作。调用方销毁所有命令和设备后重建资源。

默认同步模式在返回前进入 Completed，并释放录制引用，合法令牌的查询与等待立即成功。Vulkan 离屏命令可显式选择 Async；此时 Submit 返回队列接受结果，完成状态由原生 fence 决定。timeoutNs 单位纳秒，默认令牌为 InvalidHandle，其他设备令牌为 WrongDevice。 [离屏异步提交](rhi-async-submit.md)

## 6. 用法与错误恢复

完整代码见 [缓冲样例](../samples/null_buffers/main.cpp)，安装接入见 [SDK 文档](sdk-integration.md)。建议先运行样例，再在 VS 启动 voryx_null_buffers 调试。

Result 先判断再访问。录制或提交失败后需要释放资源时，先 Reset/销毁所有引用它的未提交命令。选择异步时，等设备观察到完成再从主机读写相关缓冲。销毁可立即使仅有在途直接引用的句柄失效，原生缓冲随后回收。 [异步资源寿命](rhi-async-submit.md#资源何时可释放)

普通错误包含操作名；诊断分配失败时仍保留原错误码。OutOfMemory 可以只有空诊断。具体错误分类沿用 Core，未知方法不以无效果的成功实现充数。

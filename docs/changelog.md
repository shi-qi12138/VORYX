# 文档与接口变更记录

## 0.1.0-draft.25 — 2026-11-27

- 增加 `CreateFramePresenter` 与 `IFramePresenter`，保留原有 Vulkan 示例入口。帧获取与旧交换链同步共用等待预算，超时保留待完成内容。使用同版头文件和库重新编译宿主。 [窗口帧接口](window-frame-api.md)
- 离屏设备 `Auto` 实施平台候选顺序。采样器或绑定布局能力为零时返回 `Unsupported`。 [设备工厂](rhi-buffer-api.md) · [资源绑定](rhi-binding-api.md)
- 关闭 Null 后仍注册后端契约与安装消费测试，安装验证逐次使用新目录。 [SDK 接入](sdk-integration.md)

## 0.1.0-draft.24 — 2026-11-27

- `GraphicsPipelineDesc` 增加 GLSL 源码阶段对，OpenGL 可创建不透明 RGBA8 绘制管线，Vulkan 继续接受 SPIR-V。升级时使用同一次构建的头文件与库重编译应用。 [管线接口](rhi-pipeline-api.md)
- 新增 OpenGL 非索引 TriangleList、顶点缓冲、整数动态视口及裁剪，安装包附带独立三角形像素验证程序。 [功能规格](features/opengl-draw.md) · [调用指南](opengl-backend.md#离屏三角形)

## 0.1.0-draft.23 — 2026-11-27

- OpenGL 支持同步离屏 RGBA8 颜色附件清除和读回，复用现有纹理、Pass、屏障和提交接口。 [功能规格](features/opengl-clear.md) · [调用指南](opengl-backend.md#离屏清屏和读回)
- 安装包附带独立 `opengl_clear` 示例，迁移后通过 `find_package` 验证 64×64 像素颜色。 [SDK 接入](sdk-integration.md)

## 0.1.0-draft.22 — 2026-11-26

- 添加 Windows WGL 和 Linux 无窗口 EGL 的 OpenGL 3.3 Core 缓冲后端，通过既有设备工厂显式选择。
- SDK 导出新增 `Voryx_WITH_OPENGL`，安装验证运行同一缓冲复制样例。 [OpenGL 指南](opengl-backend.md) · [功能规格](features/opengl-buffers.md)

## 0.1.0-draft.21 — 2026-11-26

- Vulkan Presenter 新增 `Shutdown(TimeoutNs)`。调用停止新工作，并在同一预算内等待 GPU 和窗口呈现，超时可重试。虚函数表有变更，升级 SDK 后重新编译应用与自定义 Presenter 实现。
- 新增确定性的超时重试覆盖，以及 Win32、X11 和原生 Wayland 下连续异步重建与显式关闭的自动验证。 [窗口关闭接口](window-frame-api.md#显式限时关闭) · [功能规格](features/window-shutdown.md)

## 0.1.0-draft.20 — 2026-11-26

- Vulkan 窗口帧接受显式异步提交，Presenter 用两个帧槽轮换呈现资源。应用用 `DeviceCapabilities.maxPendingWindowFrames` 查询能力，新增字段需要同版本 SDK 重新编译。
- 同步继续为默认模式。窗口示例与迁移安装后的独立程序验证连续呈现和在途资源回收。 [窗口帧接口](window-frame-api.md) · [窗口双帧规格](features/window-async-frames.md)

## 0.1.0-draft.19 — 2026-11-24

- `SubmitDesc.mode` 默认同步，显式设置 `SubmitMode::Async` 可提交 Vulkan 离屏命令。新增 `DeviceCapabilities.maxPendingSubmissions`，公开结构变更要求应用与同版本 SDK 重新编译。
- 设备按实际 fence 查询和等待完成，允许同时保留多批离屏命令及其资源引用，确认完成后回收。
- 测试覆盖超时、容量、重试、连续依赖复制与迁移安装 SDK 的异步绘制读回。 [接口说明](rhi-async-submit.md) · [功能规格](features/rhi-async-submit.md)

## 0.1.0-draft.18 — 2026-11-23

- Presenter 新增借用设备、帧获取、呈现和取消。SubmitDesc 新增 FrameToken。设备 Pass 可直接使用同设备的交换链颜色图像。
- RHI 在提交前校验图像归属、有效令牌、颜色 Pass 和最终 Present 用途。错误预校验保留活动帧，呈现或取消使借用句柄失效。
- 三个窗口样例及迁移安装程序验证纹理索引绘制、取消、失败修复和暂停恢复。现有宿主升级时须随 SDK 重新编译。 [窗口帧接口](window-frame-api.md)

## 0.1.0-draft.17 — 2026-11-23

- IDevice 支持不可变采样纹理、独立采样器、绑定布局及绑定集。常量缓冲绑定使用现有 BufferHandle 的对齐范围。
- 图形管线验证 SPIR-V 资源声明和布局，ICommandList 支持 SetBindingSet。录制引用保护绑定资源，提交继续执行整批状态校验。
- 独立安装消费程序验证纹理、两段常量变换、索引与深度的组合像素。宿主升级时随新版静态 SDK 重新编译。 [设备绑定接口](rhi-binding-api.md)

## 0.1.0-draft.16 — 2026-11-23

- 新增设备 TextureHandle、附件创建、纹理屏障和同步 RGBA8 读回。共用 Format，PipelineColorFormat 保留兼容别名。
- 新增 Pass、管线和几何绑定、视口、裁剪与 Draw/DrawIndexed，Null/Vulkan 共用状态、范围和生命周期校验。整批校验通过后才执行。
- Vulkan 共用 RenderPass 创建逻辑，补齐连续 Pass 附件同步。独立安装消费增加实际索引绘制与读回，新增共享契约和像素验收。
- 设备与命令新增虚函数和重载，宿主需随静态 SDK 重编译。 [实际接口与范围](rhi-draw-api.md)

## 0.1.0-draft.15 — 2026-11-23

- 整理中文开发日记、进度看板和模块说明，统一根目录与文档中心入口，补充按批次维护规则。记录整理沿用当前契约版本。 [开发记录入口](project-status.md)
- 新增设备级图形管线、强类型管线句柄和显式顶点属性，Null/Vulkan 共用输入与生命周期校验。
- Vulkan 独立管线实现由设备和现有 Presenter 绘制复用，支持剔除、混合和 D32 深度状态。新增共享契约、像素验收及独立安装消费。
- 本批设备管线限无资源绑定 SPIR-V，设备绘制命令待下一批接入。Destroy 新增重载，空参数应写明确句柄类型，宿主需随静态 SDK 重编译。 [实际接口](rhi-pipeline-api.md)

## 0.1.0-draft.14 — 2026-11-09

- Vulkan 窗口 Presenter 新增常量缓冲、显式绑定布局、不可变绑定集和带 UV 的资源绘制入口，支持一个绑定集内的稀疏槽与顶点/片元可见性。
- 安装窗口样例新增 `--resources`。真实像素用例同时检查常量变换和采样颜色，另检查无效布局、错误资源类型、跨 Presenter 和依赖释放。宿主需随新版 SDK 重新编译。 [接口与样例](vulkan-binding-layout.md)

## 0.1.0-draft.13 — 2026-11-09

- 新增 Presenter 独立纹理、采样器与不可变绑定句柄，可在配置间复用同一张纹理。提供依赖校验及 `ResetDraw`。已有 `SetTexturedTriangleList` 保留原有行为。
- 安装示例支持 `--binding`。新增真实像素验收与忙、跨 Presenter、失效句柄检查。宿主需和新版静态 SDK 一起重新编译。

## 0.1.0-draft.12 — 2026-11-06

- 新增 `SetIndexedTriangleList`。彩色管线使用 uint32 索引缓冲，越界和非法数量在分配前拒绝。宿主需和新版静态 SDK 一起重新编译。
- 三平台窗口示例支持 `--indexed`。新增矩形两侧、共享边及背景的真实像素读回和 SDK 安装验收。

## 0.1.0-draft.11 — 2026-11-06

- 新增 `SetDepthTriangleList`，支持带 z 的彩色顶点和同步深度缓冲。宿主需与静态 SDK 一起重新编译。
- 三平台窗口示例支持 `--depth`，增加真实 GPU 两种顺序的遮挡像素断言与安装验收。

## 0.1.0-draft.10 — 2026-11-06

- 新增 `SetTexturedTriangleList`，支持单张 RGBA8 纹理、行距、UV 和固定采样绑定。宿主需与静态 SDK 一起重新编译。
- 三个平台的安装示例支持 `--texture`。添加真实 GPU 四象限 RGBA 像素读回和无效行距验收。

## 0.1.0-draft.9 — 2026-11-06

- `IPresenter` 新增 `SetTriangleList` 与 `DrawAndPresent`，提供 Vulkan 非索引彩色三角形、固定顶点布局、Shader 模块和图形管线。宿主需使用同一版 SDK 重新编译。
- Win32、X11 与 Wayland 窗口示例支持 `--triangle`。迁移 SDK 的示例附带已编译 Shader 夹具。
- GPU 验收使用同一管线读回像素，并验证窗口缩放、暂停恢复和无效配置替换。

## 0.1.0-draft.8 — 2026-11-06

- 新增原生 Wayland Surface、`NativeWindowDesc::ForWayland` 和 `IPresenter::SetFramebufferExtent`。宿主提交已确认的缓冲像素尺寸，Wayland 复用共享 Vulkan 交换链。
- 增加 Wayland 构建开关和 xdg-shell 示例。自动验收覆盖首次配置、尺寸变化、暂停恢复和迁移安装后的独立调用。
- `IPresenter` 增加虚函数，应用与静态 SDK 需要使用同一版头文件和库重新编译。

## 0.1.0-draft.7 — 2026-11-06

- Linux 新增 `NativeWindowDesc::ForXlib`，复用 Win32 已有的 Vulkan 清屏、交换链与同步。
- 增加 X11 SDK 选项、Xlib 链接依赖导出、独立安装样例与窗口测试。原生硬件和软件验证分开记录。
- `NativeWindowDesc` 增加 Xlib Display 字段。应用和静态 SDK 应使用同一次构建的头文件与库，已有应用需重新编译。

## 0.1.0-draft.6 — 2026-11-06

- Visual Studio 的括号检查目标按当前配置生成辅助 Ninja 编译数据库，再运行同一套 clang-tidy 检查。
- 新增 Win32 Vulkan 清屏呈现入口、FIFO 交换链、缩放重建与最小化恢复。公共契约无图形和操作系统 SDK 类型。
- 安装包包含可独立编译的宿主窗口示例。CTest 执行真实呈现、验证层检查和迁移安装后的窗口示例。
- Vulkan 无窗口路径继续兼容旧开发包基线。Win32 WSI 单独控制，并要求呈现 fence 能力。

## 0.1.0-draft.5 — 2026-11-05

- 新增 Vulkan 1.2 无窗口设备与原生缓冲、状态屏障、队列/fence、同步复制和读回；保留原生错误，不将窗口/绘制标为可用。
- 将缓冲句柄、引用、状态机及整批验证提取到私有 RHI 共享实现，Null/Vulkan 独立负责存储和执行。
- DeviceDesc 增加可选严格验证、显式软件适配器与日志回调；DeviceInfo 增加实际软件/验证标记。源调用的既有字段保持默认兼容，结构布局改变，需要重新编译 SDK 和应用，不承诺 ABI 兼容。
- 复用 20 项缓冲契约执行原生 Vulkan；独立安装消费与同一公开 API 样例可选择 Vulkan；硬件/软件证据分开记录。
- 增加专用构建预设、Vulkan 使用/开发规格及依赖文档；根代码格式配置保持不变。

## 0.1.0-draft.4 — 2026-11-04

- 实现强类型 Buffer/提交标记、最小 IDevice/ICommandList、低层 CreateDevice 工厂和 Null CPU 后端；批量验证先于状态/字节提交。
- 新增同步 Buffer ReadBuffer，覆盖实际上传/复制/读回；不把 Null 的同步完成当作真实 GPU 验证。
- 提供安装导出与 find_package SDK 接入，导出公共目标、保留内部链接依赖但不泄露私有头；附带可独立构建的安装样例。
- 增加 Null 契约、安装迁移与独立用户工程验收；自有说明性注释统一中文，原格式配置不变。

## 0.1.0-draft.3 — 2026-11-04

- 实现独立 Voryx::core：Error、ErrorCode、Result<T>/void、LogSink 与 noexcept 日志回调；明确结果不可赋值和无异常移动约束。
- 增加 21 项 Core 单元测试、三公共头独立编译、诊断样例和独立消费项目，默认四配置各 29 项测试。
- 子项目默认关闭开发验证依赖，可仅链接 Core；引擎接口与渲染后端仍未实现。
- 新增 F02 功能规格、Core API 与样例说明，同步功能范围和验证记录。

## 0.1.0-draft.2 — 2026-11-04

- 建立实际 CMake/presets、Windows MSVC 环境初始化、跨平台构建入口、GoogleTest 1.17.0 归档锁定及 SHA256 校验。
- 增加 C++20/线程/中文路径环境测试、格式与括号检查和负例自检，报告与复现步骤见 environment.md；这些不是引擎功能实现。

- 按需求将 Linux 纳入首版，新增跨平台目标矩阵与 WSI 边界；macOS/移动平台/ARM64 后续独立设计。
- 新增 ForXlib / ForWayland 原生描述工厂，定义连接生命周期和平台错误行为；Linux DX12 为 Unsupported。
- Auto 更新为平台候选序列：Windows Vulkan→DX12→OpenGL，Linux Vulkan→OpenGL。
- Windows/Linux 从 M1 起都要求构建和 CPU/Null CI；真实 GPU 发布验收覆盖七个系统/WSI/后端组合。
- 同步架构、目录、功能 F17、用例 T28–T30、指南、开发与测试规范。Linux 尚无实测结果。

## 0.1.0-draft.1 — 2026-11-04

- 建立架构、功能范围、接口契约、接入指南、开发规范与验证要求。
- 明确 v0.1 为 Runtime + RHI；高层 Renderer / Render Graph 后续单独设计。
- 将对外主接口确定为 C++；C API 和动态插件不进入首版。
- 明确 Auto 回退顺序、单队列模型、资源依赖、帧取消和关闭恢复规则。
- 所有接口与功能仍处于草案、未实现状态；无可构建 SDK 或实测支持矩阵。
- 补充独立目录规范、代码规范和 CTest/GoogleTest 自动化测试方案，功能 F16 与验证 T27 建立追踪。
- 保持现有 .clang-format 不变，增加 .clang-tidy 强制括号检查和根 AGENTS.md 项目约束。

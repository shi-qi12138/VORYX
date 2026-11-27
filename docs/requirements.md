# 功能规格与里程碑

状态为 0.1.0-draft.24。Core、缓冲 RHI、Null/Vulkan 缓冲、低层工厂及安装 SDK 已落地，分别见 [Core](core-api.md)、[缓冲 RHI](rhi-buffer-api.md)、[SDK 接入](sdk-integration.md)。已实现的 GPU 窗口绘制能力按下文列出。完整 RHI 与发布矩阵仍待验收。

Win32、X11 与 Wayland 清屏已完成 F05/F12 的首批功能，包括宿主窗口接入、清屏呈现、缩放和最小化恢复。完整图形与跨平台窗口功能仍按 M2 推进。见 [Win32 规格](features/win32-clear.md) 、[X11 规格](features/xlib-clear.md) 和 [Wayland 规格](features/wayland-clear.md)。

F07/F10 的首批固定布局彩色三角形已实现，使用真实 Vulkan 管线、顶点缓冲和非索引绘制。单张 RGBA8 纹理、固定采样器与组合描述符已完成 F06/F08 的首批纹理闭环。F10 的首批彩色深度测试已实现。固定彩色布局的 uint32 索引绘制已实现。F08 单槽可复用绑定以及 Presenter 显式多槽布局和常量缓冲已实现。通用设备与命令接口的绑定和管线继续按 M2 推进。 [显式布局规格](features/vulkan-binding-layout.md) [绑定规格](features/vulkan-bindings.md) [索引规格](features/vulkan-indexed.md) [深度规格](features/vulkan-depth.md) [纹理规格](features/vulkan-texture.md) [三角形功能规格](features/vulkan-triangle.md)

F07/F10 设备级图形管线首批已实现 Null/Vulkan 创建、显式顶点布局、剔除、混合、深度状态和销毁。设备离屏 Pass、Draw/DrawIndexed 和附件同步读回已实现，已接入设备级采样纹理、采样器和常量缓冲绑定，Win32、X11 与原生 Wayland 的设备绘制已支持获取、同步提交、呈现与取消。 Vulkan 窗口帧可显式选择异步，支持两个帧槽。Vulkan Presenter 支持限时关闭与超时重试。 [关闭规格](features/window-shutdown.md) [窗口双帧规格](features/window-async-frames.md) · [窗口帧规格](features/window-frame.md) · [设备绑定规格](features/rhi-binding.md) [绘制规格](features/rhi-draw.md) [管线规格](features/rhi-pipeline.md)

当前交付进度、历史记录和模块对应关系集中见跟踪文档。 [开发进度](project-status.md) · [开发日记](development-log.md) · [模块说明](module-map.md)

## 1. v0.1 目标

提供 Windows 与 Linux 下可嵌入的 C++ 渲染基础库，以同一公共接口完成窗口呈现、三角形、纹理和深度绘制。两平台验收 OpenGL/Vulkan，Windows 另验收 DX12；Linux 分别覆盖 X11 与原生 Wayland。目标矩阵见 [跨平台设计](cross-platform.md)。Null 只用于 CPU 契约验证，不能代替真实后端。

v0.1 的使用入口为 Runtime + RHI。场景级 Renderer、Render Graph 和高级效果列入后续版本；不在入门示例中引用尚未定义的高层 API。

## 2. 功能分解

| ID | 功能 | 交付内容 | 验收要点 | 阶段 |
| --- | --- | --- | --- | --- |
| F01 | 模块化构建 | C++20 targets、独立后端开关、样例和测试开关 | 关闭某后端不需要其 SDK；公共头可单独包含 | M1 |
| F02 | 错误与诊断 | Result、错误码、日志回调、对象标签 | 可定位操作与对象；非法输入不静默成功 | M1 |
| F03 | 后端创建 | 显式选择、Auto、实际后端查询 | 区分未编译和运行环境不可用；显式选择不回退 | M1/M2 |
| F04 | 设备与能力 | 设备信息、格式支持、对齐与上限查询 | 所有限制与实际设备一致；超限创建返回错误 | M1/M2 |
| F05 | 窗口与交换链 | 原生窗口接入、获取、呈现、缩放、暂停 | 连续缩放与最小化恢复；窗口由宿主销毁 | M2 |
| F06 | 资源与上传 | Buffer、2D Texture、Sampler、复制式初始化 | 输入返回后可释放；越界与用途不匹配可诊断 | M2 |
| F07 | Shader 与管线 | 离线产物、布局校验、GraphicsPipeline | 同一材质语义在三后端一致；不匹配提前报错 | M2 |
| F08 | 资源绑定 | 不可变 BindingLayout / BindingSet | 类型、数量、偏移与对齐校验；无隐式覆盖 | M2 |
| F09 | 命令与同步 | 单图形队列、命令录制、提交、完成查询 | 状态机正确；在途资源不提前释放 | M1/M2 |
| F10 | 基础绘制 | 清屏、非索引/索引绘制、深度、视口 | 统一 Shader / 资源描述运行共同样例 | M2/M3 |
| F11 | OpenGL 后端 | CPU 命令流、上下文和状态映射，已接入无窗口缓冲、清屏、非索引三角形和同步读回 | 通过全部适用的 v0.1 用例，不泄露 GL 类型 | M3 |
| F12 | Vulkan 后端 | 资源、提交、交换链、同步实现 | 启用验证时无未解释的错误 | M2 |
| F13 | DX12 后端 | 资源、描述符、提交、交换链实现 | 启用调试层时无未解释的错误 | M3 |
| F14 | 生命周期恢复 | 延迟回收、超时、失效句柄、设备丢失 | 能停止提交并清理；不无限重试失败设备 | M1–M3 |
| F15 | 用户接入 | 可构建样例、接口说明、环境与排错文档 | 新环境按说明完成构建和绘制 | M4 |
| F16 | 自动化质量检查 | 格式、完整括号、CTest/GoogleTest、CPU/Null/GPU 分层与 CI | 零测试报错；跳过单列；必需后端有结果与报告 | M1–M4 |
| F17 | Windows/Linux 支持 | 平台私有实现、Win32/Xlib/Wayland 接入、两平台构建测试 | 公共层无平台泄漏；完整目标矩阵逐组合验收；Linux DX12 明确拒绝 | M1–M4 |
| F18 | 可安装 C++ SDK | 静态库、公共头、导出目标、Config 包和安装样例 | 迁移安装前缀后独立工程 find_package 构建运行，不依赖原仓库或开发工具 | M1/M4 |

M1 的 Null 可验证 F03 的注册与选择逻辑，真实设备检测留到 M2；不能将模拟结果算作设备支持。

当前 F02 交付为 Error/Result/LogSink、21 项单元测试、独立头编译及两个集成验证；对象标签与渲染边界错误转换随 RHI/Runtime 实现。设计依据见 [F02 Core 规格](features/core-foundation.md)。

当前 F03/F04/F06/F09/F14 的 Buffer/Null 子集已实现，拥有实际字节复制和同步完成语义。Vulkan 离屏路径已覆盖实际 fence 和在途销毁，超时重试由可控完成源验证，真实设备丢失仍待专项验收。 [离屏异步提交规格](features/rhi-async-submit.md)F18 接入验收与后端验收分开。F12 首批无窗口 Vulkan 1.2 缓冲、屏障、队列/fence 与同步读回已实现，见 [功能规格](features/vulkan-buffers.md) 与 [使用说明](vulkan-backend.md)；完整 M2 仍未完成。

## 3. v0.1 限定范围

- 一个 Runtime 管理一个设备和一个窗口交换链；不使用全局唯一设备，多个实例必须隔离。
- 只保证一个图形队列、一张已获取的帧图像和单渲染线程；CPU/GPU 可有多次在途提交，但重用前必须检查完成标记。
- Buffer 支持顶点、索引、常量和复制用途；Texture 支持单采样、单 mip、单层 2D 颜色/深度资源。更复杂子资源在后续版本单独设计。
- 栅格管线支持顶点/片元阶段、三角形列表、单颜色附件和可选深度附件；BindingSet 为单组绑定，不含数组和动态偏移。
- 不包含 Compute、异步计算、光追、Mesh Shader、Bindless、MSAA、HDR、Texture 读回或跨设备共享；Buffer 同步读回已纳入最小数据路径。
- 不提供 GPU 资源自动迁移或后端热切换；重建设备意味着重新上传资源。
- 不共享宿主任意操作的 OpenGL 上下文；引擎拥有其上下文与状态。

这些限制用于收敛第一版验证规模。后续扩展必须以能力查询和新描述扩展，不允许悄悄改变既有默认行为。

## 4. 后续功能边界

| 计划项 | 前置条件 | 暂不承诺的内容 |
| --- | --- | --- |
| 高层 Renderer | v0.1 RHI 通过三后端验证 | 场景对象格式与对外函数尚未冻结 |
| Render Graph | 有至少两类真实资源依赖的渲染 Pass | 暂不公开 Graph 数据结构 |
| PBR / 阴影 / 后处理 | 材质与 Renderer 规格完成 | 不预先创建效果继承体系 |
| 多窗口 / 多线程录制 | 单窗口生命周期稳定 | 不能假设目前 API 线程安全 |
| 动态插件 / C ABI | C++ API 稳定且存在部署需求 | 不承诺当前 C++ ABI 跨工具链兼容 |

## 5. 里程碑退出条件

- M1：最小契约冻结，Windows/Linux 的 Null 通过句柄、错误、命令状态和生命周期验证；每个 M1 API 有真实实现；两平台自动格式/括号检查与 CPU/Null 测试进入统一命令和 CI。
- M2：Vulkan 在 Windows/Win32、Linux/X11、Linux/Wayland 完成清屏、三角形、纹理与深度绘制；通过缩放、最小化、取消帧与在途销毁；冻结资源与命令契约。
- M3：Windows DX12 和两平台各 WSI 的 OpenGL 运行相同场景，通过一致性和同步验证；必要接口调整在发布前集中完成。
- M4：提供真实环境支持表、编译命令、可运行样例、排错步骤和发布记录；所有 v0.1 功能有证据后才标记发布。

每个里程碑的测试要求见 [验证与验收](validation.md)。交付时间不在架构文档中预估。

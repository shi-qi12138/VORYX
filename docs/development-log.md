# 项目开发日记

更新于 2026-11-27，当前契约版本为 `0.1.0-draft.25`。

本页记录阶段成果、工程决策、验证与遗留事项。当前进度集中见进度看板，字段和兼容性变化见变更记录。 [开发进度](project-status.md) · [变更记录](changelog.md)

## 2026-11-27 架构审查修复

- Runtime 实现离屏自动选择。在只编入 OpenGL 时可以默认创建设备。零采样器与布局能力返回 `Unsupported`。 [设备工厂](rhi-buffer-api.md) · [绑定接口](rhi-binding-api.md)
- 把窗口帧入口从固定布局绘制接口中独立出来。Vulkan 旧交换链退休与新图像获取共用预算，超时保留原生对象以便重试。 [窗口帧](window-frame-api.md)
- 安装消费测试采用隔离的新目录。再次模拟漏装 Shader 头文件，旧目录中存在的副本已无法让漏装构建通过。 [SDK 验收](sdk-integration.md)

## 2026-11-27 OpenGL 提交整理

- 清屏和三角形样例共用软件驱动参数、设备创建及错误诊断。录制及像素验证仍在各自样例中，安装规则将共用头复制到各示例目录。
- 开发进度保留当前测试快照，历史矩阵由环境报告维护。 [开发进度](project-status.md) · [环境报告](environment.md)

## 2026-11-27 OpenGL 离屏三角形

- 扩展公共管线描述，OpenGL 编译 GLSL，Vulkan 消费 SPIR-V。共享 RHI 验证载荷、录制与资源寿命，OpenGL 私有实现管理程序与每次提交的顶点数组。 [功能规格](features/opengl-draw.md)
- 原生绘制用例检查顶点偏移、首顶点、视口、裁剪、清屏状态和 Shader 失败恢复。独立 SDK 示例检查上下方向及三角形内外颜色。Windows 硬件与 WSL 软件 OpenGL 各通过 106 项，Windows Vulkan 回归通过 120 项。 [验证报告](environment.md#26-opengl-离屏三角形证据)
- OpenGL 下一步先接入 Win32 Presenter，并验证清屏、呈现、缩放与恢复。 [开发进度](project-status.md)

## 2026-11-27 OpenGL 离屏清屏与读回

- 在 OpenGL 私有后端实现原生 RGBA8 纹理、帧缓冲和读回。沿用 RHI 的句柄、用途、Pass 和同步提交契约。 [功能规格](features/opengl-clear.md)
- 安装包增加独立 `opengl_clear`，从迁移前缀链接 SDK 并校验 4096 个红色像素。Windows 硬件与 WSL 软件 OpenGL 各通过 101 项，Windows Vulkan Debug 回归通过 118 项。 [环境报告](environment.md#25-opengl-离屏清屏与读回证据)
- 下一批接入 OpenGL Shader 和顶点输入，用离屏三角形验证绘制和像素坐标，再进入窗口呈现。 [开发进度](project-status.md)

## 2026-11-26 OpenGL 无窗口缓冲

涉及契约版本 `draft.22` 和验证 T53。

- 通过 `CreateDevice` 显式选择 OpenGL 3.3 Core。Windows WGL 与 Linux 无窗口 EGL 位于独立私有文件，共用缓冲实现复用现有 RHI 校验。
- 增加 OpenGL 可选构建、共享缓冲契约、上下文隔离测试及迁移后的 SDK 验证。 [OpenGL 指南](opengl-backend.md)
- Windows 硬件 OpenGL 与 WSL 无窗口软件 OpenGL 各通过 95 项，Windows Vulkan Debug 通过 118 项。 [验证报告](environment.md#24-opengl-无窗口缓冲证据)
- OpenGL 窗口清屏、三角形、原生 Linux Vulkan 硬件验收继续推进。 [开发进度](project-status.md)

## 2026-11-26 CI 配置与 Release 验收

- 添加 GitHub Actions 的 Windows/Linux CPU 矩阵与可按预设运行的 Vulkan 硬件任务。统一构建入口保存测试计划并校验结果完整性。 [持续集成指南](ci.md)
- Windows 与 Linux 的四个 CPU 配置各通过 72 项。Win32 硬件和 WSLg X11、原生 Wayland 软件 Vulkan Release 各通过 118 项，覆盖安装后的独立消费。 [环境报告](environment.md#23-ci-配置与本地-release-验收证据)
- 首次远端执行和原生 Linux 硬件证据待接入相应环境。

## 2026-11-26 Vulkan 窗口限时关闭

涉及版本为 `draft.21`，对应 F05/F09/F14/F18 和 T52。

- 新增可重试的 `IPresenter::Shutdown`。共享 RHI 停止新工作并等待提交，Vulkan Presenter 用剩余预算等待图像获取与呈现 fence，然后释放原生设备和表面。 [窗口调用指南](window-frame-api.md#显式限时关闭)
- 三种 WSI 的样例覆盖两次尺寸变化，在每次重建后直接通过设备提交和呈现。关闭验证包含此前已呈现的帧与一个已提交但未呈现的帧。
- Windows 硬件 Vulkan、WSLg X11 与原生 Wayland 软件 Vulkan 各通过 118/118 项，包括迁移安装 SDK 验收。 [环境报告](environment.md#22-vulkan-窗口限时关闭与连续重建证据)
- 接下来补齐远程 CI、原生 Linux Vulkan 硬件与 Release 验收矩阵。 [开发进度](project-status.md)

## 2026-11-26 Vulkan 窗口双帧异步提交

涉及版本为 `draft.20`，对应 F05/F09/F14 和 T51。

- 在已有的活动帧接口上开放 Async 模式。共享 RHI 保存 GPU 命令与资源引用，Presenter 按两个帧槽轮换呈现信号量和 fence。
- 使用固定尺寸下的连续呈现验证帧槽复用。现有缩放和析构路径先回收借用交换链视图的命令，再释放呈现资源。 [窗口帧调用指南](window-frame-api.md)
- Windows 硬件 Vulkan、WSLg X11 和原生 Wayland 软件 Vulkan 各通过 117/117，三种窗口均通过迁移安装 SDK 验收。 [测试证据](environment.md#21-vulkan-窗口双帧提交证据)
- 下一批实现限时关闭和超时重试，并专项验证交换链重建与在途退役。 [开发进度](project-status.md)

## 2026-11-24 Vulkan 离屏异步提交

涉及版本为 `draft.19`，对应 F09/F14 和 T49/T50。

- 提交增加异步模式和设备在途容量，调用方可继续录制或提交后续离屏工作，再根据令牌查询、限时等待或读回。
- RHI 共享层管理在途顺序及资源生命周期，Vulkan 后端持有原生命令和 fence。迁移安装测试验证提前释放命令和绘制句柄后仍得到正确像素。 [调用文档](rhi-async-submit.md) · [环境报告](environment.md)
- Windows 硬件 Vulkan、WSLg X11 与原生 Wayland 软件 Vulkan 各通过 117/117 项。Windows 硬件的异步复制和提前释放用例另连续通过 20 次。
- 下一阶段接入窗口多帧在途，并补齐显式限时关闭与设备丢失验证。

## 2026-11-23 设备绘制接入窗口帧

涉及版本为 `draft.18`，对应 T47/T48。

- 从 Presenter 借用共享设备。AcquireFrame 暴露实际颜色格式、像素尺寸与安全的临时颜色句柄，Submit 校验同一帧和最终 Present 用途。
- 支持同步 Present 和 CancelFrame。生命周期校验阻止无效提交、引用中取消、重复使用窗口图像与不匹配的提交。
- 在三个现有窗口和迁移安装示例中使用公开设备命令执行带纹理、常量、索引和深度的绘制。后续推进异步帧、在途回收与关闭。 [窗口帧指南](window-frame-api.md) · [环境报告](environment.md)
- Windows 硬件 Vulkan、WSLg X11 与原生 Wayland 软件 Vulkan 的最终 Debug 回归各通过 114/114 项。

## 2026-11-23 设备级采样资源与绑定

涉及版本为 `draft.17`，对应 F06/F07/F08 和 T45/T46。

- 交付设备纹理上传、采样器、显式布局、绑定集及 SetBindingSet。绑定集引用现有常量缓冲范围。
- 共享层校验 SPIR-V 声明、范围、资源寿命和提交状态。Vulkan 复用窗口路径的纹理上传与描述符更新。
- 迁移安装后的独立程序执行同一 Pass 内的两次索引绘制，通过像素验证纹理、常量变换和深度遮挡。后续推进窗口帧流程。 [设备绑定](rhi-binding-api.md) · [环境报告](environment.md)
- Windows 硬件 Vulkan、WSLg X11 与原生 Wayland 软件 Vulkan 的最终 Debug 回归各通过 112/112 项。

## 2026-11-23 设备级离屏绘制

涉及版本为 `draft.16`，对应 F06/F09/F10/F14 和 T43/T44。

- 完成设备纹理附件、显式纹理屏障、Pass、管线与顶点/索引绑定、视口与裁剪、Draw/DrawIndexed，以及同步 RGBA8 读回。
- 命令持有管线、缓冲和附件。录制失败保留原状态，整批提交先完成逻辑校验，再执行后端操作。
- Vulkan 与窗口绘制共用 RenderPass 创建逻辑，补齐连续 Pass 的附件同步。独立安装程序通过公开 API 完成索引绘制与读回。
- 新增六项 Null、八项 Vulkan 契约与像素用例，覆盖失败恢复、资源引用、两种索引宽度、起点、顶点偏移、裁剪和跨提交深度复用。
- 最终 Windows 硬件 Vulkan、WSLg X11 与原生 Wayland 软件 Vulkan 三个 Debug 配置各通过 108/108 项，失败和跳过为零。

后续接入设备采样资源和绑定，以及窗口帧流程。验证证据见环境报告第 17 节。 [接口指南](rhi-draw-api.md) · [功能规格](features/rhi-draw.md) · [环境报告](environment.md)

## 2026-11-23 图形管线首批与文档整理

涉及版本为 `draft.15`，对应 F07/F10 和 T42。

- 完成设备级管线创建和销毁、强类型句柄、显式顶点布局以及剔除、混合和深度状态。
- Null/Vulkan 共用输入、线程、归属和代数校验。Vulkan 管线独立于窗口和几何，现有 Presenter 绘制复用该实现。
- 补充管线接口指南、功能规格、共享契约、像素验收与独立安装消费。新增销毁重载，空句柄调用须写清具体类型，宿主随静态 SDK 重编译。
- Windows 硬件 Vulkan、WSLg X11 软件 Vulkan、原生 Wayland 软件 Vulkan 三个 Debug 配置各通过 94/94 项，失败和跳过为零。像素用例检查带填充的顶点、同管线两份几何、绕序剔除、Alpha 与预乘混合。
- 整理中文开发日记、开发进度和模块说明，关联根目录与文档中心的入口，并补充后续维护规则。

后续接入设备级 Pass 和绘制命令。本批设备管线接收无资源绑定 SPIR-V，设备绑定及绘制命令继续推进。原生 Linux 硬件 GPU 仍待验收。 [管线指南](rhi-pipeline-api.md) · [功能规格](features/rhi-pipeline.md) · [环境报告第 16 节](environment.md)

## 2026-11-09 可复用绑定与显式资源布局

涉及版本为 `draft.13` 至 `draft.14`，对应 F08、T40 和 T41。

- 分离纹理、采样器与不可变纹理绑定，允许同一纹理搭配不同采样器，提供 ResetDraw 和按依赖销毁检查。
- 新增常量缓冲、显式绑定布局和绑定集，一次绘制可同时读取变换参数、纹理和独立采样器。
- 布局采用 set 0，允许 1 至 8 个稀疏槽位，每槽声明类型与顶点或片元可见性。
- 三种窗口及安装样例加入 `--binding` 和 `--resources`。纹理绑定阶段三个 Debug 配置各通过 82/82 项，显式布局阶段各通过 83/83 项。

设备级绑定与通用绘制的接入继续推进，动态更新、多绑定集及更复杂组合仍待实现。 [纹理绑定](vulkan-bindings.md) · [显式布局](vulkan-binding-layout.md) · [环境报告第 14、15 节](environment.md)

## 2026-11-06 窗口接入与基础绘制

涉及版本为 `draft.6` 至 `draft.12`。

- 完成 Win32、X11 和原生 Wayland 接入，共用 Vulkan 交换链与呈现实现，覆盖清屏、缩放、暂停和恢复。
- 为 Visual Studio 生成器补充括号检查所需的辅助编译数据库。
- 逐步交付彩色三角形、RGBA8 纹理上传与采样、深度遮挡、uint32 索引绘制，并同步接口、样例和安装消费测试。
- Shader 夹具由离线工具生成，窗口样例和像素测试共用资产与原生绘制实现。

| 交付批次 | 当时验证结果 |
| --- | --- |
| Win32 清屏 | Windows Debug/Release 各 77/77。Linux 无窗口软件 Debug 回归 76/76 |
| X11 清屏 | WSLg X11 软件 Debug 77/77 |
| Wayland 清屏 | WSLg 原生 Wayland 软件 Debug 77/77。Windows Debug 回归 77/77，X11 窗口专项 1/1 |
| 彩色三角形 | Windows、WSLg X11、原生 Wayland 三个 Debug 配置各 78/78 |
| 纹理采样 | 同上，各 79/79 |
| 深度测试 | 同上，各 80/80 |
| 索引绘制 | 同上，各 81/81 |

各阶段的像素和窗口场景、环境及限制保存在专项证据中。后续逐步抽出可复用资源与独立管线。 [版本记录](changelog.md) · [环境报告第 7 至 13 节](environment.md)

## 2026-11-05 Vulkan 无窗口缓冲

涉及版本为 `draft.5`，对应 F12 首批与 T32。

- 实现 Vulkan 设备、原生缓冲、状态屏障、队列和 fence，同步执行上传、复制与读回。
- 提取 RHI 共享句柄、引用、状态机和整批提交校验，由 Null/Vulkan 分别实现存储和执行。
- 设备描述增加验证开关、显式软件设备选项和日志回调，安装后的独立程序也能选择真实 Vulkan。
- Windows 硬件 Debug/Release 和 WSL 软件 Debug/Release 四个配置各通过 76/76 项。20 项缓冲契约在 Null/Vulkan 间复用。

软件执行与硬件验收分别记录。窗口和绘制在随后批次实现，真实 DeviceLost 故障恢复仍需专项验证。 [Vulkan 缓冲规格](features/vulkan-buffers.md) · [环境报告第 6 节](environment.md)

## 2026-11-04 工程基础、Core、Null 与 SDK

涉及版本为 `draft.1` 至 `draft.4`。

- 建立架构、模块依赖、功能规格、目录、接口、代码样式、跨平台与自动化测试约束，采用 C++20 和 CMake。
- 固定 GoogleTest 版本及归档校验，建立 Windows 编译环境初始化、两平台构建入口、格式与完整括号检查。
- 完成 Error、Result 和日志，建立 21 项 Core 单元测试、独立头编译和消费验证。Core 批次的默认四配置各通过 29 项。
- 实现 Null 字节存储、缓冲操作、命令状态、提交标记、线程与引用检查，提供低层设备工厂。
- 完成公共头与静态库安装、导出目标、find_package 和迁移安装前缀的独立用户工程。Null/SDK 批次的 Windows/Linux Debug/Release 四配置各通过 55/55 项。

真实图形后端从后续 Vulkan 批次开始验证。 [Core 规格](features/core-foundation.md) · [Null 缓冲规格](features/null-buffer-rhi.md) · [SDK 规格](features/sdk-package.md) · [历史版本](changelog.md)

## 后续记录方式

每次完成一批功能后在顶部追加有实际日期的条目，保留历史记录所属的版本与测试环境。建议依次记录交付内容、接口或工程决策、验证结果、遗留事项和下一步。

测试结果须写明配置、通过、失败、跳过或未执行，并链接环境报告。接口变化同时更新变更记录，功能推进同时更新开发进度。模块职责或目标归属变化再更新模块说明。 [维护规范](development.md) · [模块说明](module-map.md)

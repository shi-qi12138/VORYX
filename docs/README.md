# 文档中心

文档版本为 0.1.0-draft.24，最后更新于 2026-11-27。项目阶段：Core、缓冲 RHI、Null/Vulkan 缓冲后端和设备工厂已落地，提供可安装 C++ SDK；Win32、X11 与 Wayland 已实现 Vulkan 清屏和固定管线彩色、纹理三角形、深度测试、索引绘制、可复用纹理绑定和显式常量缓冲布局，设备级图形管线、显式顶点布局和离屏 Pass 绘制首批已实现，设备绘制已接入资源绑定、同步窗口帧、窗口双帧异步与 Vulkan 离屏异步提交。Vulkan 窗口已提供显式限时关闭。OpenGL 已接入无窗口缓冲、离屏三角形与像素读回，完整 Runtime 继续推进。接入见 [SDK 文档](sdk-integration.md)。

## 开发记录与当前状态

| 需要查看 | 文档 |
| --- | --- |
| 当前交付、里程碑缺口和后续顺序 | [开发进度](project-status.md) |
| 按日期回顾完成内容、决策和验证 | [开发日记](development-log.md) |
| 模块职责、源码位置、依赖与工程目标 | [模块说明](module-map.md) |

## 阅读顺序

使用者：[功能规格](requirements.md) → [接入指南](getting-started.md) → [接口契约](api-reference.md)。

开发者：[架构](architecture.md) → [设计决策](decisions.md) → [功能规格](requirements.md) → [开发规范](development.md) → [验证与验收](validation.md)。

新增功能先复制 [功能设计模板](templates/feature.md)，评审后再进入实现。

## 文档职责与权威来源

| 文档 | 唯一维护的内容 |
| --- | --- |
| [project-status.md](project-status.md) | 当前进度、交付状态、里程碑缺口和后续顺序 |
| [development-log.md](development-log.md) | 按交付日期维护的开发日记与历史依据 |
| [module-map.md](module-map.md) | 实际源码、模块职责和构建目标的对应关系 |
| [environment.md](environment.md) | 已验证环境、可执行构建命令、依赖锁定和报告路径 |
| [architecture.md](architecture.md) | 模块职责、依赖方向、所有权与线程原则 |
| [cross-platform.md](cross-platform.md) | 操作系统/窗口系统/后端矩阵、平台边界及分平台验收 |
| [repository-layout.md](repository-layout.md) | 目录、文件归属、命名与构建 target 组织 |
| [code-style.md](code-style.md) | 根格式配置使用、完整括号与代码职责规则 |
| [requirements.md](requirements.md) | 功能 ID、版本范围、验收目标、开发顺序 |
| [api-reference.md](api-reference.md) | API 名称、字段、默认值、错误、状态机与调用契约 |
| [core-api.md](core-api.md) | 已实现 Core 的准确调用、类型约束、引用与日志规则 |
| [window-frame-api.md](window-frame-api.md) | 设备绘制直接呈现、双帧异步、借用颜色附件、帧与提交令牌、取消恢复与限时关闭 |
| [rhi-binding-api.md](rhi-binding-api.md) | 设备纹理上传、常量范围、资源布局、集合与生命周期 |
| [rhi-draw-api.md](rhi-draw-api.md) | 设备离屏附件、Pass、绘制命令与同步读回 |
| [rhi-pipeline-api.md](rhi-pipeline-api.md) | 设备级管线、显式顶点布局、生命周期与当前范围 |
| [rhi-buffer-api.md](rhi-buffer-api.md) | 已实现缓冲设备、句柄、命令与提交契约 |
| [rhi-async-submit.md](rhi-async-submit.md) | Vulkan 离屏异步提交、查询、超时与资源回收 |
| [vulkan-binding-layout.md](vulkan-binding-layout.md) | 显式多槽布局、常量缓冲和纹理组合 |
| [vulkan-bindings.md](vulkan-bindings.md) | 独立纹理/采样器、不可变绑定、句柄归属与销毁 |
| [vulkan-indexed.md](vulkan-indexed.md) | 共享顶点、索引缓冲、越界处理和像素验收 |
| [vulkan-depth.md](vulkan-depth.md) | 深度顶点、遮挡规则、尺寸变化和像素验收 |
| [vulkan-texture.md](vulkan-texture.md) | RGBA8 纹理上传、行距、UV、采样和绑定契约 |
| [vulkan-triangle.md](vulkan-triangle.md) | 固定布局三角形 API、Shader 资产与窗口运行步骤 |
| [wayland-clear.md](wayland-clear.md) | 原生 Wayland 接入、configure、像素尺寸与暂停恢复 |
| [xlib-clear.md](xlib-clear.md) | Linux X11 宿主窗口接入、清屏与恢复 |
| [win32-clear.md](win32-clear.md) | Win32 宿主窗口接入、清屏、恢复与呈现契约 |
| [vulkan-backend.md](vulkan-backend.md) | Vulkan 构建、设备选择、验证层、调用与当前限制 |
| [opengl-backend.md](opengl-backend.md) | OpenGL 无窗口上下文、缓冲调用、离屏清屏、三角形和 SDK 配置 |
| [OpenGL 清屏规格](features/opengl-clear.md) | 附件格式、同步提交、读回和验收条件 |
| [OpenGL 绘制规格](features/opengl-draw.md) | GLSL 阶段对、顶点布局、裁剪坐标与验收 |
| [sdk-integration.md](sdk-integration.md) | 安装导出、独立 find_package 接入、迁移与消费验收 |
| [getting-started.md](getting-started.md) | 使用步骤、典型场景和故障处理；引用 API 定义 |
| [development.md](development.md) | 开发流程、编码与依赖规则、文档同步要求 |
| [validation.md](validation.md) | 测试场景、证据要求和发布门槛 |
| [automated-testing.md](automated-testing.md) | CTest/GoogleTest、套件、过滤、CI 和报告方案 |
| [ci.md](ci.md) | GitHub Actions、CPU 矩阵、GPU 执行器配置与归档 |
| [decisions.md](decisions.md) | 决策原因、适用范围、尚未解决的问题 |
| [changelog.md](changelog.md) | 文档与契约变更记录 |

接口细节以接口契约为准，范围以功能规格为准，依赖关系以架构为准。发现冲突必须修正文档，不允许由实现者自行猜测后继续扩散。

根 [AGENTS.md](../AGENTS.md) 将这些边界约束提供给后续编码代理；实际格式以根 [.clang-format](../.clang-format) 为唯一来源，完整括号检查由 [.clang-tidy](../.clang-tidy) 补充。

## 状态用语

- 草案：供实现前评审，可变更，但要同步受影响文档。
- 已冻结：确定了当前版本的契约，变更必须记录兼容影响。
- 已实现：存在实现和对应验证证据；不能仅凭声明或空实现标记。
- 已发布：有版本、支持环境、可构建示例和发布验证记录。

Core 和缓冲 RHI/Null/Vulkan 已实现；SDK 接入、CPU 契约与完整图形后端验收分开。窗口绘制以各已实现指南为准。完整设备与命令接口的设计规格另见接口契约。

## 需求到实现的追踪

一个可交付功能必须能沿以下路径追踪：功能 ID → 接口契约 → 用户调用流程 → 测试用例 → 实现与验收证据。不能只有函数声明，也不能只有功能描述。

例如 F05 对应 API 的 Runtime / 帧状态、指南的清屏与缩放步骤、验证 T15–T17；F09 对应命令状态与提交、指南的每帧重用与错误出口、验证 T06/T09/T18/T19。新增功能使用相同方式维护。

## 实现前与发布前的区别

本文档先定义用户应看到的行为。开发阶段可以维护未实现接口，但发布包只能公开实际可用且有说明的能力。

已验证工具链、Vulkan 缓冲路径与实际命令见环境报告；最低支持范围、完整窗口后端与发布驱动矩阵仍待验收。发布前不得用设计目标代替支持证据。

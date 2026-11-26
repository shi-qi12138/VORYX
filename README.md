# VORYX

VORYX 是使用 C++20 开发的可嵌入渲染引擎，目标图形后端为 OpenGL、Vulkan 和 Direct3D 12。

首版目标为 Windows 与 Linux：两平台使用 Vulkan/OpenGL，Windows 另提供 DX12；Linux 覆盖 X11 与原生 Wayland。当前提供无窗口 Vulkan 缓冲，以及 Win32、X11 和 Wayland 的清屏、三角形、深度、索引和显式资源绑定示例。Null 用于 CPU 契约测试。OpenGL 与 DX12 后端处于设计阶段。 [跨平台设计](docs/cross-platform.md)

项目主体是可被其他工程接入的 C++ 渲染引擎 SDK，支持安装及 find_package 接入。Win32、X11 与 Wayland 窗口可通过公共 API 清屏、纹理三角形和深度测试。已提供固定彩色布局的索引绘制。可复用纹理绑定、显式多槽布局和设备级管线已交付首批能力，设备离屏 Pass 和绘制命令已交付首批能力，设备绘制支持采样资源和常量缓冲绑定，并可通过 Presenter 输出到窗口。 [窗口帧](docs/window-frame-api.md)命令见 [环境搭建](docs/environment.md)。

## 文档入口

完整目录、阅读顺序和文档状态见 [文档中心](docs/README.md)。

| 需要做什么 | 阅读文档 |
| --- | --- |
| 查看已完成内容、里程碑与下一步 | [开发进度](docs/project-status.md) |
| 按日期回顾开发过程 | [中文开发日记](docs/development-log.md) |
| 了解模块职责与各工程目标 | [模块说明](docs/module-map.md) |
| 配置、构建和运行环境测试 | [环境搭建](docs/environment.md) |
| 使用已实现的错误结果和日志 | [Core API](docs/core-api.md) |
| 在其他工程中接入安装后的库 | [SDK 接入](docs/sdk-integration.md) |
| 将 Vulkan 接入宿主窗口 | [Win32 清屏指南](docs/win32-clear.md) / [X11 清屏指南](docs/xlib-clear.md) / [Wayland 清屏指南](docs/wayland-clear.md) |
| 创建真实 Vulkan 设备与运行缓冲复制 | [Vulkan 后端](docs/vulkan-backend.md) |
| 用设备命令绘制和呈现窗口帧，包括双帧异步和限时关闭 | [窗口帧 API](docs/window-frame-api.md) |
| 异步提交离屏任务，等待并读回结果 | [Vulkan 离屏异步提交](docs/rhi-async-submit.md) |
| 在设备绘制中绑定采样资源和常量缓冲 | [RHI 设备资源绑定](docs/rhi-binding-api.md) |
| 录制设备绘制并读回附件 | [RHI 设备绘制](docs/rhi-draw-api.md) |
| 创建设备级管线与显式顶点布局 | [RHI 管线 API](docs/rhi-pipeline-api.md) |
| 一次绘制绑定常量缓冲、纹理和采样器 | [Vulkan 显式绑定布局](docs/vulkan-binding-layout.md) |
| 复用同一张纹理和采样器 | [Vulkan 纹理绑定](docs/vulkan-bindings.md) |
| 用索引复用顶点 | [Vulkan 索引绘制](docs/vulkan-indexed.md) |
| 验证近处三角形遮挡远处 | [Vulkan 深度测试](docs/vulkan-depth.md) |
| 上传 RGBA8 图像并采样 | [Vulkan 纹理](docs/vulkan-texture.md) |
| 通过公共 API 绘制第一个三角形 | [Vulkan 三角形](docs/vulkan-triangle.md) |
| 使用已实现的设备、缓冲和命令 | [缓冲 RHI API](docs/rhi-buffer-api.md) |
| 了解框架和模块耦合 | [架构设计](docs/architecture.md) |
| 确认平台、窗口系统与后端组合 | [跨平台设计](docs/cross-platform.md) |
| 查看目录、文件与构建边界 | [目录规范](docs/repository-layout.md) |
| 确认括号、空格与代码组织要求 | [代码规范](docs/code-style.md) |
| 确认功能范围与交付条件 | [功能规格](docs/requirements.md) |
| 接入引擎、绘制和处理错误 | [接入指南](docs/getting-started.md) |
| 查询参数、返回值及调用限制 | [接口契约](docs/api-reference.md) |
| 开发功能、增加后端、修改接口 | [开发规范](docs/development.md) |
| 判断功能是否开发完成 | [验证与验收](docs/validation.md) |
| 了解自动化测试与 CI 要求 | [自动化测试](docs/automated-testing.md) · [持续集成](docs/ci.md) |
| 理解已定决策和待定事项 | [设计决策](docs/decisions.md) |

Core、缓冲 SDK 与 Vulkan 窗口绘制已有可运行入口和对应接口指南。通用设备绘制流程按进度看板继续实现。自有代码说明性注释统一使用中文。

## 基本约束

- C++ 是主要开发语言，对外优先提供 C++ API。
- 统一渲染接口采用显式资源与命令提交模型。
- 图形 API 类型及头文件只出现在对应后端内。
- 后端选择集中在组装层，渲染算法不判断当前使用哪一种后端。
- 首期采用编译期可选后端；运行时动态插件与跨语言绑定留待有明确需求后设计。
- 实现开始前，以架构文档中的模块依赖、所有权和线程规则为约束。
- 自有 C++ 遵守根 `.clang-format`，控制语句不得省略 `{}`；根 `.clang-tidy` 补充括号检查。
- 自动化测试按 CTest + GoogleTest 方案落地；未实现、未执行与跳过不能算通过。

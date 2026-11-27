# 项目开发进度

更新日期为 2026-11-27，当前契约版本为 `0.1.0-draft.24`。

## 当前阶段

M2 的 Vulkan 已接入 Win32、X11 和原生 Wayland，原生 Linux 硬件验收仍在推进。M3 的 OpenGL 已交付无窗口缓冲、清屏、三角形与读回。两种后端均可随 C++ SDK 安装，并通过独立工程接入。

本页集中维护当前状态和后续顺序。需求与里程碑的验收条件见功能规格，模块职责见架构，历史交付见开发日记，测试原始证据见环境报告。 [功能规格](requirements.md) · [架构](architecture.md) · [开发日记](development-log.md) · [环境报告](environment.md)

## 已交付能力

| 功能范围 | 当前交付 | 接口和使用说明 |
| --- | --- | --- |
| 工程基础 | C++20、CMake 预设、Windows 编译环境初始化、Linux 构建入口、依赖锁定 | [环境搭建](environment.md) |
| Core | 错误码、错误信息、Result、日志回调与所有权约束 | [Core 接口](core-api.md) |
| 缓冲 RHI | 设备、缓冲、屏障、复制、提交、等待、同步读回，句柄归属与生命周期校验 | [缓冲接口](rhi-buffer-api.md) |
| Null 后端 | CPU 字节存储与复制，命令契约验证，管线描述副本和生命周期 | [Null 功能规格](features/null-buffer-rhi.md) · [管线接口](rhi-pipeline-api.md) |
| Vulkan 缓冲 | 原生设备、缓冲、队列、屏障、同步提交与读回 | [Vulkan 后端](vulkan-backend.md) |
| 窗口接入 | Win32、X11、原生 Wayland 的清屏呈现、缩放与暂停恢复 | [Win32](win32-clear.md) · [X11](xlib-clear.md) · [Wayland](wayland-clear.md) |
| 窗口绘制 | 彩色和纹理三角形、彩色深度测试、彩色索引绘制 | [三角形](vulkan-triangle.md) · [纹理](vulkan-texture.md) · [深度](vulkan-depth.md) · [索引](vulkan-indexed.md) |
| Presenter 资源绑定 | 独立纹理和采样器、不可变绑定、多槽布局、常量缓冲与纹理组合 | [纹理绑定](vulkan-bindings.md) · [显式布局](vulkan-binding-layout.md) |
| 设备级图形管线 | 独立创建和销毁、显式单流顶点布局、剔除、混合、深度状态。旧窗口绘制复用原生实现 | [管线接口](rhi-pipeline-api.md) |
| 设备级绘制 | 离屏颜色和深度附件、显式屏障、Pass、几何绑定、动态状态、两种绘制及读回 | [设备绘制](rhi-draw-api.md) |
| 设备资源绑定 | 不可变采样纹理与采样器、常量缓冲范围、显式布局及集合、Pass 中切换资源 | [设备绑定](rhi-binding-api.md) |
| 设备窗口帧 | 借用窗口设备和颜色图像，录制纹理索引绘制，支持同步提交、双帧异步提交、呈现与取消 | [窗口帧](window-frame-api.md) |
| Vulkan 离屏异步 | 连续提交、fence 查询与等待、资源延迟回收、SDK 纹理绘制读回 | [离屏异步提交](rhi-async-submit.md) |
| 窗口限时关闭 | 停止新工作、按预算等待、超时重试、未呈现帧回收 | [窗口关闭](window-frame-api.md#显式限时关闭) |
| OpenGL 无窗口缓冲 | WGL 和 EGL 私有上下文、原生缓冲和同步复制、迁移 SDK 接入 | [OpenGL 后端](opengl-backend.md) |
| OpenGL 离屏清屏 | RGBA8 颜色附件、同步提交、整图像素读回和独立安装示例 | [清屏与读回](opengl-backend.md#离屏清屏和读回) |
| OpenGL 离屏三角形 | GLSL 管线、单流顶点、动态视口和裁剪、同步读回及独立 SDK 示例 | [绘制指南](opengl-backend.md#离屏三角形) |
| SDK 接入 | 安装公共头和静态库、导出目标、find_package、迁移安装前缀后的独立消费 | [SDK 接入](sdk-integration.md) |
| 自动化验证 | CTest 调度 GoogleTest，环境、Core、Null、Vulkan、OpenGL、窗口、安装、格式和完整括号检查 | [自动化测试](automated-testing.md) |
| 持续集成 | GitHub Actions 四套 CPU 配置首轮通过，Vulkan 硬件工作流等待专用执行器 | [持续集成](ci.md) |

设备绘制支持单组显式资源绑定。应用可在同一个 Pass 通过不同绑定集组合纹理和常量范围。 [设备绑定](rhi-binding-api.md)Presenter 已有独立资源句柄和绘制入口，其纹理、深度、索引与绑定能力的组合范围以各指南为准。 [管线首批范围](rhi-pipeline-api.md)

## 里程碑进展

| 里程碑 | 进展判断 | 已有成果 | 剩余验收 |
| --- | --- | --- | --- |
| M1 基础与契约 | 基础能力已交付，退出条件尚有待办 | Core、Null 缓冲契约、两平台本地验证、格式与括号检查、SDK | 最小契约完整冻结 |
| M2 Vulkan 与通用渲染 | 正在推进 | 三种窗口接入、颜色与纹理、深度、索引、显式绑定、设备管线、离屏绘制与设备绑定 | 原生 Linux GPU 的可追踪验收 |
| M3 三后端一致性 | OpenGL 首批绘制已接入 | Windows 硬件与 WSL 软件 OpenGL 的缓冲、三角形与同步读回 | OpenGL 窗口及完整管线、DX12 实现、共同场景一致性验证 |
| M4 对外发布 | 已完成部分接入准备 | 安装导出、独立用户工程、调用与排错文档 | 完整支持矩阵、发布环境验收、兼容策略与正式发布记录 |

整体进度按上述交付物和验收缺口跟踪。各里程碑的完整退出条件由功能规格维护。 [里程碑定义](requirements.md)

## 最新验证快照

本轮验证 `0.1.0-draft.24` 的 OpenGL 离屏三角形与独立 SDK 接入。Windows 硬件和 WSL 软件 OpenGL Debug 各通过 106 项，Windows Vulkan Debug 回归通过 120 项。 [环境报告第 26 节](environment.md#26-opengl-离屏三角形证据)

| 构建配置 | 执行环境 | 通过 | 失败 | 跳过 |
| --- | --- | --- | --- | --- |
| `windows-opengl-debug` | Windows WGL 硬件 OpenGL | 106/106 | 0 | 0 |
| `linux-opengl-software-debug` | WSL 无窗口 EGL 软件 OpenGL | 106/106 | 0 | 0 |
| `windows-vulkan-debug` | Windows 硬件 Vulkan、Win32 | 120/120 | 0 | 0 |

原生 Linux GPU、OpenGL 窗口呈现和 DX12 后端仍待验收。历史 Linux Vulkan、Release 和 CPU/Null 结果见环境报告，远端执行记录见持续集成指南。 [历次验收](environment.md#23-ci-配置与本地-release-验收证据) · [持续集成](ci.md)

## 后续开发顺序

下表为当前建议顺序，实施时先补齐对应功能规格、接口契约和测试场景，再交付实现。

| 顺序 | 工作内容 | 验收结果 |
| --- | --- | --- |
| 1 | 原生 Linux Vulkan 硬件验收 | X11 与 Wayland 的真机结果可追踪 |
| 2 | OpenGL 窗口呈现，随后补齐管线范围并接入 DX12 | 先接入 Win32 窗口的清屏、呈现和缩放，再验证 X11、Wayland 与窗口三角形 |
| 3 | 发布准备 | 支持表、版本、安装包、迁移说明和全新工程接入一致 |

高层 Renderer、Render Graph、材质与高级效果按已有架构的后续条件推进。 [后续功能边界](requirements.md)

## 更新规则

完成一批功能后，更新本页的交付状态、剩余项和最新验证日期，同时在开发日记追加记录。测试结论须指向环境报告，接口参数须链接实际接口指南。发布判断遵循验证与验收文档。 [开发日记](development-log.md) · [维护规范](development.md) · [验证与验收](validation.md)

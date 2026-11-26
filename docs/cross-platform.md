# 跨平台范围与实现边界

状态为 0.1.0-draft.21。Vulkan 设备绘制支持 Win32、X11 与原生 Wayland 的同步窗口帧、双帧异步与 Presenter 限时关闭，Windows 和 Linux 均支持离屏异步提交。Windows 经硬件 Vulkan 验证，Linux 两种窗口系统以 WSLg 软件 Vulkan 验证，原生 Linux 硬件验证继续推进。 [窗口帧](window-frame-api.md) · [环境报告](environment.md)

## 1. 首版目标矩阵

| 系统 / 窗口系统 | Vulkan | OpenGL | DX12 | Null |
| --- | --- | --- | --- | --- |
| Windows x86-64 / Win32 | 同步与双帧异步 | 计划支持 | 计划支持 | 已实现，无窗口 CPU 验证 |
| Linux x86-64 / X11（Xlib） | 同步与双帧异步，WSLg 软件验收 | 计划支持 | 不纳入支持 | 已实现，当前为 WSL2 CPU 验证 |
| Linux x86-64 / 原生 Wayland | 同步与双帧异步，WSLg 软件验收 | 计划支持 | 不纳入支持 | 已实现，当前为 WSL2 CPU 验证 |

每项计划均须单独验收。Linux 正式支持范围需固定发行版、glibc/标准库、驱动与桌面会话，不能概括为所有 Linux。首版另已提供 Vulkan 无窗口设备与离屏绘制。xcb、ARM64、macOS 和移动平台留待后续。

Auto 在 Windows 按 Vulkan → DX12 → OpenGL，Linux 按 Vulkan → OpenGL。Null 不自动回退。Linux 显式请求 DX12 返回 Unsupported，不暗中引入兼容层。

## 2. 差异归属

操作系统和窗口系统是不同维度。Linux 不能只有一个没有连接信息的 windowId；X11 和 Wayland 分别描述原生对象与生命周期。

| 差异 | 归属 |
| --- | --- |
| 文件、路径、动态库与线程系统调用 | 优先标准 C++，系统调用限制在 Platform 私有实现 |
| 原生窗口描述、连接与物理尺寸 | Platform 公共契约及窗口系统适配 |
| Vulkan Surface / 窗口系统扩展 | Vulkan 后端的私有 WSI 适配 |
| OpenGL 上下文、函数加载与交换缓冲 | OpenGL 后端的私有 WSI 适配 |
| GPU 资源、命令和渲染算法 | 通过 RHI 表达，保持平台无关 |

Core、通用 RHI、Renderer、Render Graph 不包含 Windows、Xlib、Wayland、GLX、WGL、EGL 头文件，不散布平台宏。CMake 选择平台源码；编译条件集中在 Platform、后端适配与组装层。

每个图形后端复用自身对象/命令实现，仅拆分 WSI；不复制成 Windows Vulkan 与 Linux Vulkan 两套引擎。

## 3. 窗口接入

| 平台 | Vulkan | OpenGL |
| --- | --- | --- |
| Windows / Win32 | Win32 Surface | WGL |
| Linux / X11 | Xlib Surface | GLX |
| Linux / Wayland | Wayland Surface | EGL，要求桌面 OpenGL |

EGL 不是用 OpenGL ES 替代桌面 OpenGL；不能创建所需上下文时明确失败。API 基线按 Q02 冻结。

宿主拥有窗口、X11 Display 或 Wayland display/surface 及事件循环，必须存活到引擎释放 Surface/Swapchain/上下文之后。Wayland surface 须完成宿主协议要求的配置；引擎不抢占宿主默认事件循环。当前 Win32/X11 清屏由适配器读取客户区像素尺寸。Wayland 宿主用 SetFramebufferExtent 提交已确认的缓冲像素尺寸。通用帧接口的 Resize 契约另行实现。

各 WSI 首版验收基线为窗口事件与渲染同线程。分离线程必须满足原生连接和宿主窗口库的并发契约，单独验证后才列为支持配置。公共 RHI 的单渲染线程规则不能推导出任意跨线程窗口操作安全。

样例窗口库在 M1 固定；其对象类型不进入公共 API，宿主可接入已有窗口框架。

## 4. 构建与数据规则

- Windows 主工具链候选 MSVC，Linux 候选 GCC，增加 Clang 编译检查；具体版本、标准库与系统基线由 Q01 冻结。
- 使用独立 Windows/Linux presets 和 build 目录，不写死盘符、反斜杠或开发机绝对路径。
- Linux 默认关闭 DX12；显式启用 VORYX_ENABLE_DX12 时 CMake 配置报错，不静默忽略。
- VORYX_ENABLE_X11 控制 Linux Vulkan Xlib 适配器，默认关闭。VORYX_ENABLE_WAYLAND 控制 Linux Vulkan Wayland 适配器，也默认关闭。
- 启用的后端/WSI 缺依赖时配置失败；CPU/Null 配置不要求安装桌面依赖。
- Windows SDK、X11/Wayland、GLX/EGL 依赖进入相应私有 target，不传播到公共头。
- 测试与质量工具采用跨平台入口，PowerShell 只能是平台包装，不是唯一入口。
- 固定宽度整数、显式布局与 UTF-8 文本；不假设 long、指针或宿主结构体跨平台布局相同。
- 文件名与 include 大小写完全一致；原生路径/编码转换在平台边界处理。
- Shader 产物与原生 Pipeline 缓存分离；后者记录系统、设备与驱动，不跨平台盲目复用。

## 5. 测试与交付

M1 即在 Windows/Linux 执行配置、构建、公共头检查和 CPU/Null 测试。M2 两平台验证 Vulkan，Linux 分别覆盖 X11 与原生 Wayland。M3 加入两平台 OpenGL 和 Windows DX12。

完整发布需要七个真实系统/WSI/后端组合的适用测试。CI 无桌面任务只证明 CPU/Null 部分；软件驱动、虚拟显示与 WSL 可作补充，不能替代声明支持的原生 Linux GPU/桌面验收。未测组合标明未验证，不借用其他组合的结果。

## 6. 参考

- [Khronos：Vulkan WSI](https://docs.vulkan.org/guide/latest/wsi.html)
- [Khronos：OpenGL / GLX / WGL](https://registry.khronos.org/OpenGL/index_gl.php)
- [Khronos：EGL](https://www.khronos.org/egl)
- [Microsoft：Direct3D 12](https://learn.microsoft.com/en-us/windows/win32/direct3d12/)

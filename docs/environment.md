# 开发环境搭建与复现

截至 2026-11-27，已落地 Core、缓冲 RHI/Null、低层工厂和可安装 SDK，并新增 Vulkan 真实缓冲后端；Win32、X11 与 Wayland 已可清屏、绘制彩色或纹理三角形、验证深度遮挡和使用索引复用顶点，设备离屏 Pass、Draw/DrawIndexed 和同步读回已交付首批能力，设备级绘制已接入采样纹理、独立采样器和常量缓冲。窗口设备帧支持获取、同步提交、呈现和取消。Vulkan 离屏支持异步提交、完成查询、限时等待和在途回收。Vulkan 窗口已支持显式双帧异步提交。Vulkan 窗口支持可重试的显式限时关闭，GitHub Actions 首次 CPU 矩阵已通过。OpenGL 无窗口设备支持缓冲操作、RGBA8 离屏清屏、GLSL 三角形绘制和同步读回。完整 Runtime 继续推进。 [离屏异步提交](rhi-async-submit.md) [窗口帧](window-frame-api.md)接口见 [缓冲 RHI](rhi-buffer-api.md)，安装见 [SDK 接入](sdk-integration.md)。

## 1. 已验证环境

| 项目 | Windows x64 | Linux x64（Ubuntu WSL2） |
| --- | --- | --- |
| 系统 | 本机 Windows | Ubuntu 26.04 LTS，WSL2 内核 6.6.87.2 |
| C++ 编译器 | MSVC 19.51.36260，工具集目录 14.51.36231 | GCC 15.2.0 |
| CMake / CTest | 4.3.2 | 4.2.3 |
| Ninja | 1.11.1 | 1.13.2 |
| Python | 3.13.2 | 3.14.4 |
| clang-format / clang-tidy | Visual Studio 附带 22.1.3 | Ubuntu 包 21.1.8 |
| GoogleTest | 固定 1.17.0 | 固定 1.17.0 |

这张表记录已测试配置，不是最低支持版本承诺。当前质量工具版本按平台不同，两边对现有源码的格式及括号检查都已通过；跨平台统一工具版本仍属 Q07，不能宣称已经冻结。Linux 结果只覆盖 WSL2 内的构建与 CPU 环境，不代表原生 Linux 图形/桌面验证。

构建入口最低要求 CMake 3.25、Python 3.10；编译器必须支持测试使用的 C++20 标准库。最低系统、编译器与运行库支持范围仍需在 M1 冻结。

## 2. Windows

安装或保留 Visual Studio 的 C++ 桌面开发工具、Windows SDK、CMake、Ninja 和 Python。此机器已有这些组件，无需重复安装。质量工具使用 Visual Studio 的 VC/Tools/Llvm/x64/bin 下版本，不再依赖 Qt 随附的旧版本。

从项目根目录执行：

```powershell
python tools/environment/build.py --preset windows-debug
python tools/environment/build.py --preset windows-release
```

入口使用 vswhere 定位带 C++ 工具的 Visual Studio，初始化 x64 开发环境，将该安装中的 LLVM 工具加入本次子进程 PATH，再依次 configure/build/test。不修改系统 PATH，不记录完整环境变量，不要求普通终端事先能找到 cl。

### 用 CMake GUI 或 Visual Studio 生成器打开项目

在 CMake GUI 中将源码目录指向项目根目录，选择一个独立的构建目录。点击 **Configure**，选择 **Visual Studio 18 2026** 和 **x64**，然后点击 **Generate** 和 **Open Project**。用命令行也可以生成并编译同一配置。

```powershell
cmake -S . -B build/vs2026 -G "Visual Studio 18 2026" -A x64
cmake --build build/vs2026 --config Debug
```

CMake 会在选中的 Visual Studio 安装中查找 `clang-format` 和 `clang-tidy`。若缺少这两个工具，可在 Visual Studio Installer 中安装 LLVM 组件，或将 `VORYX_CLANG_FORMAT` 和 `VORYX_CLANG_TIDY` 设为对应可执行文件的绝对路径。修复后可在原构建目录重新配置。如果要切换生成器，请另选构建目录。

可在 Visual Studio 中生成 `voryx_braces_check` 或 `voryx_quality`。Visual Studio 生成器本身不导出 `compile_commands.json`，这两个目标会按所选配置在构建目录的 `quality-ninja/<配置>/` 下生成 Ninja 编译数据库，再用根 `.clang-tidy` 执行检查。优先使用 Visual Studio 附带的 Ninja，也可使用 `PATH` 中的 Ninja。首次执行会配置辅助目录，启用测试时会读取 GoogleTest 依赖缓存。

完整自动化验证请使用上面的 `windows-debug` 或 `windows-release` Ninja 预设。独立 SDK 消费测试也使用 Ninja。

Windows/MSVC 在本次默认沙盒中发生 C1902，正常本机权限下同一代码和配置可编译通过。若受限环境重复出现该错误，先检查调试数据库进程权限与工具路径，不通过跳过编译器检查解决；普通终端不需要为项目主动申请管理员权限。

## 3. Linux / Ubuntu

原生 Linux 需准备 C++20 编译器、CMake、Ninja、Python 和兼容根配置的 LLVM 检查工具。本次在已有 Ubuntu WSL2 中补装 ninja-build、clang-format-21、clang-tidy-21；软件源索引刷新后安装成功，没有升级其他已安装包。

Ubuntu 26.04 的复现安装命令（需要系统包管理权限）：

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends ninja-build clang-format-21 clang-tidy-21
```

在项目根目录执行：

```sh
python3 tools/environment/build.py --preset linux-debug
python3 tools/environment/build.py --preset linux-release
```

本机可从 Windows 调用已有发行版：

```powershell
wsl -d Ubuntu --exec python3 /mnt/d/VORYX/tools/environment/build.py --preset linux-debug
wsl -d Ubuntu --exec python3 /mnt/d/VORYX/tools/environment/build.py --preset linux-release
```

路径仅说明当前工作区；迁移项目后应使用实际路径。WSL 可能报告 localhost 代理不能映射到 NAT 网络，本次未阻止软件源访问；将其与编译/测试失败分开诊断。没有安装或修改显卡驱动。

## 4. CMake、依赖与离线缓存

入口由 CMakePresets.json 定义四套默认 CPU 配置及单独 Vulkan、OpenGL 配置，产物分别在 build/<preset>/。Null 默认在顶层构建启用，Vulkan 默认关闭，可按 [Vulkan 指南](vulkan-backend.md) 显式构建。Win32、X11 和 Wayland 可按各自选项构建。OpenGL 可显式启用，提供无窗口缓冲和离屏三角形，DX12 仍处于设计阶段。 [OpenGL 后端](opengl-backend.md)

GoogleTest 从官方 v1.17.0 归档获取，SHA256：

```text
65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
```

默认先读取 build/_downloads/googletest-v1.17.0.tar.gz，再按 SHA256 校验并解压到本配置的 _deps；没有缓存时从锁定的官方 URL 下载。也可通过 VORYX_DEPENDENCY_CACHE 指定归档缓存目录。Windows/Linux 使用相同归档，各自编译，不能共享已编译库。

依赖缓存和构建产物不进入源码版本控制，GoogleTest 许可位于解压源码中的 LICENSE。删除整个 build 后，离线用户必须先恢复归档缓存。初次配置仍需能访问归档或已有缓存，不能将下载失败解释为测试跳过。

已处于相应开发环境时，可直接使用 CMake：

```text
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
cmake --build --preset windows-debug --target voryx_quality
```

Linux 替换成 linux-debug。Windows 直接调用这些命令前须初始化 MSVC 环境，并使匹配的 clang-format/clang-tidy 可发现；推荐使用 Python 统一入口。可在 configure 时通过 VORYX_CLANG_FORMAT / VORYX_CLANG_TIDY 的 CMake 缓存值显式指定可执行文件。

## 5. 测试、报告与当前边界

2026-11-04 默认 CPU/Null 配置结果：

| 配置 | 环境测试 | 失败 / 跳过 |
| --- | --- | --- |
| windows-debug | 55/55 通过 | 0 / 0 |
| windows-release | 55/55 通过 | 0 / 0 |
| linux-debug（WSL2） | 55/55 通过 | 0 / 0 |
| linux-release（WSL2） | 55/55 通过 | 0 / 0 |

当前完整默认配置注册 55 项测试，另有七个公共头的独立编译检查：

- 三项 C++/标准库行为：span/ranges、jthread/latch 同步、中文文件路径读写。
- 三项质量检查：源码格式、真实编译数据库上的括号检查、完整/缺失括号正反例。
- 21 项 Core 单元测试和 24 项 Null 契约测试，覆盖所有权、状态机、字节复制、非法分支、线程与失败原子性。
- 四项集成测试：Core 样例、无开发工具依赖的源码接入、Null 字节样例、迁移安装前缀后的独立 SDK 消费。

测试标签包括 cpu / quality / unit / integration / contract_null；CTest JUnit 位于 build/<preset>/reports/ctest.xml，GoogleTest XML 同目录，详细日志在 Testing/Temporary/。作为依赖时可关闭测试、样例、质量工具和 Null，不需要 GoogleTest/Python/LLVM。

另已验证 ctest --no-tests=error 遇到空测试目录返回非零（本机退出码 8）。所有预设启用同一零测试保护，不能依赖默认“找不到测试也成功”的行为。

尚未建立远程 CI。已有无窗口 Vulkan 缓冲测试，Win32 WSI 清屏专项结果见下节；同步提交不证明多帧在途回收或真实设备丢失恢复。


## 6. Vulkan 缓冲专项证据

2026-11-05，契约版本 0.1.0-draft.5，对应 F12 第一批和 T32。构建选项为 VORYX_ENABLE_VULKAN=ON、VORYX_BUILD_GPU_TESTS=ON；软件配置另开 VORYX_ALLOW_SOFTWARE_VULKAN。验证层与同步验证严格启用，回调在销毁设备和命令后累计 Error 必须为零，不能仅检查 vkQueueSubmit 的返回值。

| 配置 | 实际执行设备 | 完整测试 | 失败 / 跳过 |
| --- | --- | --- | --- |
| windows-vulkan-debug | NVIDIA GeForce RTX 3050 Laptop GPU | 76/76 通过 | 0 / 0 |
| windows-vulkan-release | 同上，硬件执行 | 76/76 通过 | 0 / 0 |
| linux-vulkan-software-debug（WSL2） | llvmpipe，显式允许软件 | 76/76 通过 | 0 / 0 |
| linux-vulkan-software-release（WSL2） | 同上，软件执行 | 76/76 通过 | 0 / 0 |

每个 Vulkan 配置注册 20 项与 Null 共享的缓冲/命令契约、一项直接运行 Vulkan 样例；原有安装迁移消费测试还在独立编译的应用执行 Vulkan。gpu_vulkan 标签共 22 项，其中安装消费同时属于 integration。成功字节比较、提交前整批拒绝、句柄/引用/线程、原生设备销毁都纳入检查；没有图像、窗口或呈现断言。

本次版本证据：

| 项目 | Windows 硬件 | WSL 软件 |
| --- | --- | --- |
| loader / Khronos 验证层 | 1.4.341 / 1.4.341 | 1.4.341 / 1.4.341 |
| 本机开发包 | D:/APP/vulkan1.4，头文件 1.4.341 | libvulkan-dev 1.4.341 |
| 实际设备 API | 1.4.325（应用请求 1.2） | 1.4.335（应用请求 1.2） |
| 驱动 | NVIDIA 592.82 | Mesa 26.0.3-1ubuntu1，LLVM 21.1.8 |
| 设备类型 | DISCRETE_GPU | CPU（llvmpipe） |

Windows 还有 AMD 集显，本次选择独显；本次结果不代表集显验收。Windows 测试进程将隐式层搜索目录设为空目录以隔离损坏的 Epic Overlay 注册项，不修改系统配置，不忽略显式验证错误。当前 WSL 没有可用的硬件 Vulkan ICD，因此 linux-vulkan-debug/release 的硬件验收未执行，不能以软件配置替代。运行和依赖命令见 [Vulkan 指南](vulkan-backend.md)。

报告沿用 build/<preset>/reports/ctest.xml，设备名与验证消息在各测试的 system-out 和 Testing/Temporary/ 日志；源码未处于 Git 仓库，当前以本地工作区和 draft.5 文档版本记录，正式发布前应绑定提交或制品摘要。三角形、原生 Linux 硬件、非 coherent 专用分支及真实 DeviceLost 故障恢复仍未独立验证。

## 7. Win32 Vulkan 清屏证据

2026-11-06，契约版本 0.1.0-draft.6。Windows Vulkan Debug 和 Release 各通过 77/77 项，失败与跳过均为零。相比前一轮缓冲专项报告，Windows 配置新增 `integration.win32_clear`，`integration.installed_sdk` 也执行已安装窗口示例。启用 Khronos 验证和同步验证，检查呈现多帧、缩放、最小化暂停、恢复以及关闭。JUnit 路径为相应构建目录下的 `reports/ctest.xml`。

Linux WSL2 软件 Vulkan Debug 回归为 76/76，通过共享上下文、公共头和缓冲契约验证。X11 和 Wayland 专项结果见后续章节。该轮测试未执行原生 Linux GPU。Windows 窗口使用要求和复现命令见 [Win32 清屏指南](win32-clear.md)。

## 8. X11 Vulkan 清屏证据

2026-11-06，契约版本 0.1.0-draft.7。Ubuntu WSL2 / WSLg 的 X11 显示使用 llvmpipe 软件 Vulkan（Mesa 26.0.3、LLVM 21.1.8）。Xlib 开发包为 1.8.13，Vulkan 开发包与 Khronos 验证层为 1.4.341。`linux-vulkan-x11-software-debug` 明确允许软件设备，77/77 通过，失败与跳过均为零。

`integration.xlib_clear` 执行多帧、缩放、取消映射与恢复。`integration.installed_sdk` 把 SDK 迁移后独立构建并运行同一 X11 示例。原生 Linux 硬件仍待验证。Wayland 专项见下节。Windows 硬件 Vulkan Debug 回归 77/77，Linux 软件 Vulkan 无显示回归 76/76。运行入口及限制见 [X11 清屏指南](xlib-clear.md)。

## 9. 原生 Wayland Vulkan 清屏证据

2026-11-06，契约版本 0.1.0-draft.8。Ubuntu WSL2 / WSLg 的原生 `wayland-0` 会话使用 llvmpipe 软件 Vulkan。运行时清除 `DISPLAY`。宿主示例使用 libwayland 1.24.0 与 wayland-protocols 1.47 的稳定 xdg-shell。

`linux-vulkan-wayland-software-debug` 77/77 通过，失败与跳过均为零。窗口用例验证首次 configure、最大化后的尺寸变化、显式暂停、恢复与销毁。安装消费测试从迁移前缀独立生成协议代码，编译并运行同一示例。Khronos 验证层与同步验证开启。

Windows 硬件 Vulkan Debug 回归 77/77，X11 窗口专项回归 1/1。原生 Linux GPU、分数缩放及不同 compositor 的行为仍待单独验证。见 [Wayland 接入指南](wayland-clear.md)。

## 10. Vulkan 三角形专项证据

2026-11-06，契约版本 0.1.0-draft.9。三个配置均通过 78/78 项，失败与跳过均为零。

| 配置 | 实际环境 |
| --- | --- |
| `windows-vulkan-debug` | Windows 硬件 Vulkan / Win32 |
| `linux-vulkan-x11-software-debug` | WSLg llvmpipe / X11 |
| `linux-vulkan-wayland-software-debug` | WSLg llvmpipe / 原生 Wayland，清除 `DISPLAY` |

窗口用例经公共 API 绘制并验证缩放恢复。迁移 SDK 后独立编译的应用也执行绘制。离屏像素用例使用同一管线读回 64×64 RGBA8 颜色附件，断言中心为红色且角落为蓝色。测试启用 Khronos 同步验证。原生 Linux 硬件、通用图形管线和纹理/深度未在本阶段验收。 [三角形运行与接口](vulkan-triangle.md)

## 11. Vulkan 纹理采样证据

2026-11-06，契约版本 0.1.0-draft.10。Windows 硬件 Vulkan Debug、WSLg X11 软件 Debug 与 WSLg 原生 Wayland 软件 Debug 均通过 79/79 项，失败与跳过为零。Wayland 验收时已清除 `DISPLAY`。

`SamplesPaddedRgbaRowsWithoutFlipping` 使用 2×2 RGBA8、11 字节行距及不相同的 alpha，断言 64×64 颜色附件中四象限的实际 RGBA。三个窗口用例均配置并替换彩色与纹理绘制，检查非法行距后仍可呈现，以及缩放和暂停恢复。迁移安装后独立编译并运行同一窗口示例。Khronos 同步验证已启用。原生 Linux GPU 仍待验收。 [纹理接入指南](vulkan-texture.md)

## 12. Vulkan 深度测试证据

2026-11-06，契约版本 0.1.0-draft.11。Windows 硬件 Vulkan Debug、WSLg X11 软件 Debug 和 WSLg 原生 Wayland 软件 Debug 均通过 80/80 项，失败与跳过为零。Wayland 验收时已清除 `DISPLAY`。

`NearTriangleWinsInEitherVertexOrder` 交换两个三角形的顶点组顺序。两次读回均确认重叠区域为近处的红色，未遮挡处仍保留远处绿色，且角落背景正确。三个窗口用例与迁移后的独立 SDK 示例检查非法深度、缩放和暂停恢复。Khronos 同步验证已启用。原生 Linux GPU 与其他深度格式的独立覆盖仍待验收。 [深度接口及示例](vulkan-depth.md)

## 13. Vulkan 索引绘制证据

2026-11-06，契约版本 0.1.0-draft.12。Windows 硬件 Vulkan Debug、WSLg X11 软件 Debug 和 WSLg 原生 Wayland 软件 Debug 均通过 81/81 项，失败与跳过为零。Wayland 验收时清除 `DISPLAY`。

`IndexedQuadSharesVerticesAndCoversTheSeam` 用四顶点和六索引绘制矩形，断言两个三角形内部、共享对角线和背景实际像素。三个窗口用例及迁移安装后的示例均检查越界索引、非法数量、错误后继续呈现以及缩放和暂停恢复。Khronos 同步验证已启用。原生 Linux GPU 仍待验收。 [索引绘制指南](vulkan-indexed.md)

## 14. Vulkan 可复用纹理绑定证据

2026-11-09，契约版本 0.1.0-draft.13。Windows 硬件 Vulkan Debug、WSLg X11 软件 Debug 和 WSLg 原生 Wayland 软件 Debug 各通过 82/82 项，失败与跳过为零。Wayland 验收时清除 `DISPLAY`。

像素用例复用同一纹理，分别使用边缘钳位和重复采样，验证实际中心像素分别为绿色和红色。用例检查资源忙、跨 Presenter 句柄和销毁后的失效句柄。窗口和迁移安装 SDK 示例验证 `ResetDraw` 后重新使用同一绑定、缩放和恢复。原生 Linux GPU 仍待验收。 [可复用纹理绑定](vulkan-bindings.md)

## 15. Vulkan 显式绑定布局证据

2026-11-09，契约版本 0.1.0-draft.14。Windows 硬件 Vulkan Debug、WSLg X11 软件 Debug 和 WSLg 原生 Wayland 软件 Debug 各通过 83/83 项，失败与跳过为零。Wayland 验收时清除 `DISPLAY`。

`UsesSparseBindingsForTransformAndSampling` 在 Khronos 验证层下读取 64×64 RGBA8 附件。变换区域外为蓝色，区域内分别采得红色和绿色。测试在创建常量缓冲后覆盖源内存，并验证重复布局、错误槽位、错误资源类型、跨 Presenter 句柄和依赖销毁。三个窗口和迁移安装后的独立示例均通过公共接口配置新布局并完成呈现、缩放与恢复。原生 Linux GPU 仍待验收。 [调用指南](vulkan-binding-layout.md)

测试报告位于各 preset 的 `build/<preset>/reports/ctest.xml`。

## 16. RHI 图形管线首批证据

2026-11-23，契约版本 0.1.0-draft.15。以下三个 Debug 配置各通过 94/94 项，失败和跳过均为零。

| 配置 | 本次验证范围 |
| --- | --- |
| `windows-vulkan-debug` | Windows 硬件 Vulkan 与 Win32 |
| `linux-vulkan-x11-software-debug` | WSLg llvmpipe 软件 Vulkan 与 X11 |
| `linux-vulkan-wayland-software-debug` | WSLg llvmpipe 软件 Vulkan 与原生 Wayland，清除 DISPLAY |

新增五项 Null 和五项 Vulkan 共享管线契约，以及一项真实像素用例。共享契约验证顶点布局、Shader 阶段与指令边界、无效输入后的继续创建、句柄归属、代数、线程和图形状态创建。像素用例用同一原生管线绘制两份带填充的顶点数据，检查红色和绿色输出、绕序剔除、Alpha 与预乘混合。原有深度、纹理、索引与资源绑定路径均已回归统一原生管线实现。

三个配置均通过格式、完整括号和独立迁移安装消费。消费端通过 find_package 编译并运行新增公共管线创建和销毁接口。设备命令绑定与绘制尚待后续批次，原生 Linux 硬件 GPU 仍待验收。报告位于各 preset 的 `build/<preset>/reports/ctest.xml`。 [本批接口与范围](rhi-pipeline-api.md)

## 17. 设备级离屏绘制证据

2026-11-23，契约版本 0.1.0-draft.16。最终代码的三个 Debug 配置均通过 108/108 项，失败与跳过为零。

| 配置 | 实际执行范围 | 测试结果 |
| --- | --- | --- |
| `windows-vulkan-debug` | Windows 硬件 Vulkan、Win32 和独立设备绘制 | 108/108 |
| `linux-vulkan-x11-software-debug` | WSLg llvmpipe 软件 Vulkan、X11 和独立设备绘制 | 108/108 |
| `linux-vulkan-wayland-software-debug` | WSLg llvmpipe 软件 Vulkan、原生 Wayland 和独立设备绘制，清除 DISPLAY | 108/108 |

本轮新增六项 Null 和八项 Vulkan 测试。共享用例覆盖连续附件清除、录制顺序、越界、资源引用、Reset 与提交完成后的释放、纹理归属与代数、线程、跨列表整批校验和设备先于命令销毁。

Vulkan 设备绘制像素用例通过公开 API 验证 UInt16/UInt32 索引、非零绑定偏移与 firstIndex、负 vertexOffset、裁剪、非索引 Draw、深度遮挡和跨提交管线与附件复用。索引矩形的裁剪内像素为红色，裁剪外与角落保持蓝色。近远几何交换提交顺序后，中心仍为近处红色。验证层错误累计为零。

迁移安装后的独立 `pipeline_consumer` 在 Null 和 Vulkan 下创建附件、录制、提交、同步读回并检查结果。原有窗口绘制、安装消费、格式和完整括号检查同时回归。最终检查还补齐原生图像、视图和帧缓冲在创建成功后才接管句柄的失败回收处理。该处理经过重建回归，本轮未注入原生内存耗尽故障。

报告位于各 preset 的 `build/<preset>/reports/ctest.xml`。本批设备绘制为离屏同步流程，设备资源绑定与窗口帧接入继续推进。原生 Linux 硬件 GPU 和当前版本完整 Release 矩阵仍待验收。 [接口与限制](rhi-draw-api.md)

## 18. 设备资源绑定与组合绘制证据

2026-11-23，契约版本为 0.1.0-draft.17。最终代码的三个 Debug 配置均通过 112/112 项，失败和跳过为零。

| 配置 | 验证环境 | 结果 |
| --- | --- | --- |
| `windows-vulkan-debug` | Windows 硬件 Vulkan、Win32 | 112/112 |
| `linux-vulkan-x11-software-debug` | WSLg llvmpipe 软件 Vulkan、X11 | 112/112 |
| `linux-vulkan-wayland-software-debug` | WSLg llvmpipe 软件 Vulkan、原生 Wayland，清除 DISPLAY | 112/112 |

T45 对 Null 和 Vulkan 各新增两项共享用例，覆盖 Shader 槽位类型与可见阶段、范围和对齐、跨设备句柄、资源忙、换管线后重新绑定，以及补录常量状态屏障后重试提交。

T46 的独立 `pipeline_consumer` 从迁移后的 SDK 编译和执行。Vulkan 在一个深度 Pass 连续调用两次 DrawIndexed，使用带行距填充的纹理、最近邻采样和同一常量缓冲的两个对齐范围。红色近处覆盖绿色远处，移动边界外保留蓝色背景。两份 Shader 通过 Vulkan 1.2 目标的 SPIR-V 离线验证。格式、完整括号和原有窗口路径一并回归。

X11 首轮测试在配置安装后的 X11 窗口示例时达到子进程超时。最终完整回归中的安装消费和窗口测试均通过。测试报告位于各 preset 的 `build/<preset>/reports/ctest.xml`。原生 Linux 硬件 GPU 和当前版本的完整 Release 验收仍待执行。 [设备绑定指南](rhi-binding-api.md)

## 19. 设备绘制接入窗口帧证据

2026-11-23，契约版本为 0.1.0-draft.18。最终三个 Debug 配置各通过 114/114 项，失败和跳过均为零。

| 配置 | 验证环境 | 结果 |
| --- | --- | --- |
| `windows-vulkan-debug` | Windows 硬件 Vulkan、Win32 | 114/114 |
| `linux-vulkan-x11-software-debug` | WSLg llvmpipe 软件 Vulkan、X11 | 114/114 |
| `linux-vulkan-wayland-software-debug` | WSLg llvmpipe 软件 Vulkan、原生 Wayland，清除 DISPLAY | 114/114 |

T47 通过窗口程序和迁移安装后的同一示例执行真实 AcquireFrame、带纹理及常量绑定的索引绘制、深度附件、提交、Present 与取消。用例检查借用图像、重复获取、无令牌和过期令牌提交、提交令牌不匹配、缺少最终 Present 转换、转换后再次引用以及引用中取消。窗口尺寸变化与暂停恢复覆盖三个 WSI。程序要求实际 Present 返回成功，验证层中的错误会使测试失败。

T48 在 Null 和 Vulkan 各新增一项测试。普通离屏附件拒绝转换为 Present，随后可正常执行清除和读回。格式、完整括号和独立 SDK 消费一同通过。Linux 中间轮次曾把更新后的安装示例与旧库组合，最终同版完整重建后通过。

报告位于 `build/<preset>/reports/ctest.xml`。本次实测为同步单活动帧。异步多帧、原生 Linux 硬件 GPU 与当前版本的完整 Release 验收继续推进。 [窗口帧接口](window-frame-api.md)

## 20. Vulkan 离屏异步提交证据

2026-11-24，契约版本为 `0.1.0-draft.19`。三个 Debug 配置均通过 117 项，零失败、零跳过。

| 构建配置 | 实际设备和窗口环境 | 通过 |
| --- | --- | --- |
| `windows-vulkan-debug` | NVIDIA GeForce RTX 3050 Laptop GPU，Win32 | 117/117 |
| `linux-vulkan-x11-software-debug` | WSLg llvmpipe 软件 Vulkan，X11 | 117/117 |
| `linux-vulkan-wayland-software-debug` | WSLg llvmpipe 软件 Vulkan，原生 Wayland，清除 DISPLAY | 117/117 |

T49 增加两项 CPU 契约测试。可控完成源复现 Pending、零与有限等待超时、队列满以及保留命令重试，Null 验证不支持异步时仍能使用同一批次同步提交。T50 新增 Vulkan 连续复制用例，开启同步验证，提前释放句柄及命令包装后，按较新令牌等待并核对完整字节。该用例在 Windows 硬件上另连续通过 20 次。

迁移安装 SDK 的独立程序执行 Vulkan 离屏异步绘制。提交后提前释放绑定集、图形管线、深度附件和顶点索引缓冲，等待后对比采样、常量变换和深度遮挡的目标像素。三套配置通过格式、完整括号和同步窗口帧回归。

复现时在对应平台执行 `python tools/environment/build.py --preset <构建配置>`，Linux 可使用 `python3`，Wayland 配置先执行 `unset DISPLAY`。汇总报告分别在 `build/<构建配置>/reports/ctest.xml`，本轮日志在 `build/async-windows.log`、`build/async-x11.log` 与 `build/async-wayland.log`。原生 Linux 硬件 GPU、当前版本完整 Release 和真实设备丢失路径待验收。 [异步调用指南](rhi-async-submit.md)

## 21. Vulkan 窗口双帧提交证据

2026-11-26，契约版本为 `0.1.0-draft.20`。三个 Debug 配置各通过 117 项，零失败、零跳过。

| 构建配置 | 实际设备和窗口环境 | 通过 |
| --- | --- | --- |
| `windows-vulkan-debug` | NVIDIA GeForce RTX 3050 Laptop GPU，Win32 | 117/117 |
| `linux-vulkan-x11-software-debug` | WSLg llvmpipe 软件 Vulkan，X11 | 117/117 |
| `linux-vulkan-wayland-software-debug` | WSLg llvmpipe 软件 Vulkan，原生 Wayland，清除 DISPLAY | 117/117 |

T51 扩展现有窗口集成和 SDK 消费用例，测试总数保持不变。每轮先异步绘制一个使用纹理、常量、索引和深度的帧，再连续异步清屏并呈现。中途验证旧提交令牌无法呈现当前帧，呈现后借用图像失效。连续提交期间应用未调用 Wait，随后等待最新令牌并释放场景资源。窗口示例还留下一帧给旧式呈现切换或析构清理，迁移安装后的程序执行同样流程。

三个配置同时通过原有同步帧、缩放和暂停恢复、离屏像素读回、格式及完整括号检查。本批的窗口双帧验收以固定尺寸连续呈现为基线。显式限时关闭、连续重建专项、原生 Linux 硬件 GPU 和当前版本 Release 待验收。

复现使用 `python tools/environment/build.py --preset <构建配置>`，Linux 可用 `python3`，Wayland 配置先执行 `unset DISPLAY`。各构建目录含 `reports/ctest.xml`，本轮日志在 `build/window-async-windows.log`、`build/window-async-x11.log` 和 `build/window-async-wayland.log`。 [窗口帧接口](window-frame-api.md)

## 22. Vulkan 窗口限时关闭与连续重建证据

2026-11-26，契约版本为 `0.1.0-draft.21`。三个 Debug 配置各通过 118 项，零失败、零跳过。

| 构建配置 | 实际设备和窗口环境 | 通过 |
| --- | --- | --- |
| `windows-vulkan-debug` | NVIDIA GeForce RTX 3050 Laptop GPU，Win32 | 118/118 |
| `linux-vulkan-x11-software-debug` | WSLg llvmpipe 软件 Vulkan，X11 | 118/118 |
| `linux-vulkan-wayland-software-debug` | WSLg llvmpipe 软件 Vulkan，原生 Wayland，清除 DISPLAY | 118/118 |

T52 新增一个可控完成源测试，验证命令包装仍在时返回 ResourceBusy，释放后仍保留未完成提交，有限等待返回 Timeout。随后令牌仍可查询，完成后可以重试关闭，关闭后令牌不可再查询。

三个窗口样例都在原有异步帧尚未由应用等待时改变尺寸，两次直接从新交换链获取图像、核对像素尺寸并异步呈现。Wayland 使用最大化和恢复触发两次真实 configure。退出前另外提交一帧并保留为未呈现状态，验证错误线程被拒绝、持有命令列表时资源忙、释放命令包装后完成显式关闭以及重复关闭。窗口测试启用 Vulkan 验证，移动 SDK 前缀后独立编译运行同样流程。

复现使用 `python tools/environment/build.py --preset <构建配置>`，Linux 可用 `python3`，Wayland 配置先执行 `unset DISPLAY`。各构建目录含 `reports/ctest.xml`，本轮日志分别为 `build/window-close-windows.log`、`build/window-close-x11.log` 和 `build/window-close-wayland.log`。当前版本 Release、原生 Linux 硬件和真实设备丢失仍待验收。 [窗口关闭接口](window-frame-api.md#显式限时关闭)

## 23. CI 配置与本地 Release 验收证据

2026-11-26，接口契约仍为 `0.1.0-draft.21`。下表是当前软件状态的本地结果，所有最终轮次零失败、零跳过。

| 构建配置 | 执行环境 | 通过 |
| --- | --- | --- |
| `windows-debug`、`windows-release` | Windows CPU、Null | 各 72/72 |
| `linux-debug`、`linux-release` | Ubuntu WSL2 CPU、Null | 各 72/72 |
| `windows-vulkan-release` | NVIDIA GeForce RTX 3050 Laptop GPU，Win32 | 118/118 |
| `linux-vulkan-x11-software-release` | WSLg llvmpipe 软件 Vulkan，X11 | 118/118 |
| `linux-vulkan-wayland-software-release` | WSLg llvmpipe 软件 Vulkan，原生 Wayland，清除 DISPLAY | 118/118 |

四套 CPU 配置复用了 GitHub Actions 的同名预设和 Python 入口，并包含独立 SDK 接入。Vulkan Release 覆盖离屏像素验证、窗口缩放与限时关闭，以及迁移安装后运行的窗口示例。Win32 Vulkan Release 首轮在多个配置并行运行时触发括号检查超时，随后完整复跑通过。

本地核对了工作流 YAML、预设映射、权限和 Bash 语法，并用可控报告验证空测试、缺失 WSI 或套件、跳过项与不完整报告会使入口失败。GitHub 仓库和专用执行器尚未接入此工作区的验收流程，远端 CI 与原生 Linux 硬件仍待实际运行。 [持续集成指南](ci.md)

构建入口为 `python tools/environment/build.py --preset <预设>`，Linux 可用 `python3`。测试注册清单和汇总分别在 `build/<预设>/reports/ctest-plan.json` 与 `ctest.xml`，初轮 Windows Vulkan Release 超时见 `build/ci-release-windows.log`，复跑见 `build/ci-release-windows-retry.log`。

## 24. OpenGL 无窗口缓冲证据

2026-11-26，契约版本为 `0.1.0-draft.22`。以下为本轮最终 Debug 验证，零失败、零跳过。

| 构建配置 | 执行环境 | 通过 |
| --- | --- | --- |
| `windows-opengl-debug` | Windows WGL，AMD Radeon(TM) Graphics，硬件 OpenGL | 95/95 |
| `linux-opengl-software-debug` | Ubuntu WSL，EGL 无窗口平台，llvmpipe (LLVM 21.1.8, 256 bits)，软件 OpenGL | 95/95 |
| `windows-vulkan-debug` | Windows Win32 硬件 Vulkan 回归 | 118/118 |

T53 在两平台实测 `glBufferSubData` 上传、`glCopyBufferSubData` 复制和 `glGetBufferSubData` 读回，复用缓冲状态、范围校验、连续覆盖写入与错误恢复。两个设备同时存在时，销毁一个设备不会影响另一个设备的读回。迁移安装 SDK 后另通过 `find_package` 构建独立用户程序，并运行同样的 OpenGL 字节验证。

Linux 本机补装 `libegl-dev` 1.7.0-3。软件验证在 `DISPLAY` 和 `WAYLAND_DISPLAY` 均未设置且 `LIBGL_ALWAYS_SOFTWARE=1` 的环境执行。Windows 首轮的括号检查将两个只用于 EGL 的字段标为未使用，按平台拆分后完整复跑通过。

汇总及注册计划存放于各预设的 `build/<preset>/reports/`，本轮日志为 `build/opengl-windows-final.log`、`build/opengl-linux.log` 与 `build/opengl-vulkan-regression.log`。原生 Linux 硬件、OpenGL 窗口呈现、绘制及 OpenGL Release 待专项验收。 [OpenGL 指南](opengl-backend.md)

GitHub Actions 首次托管执行基于初始提交 `b20f860`，Windows/Linux Debug 和 Release 四套 CPU/Null 均已通过。 [首次运行](https://github.com/shi-qi12138/VORYX/actions/runs/45766123021)

## 25. OpenGL 离屏清屏与读回证据

2026-11-27 验证 `0.1.0-draft.23`，以下配置均为 Debug。

| 构建配置 | 执行环境 | 通过 | 失败 | 跳过 |
| --- | --- | --- | --- | --- |
| `windows-opengl-debug` | WGL，AMD Radeon(TM) Graphics，硬件 OpenGL | 101/101 | 0 | 0 |
| `linux-opengl-software-debug` | Ubuntu WSL，无窗口 EGL，llvmpipe (LLVM 21.1.8, 256 bits) | 101/101 | 0 | 0 |
| `windows-vulkan-debug` | Windows 硬件 Vulkan，Win32 回归 | 118/118 | 0 | 0 |

T54 通过公共 RHI 创建原生颜色附件、转换用途、同步清除和读回。共享测试验证重复清除、整批拒绝后的重试、句柄归属和屏障错误恢复。OpenGL 专项还验证两块 5×3 纹理分别保留各自的颜色和 Alpha，销毁一块后另一块仍可读回。迁移安装前缀后，使用 `find_package` 单独编译并链接 `opengl_clear`，两套 OpenGL 配置都逐像素核对了 64×64 红色附件。

执行入口见下方。Windows 环境首次编译新的独立示例时遇到受限环境下已记录的 MSVC C1902，使用本机正常工具链权限完成完整复验。

```text
python tools/environment/build.py --preset windows-opengl-debug
python tools/environment/build.py --preset windows-vulkan-debug
```

```bash
unset DISPLAY WAYLAND_DISPLAY
LIBGL_ALWAYS_SOFTWARE=1 python3 tools/environment/build.py --preset linux-opengl-software-debug
```

日志分别为 `build/opengl-clear-windows-final.log`、`build/opengl-clear-linux.log` 和 `build/opengl-clear-vulkan.log`。各预设的 `reports/ctest.xml` 记录明细。原生 Linux 硬件、OpenGL Release、Shader 绘制及窗口呈现待专项验收。 [OpenGL 接入](opengl-backend.md)

## 26. OpenGL 离屏三角形证据

2026-11-27 验证 `0.1.0-draft.24`。测试包含格式和完整括号检查，配置均为 Debug。

| 构建配置 | 执行环境 | 通过 | 失败 | 跳过 |
| --- | --- | --- | --- | --- |
| `windows-opengl-debug` | Windows WGL，AMD Radeon(TM) Graphics，硬件 OpenGL | 106/106 | 0 | 0 |
| `linux-opengl-software-debug` | Ubuntu WSL，无窗口 EGL，llvmpipe (LLVM 21.1.8, 256 bits) | 106/106 | 0 | 0 |
| `windows-vulkan-debug` | Windows 硬件 Vulkan，Win32 与 SPIR-V 回归 | 120/120 | 0 | 0 |

T55 验证驱动编译和链接错误诊断、异常后重新创建管线、顶点数据偏移和非零首顶点、左上角视口和裁剪，以及新 Pass 全图清屏。分数视口导致批次被拒绝时，已提交的像素保持原值。在两套 OpenGL 预设中，测试均从迁移后的 SDK 单独配置、编译并运行 `opengl_triangle`，检查 64×64 图像中三角形内外及垂直方向的像素。另从 Windows 的 `build/sdk/windows-x64-opengl-debug-draft24` 安装前缀独立运行同一示例。

构建命令沿用第 25 节。对应日志为 `build/opengl-draw-windows.log`、`build/opengl-draw-linux.log` 和 `build/opengl-draw-vulkan.log`，各预设的 `reports/ctest.xml` 保留测试明细。原生 Linux 硬件、OpenGL Release 和 OpenGL 窗口呈现待专项验收。 [OpenGL 绘制指南](opengl-backend.md#离屏三角形)

## 27. 架构审查修复验证

2026-11-27 验证 `0.1.0-draft.25`。T56 使用可控完成源复现帧提交退休超时和获取 fence 尚未就绪，确认提交记录与已取得的图像均可在后续调用继续等待。Windows Vulkan 安装示例通过通用 `CreateFramePresenter` 入口实际提交并呈现，再验证限时关闭。T57 检查仅编入 OpenGL 时的自动选择、无采样器或布局能力时的 `Unsupported`、关闭 Null 后的契约用例及独立安装消费。

安装缺件复测在隔离的构建目录保留旧的 `triangle_glsl.hpp` 副本，并让模拟新包漏装该文件。新消费者从空前缀构建并报告缺少头文件，验证旧目录不会使漏装误通过。

| 配置 | 验证结果 |
| --- | --- |
| `windows-vulkan-debug` | Win32 硬件 Vulkan 122/122 |
| `linux-vulkan-wayland-software-debug` | WSLg 原生 Wayland 软件 Vulkan 122/122 |
| `windows-opengl-debug` | 109 项首轮通过，超时的完整括号检查单独复跑通过，共 110 项 |
| `linux-opengl-software-debug` | 108 项首轮通过，超时的完整括号与独立 SDK 消费串行复跑通过，共 110 项 |
| Windows OpenGL Debug 关闭 Null | 65/65，含后端契约与独立 SDK 验收 |
| `linux-vulkan-x11-software-debug` | X11 窗口样例与两个帧状态回归通过，超时的安装消费单独复跑通过，共 4 项专项 |

关闭 Null 配置可在 MSVC 环境通过 `cmake --preset windows-opengl-debug -B build/gl-no-null -DVORYX_ENABLE_NULL=OFF` 生成，构建后用 `ctest --test-dir build/gl-no-null --output-on-failure` 运行。 [测试入口](automated-testing.md)

构建与测试日志位于 `build/audit-fix-*.log`，包含首轮并行测试及 `*-retry.log` 中的串行复跑结果。Linux Vulkan 窗口结果来自 WSLg。原生 Linux 显卡继续专项验收。

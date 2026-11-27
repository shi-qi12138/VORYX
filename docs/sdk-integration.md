# 在其他工程中使用 VORYX SDK

SDK 提供静态库、公共头和 CMake Config 包。可接入 Core、缓冲 RHI、Null 契约后端及可选的 Vulkan、OpenGL 后端。Vulkan 可执行离屏绘制和已启用窗口系统的呈现。OpenGL 可执行离屏三角形绘制、颜色清除、读回和缓冲操作。

## 1. 安装 SDK

先用 [构建入口](environment.md) 完成对应配置的编译和测试，再从已初始化的开发环境执行安装：

```text
cmake --install build/windows-debug --prefix D:/VoryxSDK/windows-debug
```

Linux 对应 linux-debug，prefix 使用本机实际可写路径。Debug/Release、操作系统、架构和工具链使用不同安装目录；头文件和库必须来自同一次构建，不能混用。不要修改系统目录来接入 SDK。

VORYX_INSTALL 默认在顶层项目启用，作为子项目默认关闭。当前所有库都是静态库；不提供 DLL 导出或跨编译器 ABI 承诺。开发构建版本为 0.1.0，接口验证期使用精确版本匹配，不代表不同提交已二进制兼容。

## 2. 安装内容与公共目标

MSVC 的 Debug/RelWithDebInfo 调试信息嵌入对象文件，静态库安装后不依赖原构建目录中的编译器 PDB。要单步查看库的 .cpp 实现仍需匹配版本的源码并配置源码映射，不能混用其他版本调试。

| 内容 | 默认路径 |
| --- | --- |
| 公共头 | include/voryx/core、rhi、runtime |
| 静态库 | lib/；文件后缀随工具链变化 |
| Config / Version / Targets | lib/cmake/Voryx/ |
| 可独立构建的 Null 示例 | share/Voryx/examples/null_buffers/ |
| 可独立构建的 OpenGL 离屏清屏示例 | share/Voryx/examples/opengl_clear/（启用 OpenGL 时） |
| 可独立构建的 OpenGL 离屏三角形示例 | share/Voryx/examples/opengl_triangle/（启用 OpenGL 时） |
| 可独立构建的 Win32 Vulkan 清屏示例 | share/Voryx/examples/win32_clear/（启用 Win32 WSI 时） |
| 可独立构建的 X11 Vulkan 清屏示例 | share/Voryx/examples/xlib_clear/（启用 X11 时） |
| 可独立构建的 Wayland Vulkan 清屏示例 | share/Voryx/examples/wayland_clear/（启用 Wayland 时） |

设备管线的公共类型、可编译创建示例与首批限制见管线指南，安装消费测试运行独立的管线、绑定与离屏绘制程序，并在 Vulkan 配置中选择离屏异步提交，提前释放命令包装与直接绘制句柄，等待后检查纹理采样、常量变换、索引绘制和深度遮挡的组合像素。 [离屏异步提交](rhi-async-submit.md) [设备绑定](rhi-binding-api.md) [设备绘制](rhi-draw-api.md) [管线 API](rhi-pipeline-api.md)

窗口示例的 --verify 还使用窗口设备录制和呈现同一套资源绘制，覆盖默认同步、连续异步帧提交、在途重建与显式关闭。 [窗口帧](window-frame-api.md)

对外链接入口为 Voryx::core、Voryx::rhi_api、Voryx::runtime。Voryx::runtime 自动携带所需的 Core、RHI 和当前已编译后端链接关系，不要手工枚举 .lib/.a。Voryx::detail_* 仅为静态链接依赖导出，不是供应用调用的接口。

安装包不包含私有后端头、源仓库路径、GoogleTest、Python 或 LLVM。Config 通过系统标准的 Threads 查找恢复线程依赖；用户程序不需要 VORYX 的开发检查工具。

Voryx_WITH_OPENGL 表示该安装包含 OpenGL 后端。Linux 静态包会通过 find_dependency 恢复 EGL/OpenGL 链接依赖，Windows 使用系统 OpenGL 库。迁移 SDK 的独立缓冲示例支持 --opengl。 [OpenGL 后端](opengl-backend.md)

Voryx_WITH_VULKAN 表示该安装包含 Vulkan 后端。当前为静态链接，消费这种 SDK 的工程也需可发现的 Vulkan 1.2+ 开发包/loader 链接库；Config 会执行 find_dependency(Vulkan 1.2)。部署机器需要可用 Vulkan loader 和驱动，开启 validation 另需验证层。公共头不包含 SDK，Voryx_WITH_VULKAN=OFF 的 SDK 完全不要求 Vulkan。

CMAKE_INSTALL_INCLUDEDIR/LIBDIR/BINDIR/DATADIR 必须使用相对目录，根 prefix 决定安装位置。安装后的 SDK 可整体迁移，不能只移动某个库文件后仍使用旧头文件或 Config。

## 3. 独立 CMake 工程

应用自己的 CMakeLists.txt：

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyRendererApp LANGUAGES CXX)

find_package(Voryx 0.1.0 EXACT CONFIG REQUIRED COMPONENTS core rhi_api runtime)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE Voryx::runtime)
```

配置时通过 CMAKE_PREFIX_PATH 指向 SDK 根目录：

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=D:/VoryxSDK/windows-debug
cmake --build build
```

Windows 须在与 SDK 相匹配的 x64 MSVC 开发环境执行；Linux 使用相匹配的编译器/标准库。C++20、包含目录与传递链接依赖由目标提供，不需要添加 VORYX 私有 include 路径，也不需要 add_subdirectory 原仓库。

只使用基础错误/日志时，链接 Voryx::core；自定义后端契约使用 Voryx::rhi_api；创建设备使用 Voryx::runtime。准确调用流程见 [缓冲 RHI API](rhi-buffer-api.md)。

## 4. 只拿安装包也能运行的示例

安装包中 share/Voryx/examples/null_buffers 包含 main.cpp 和独立 CMakeLists.txt。将该目录复制到应用工作区，通过 CMAKE_PREFIX_PATH 配置、编译并运行 null_buffers。

它会显式创建 Null 设备，上传四个字节，记录状态转换与复制，提交、等待，再读回比较。成功输出 Buffer copy verified. 并返回 0，任意失败返回 1。默认不创建 GPU。Vulkan SDK 构建的程序可传 --vulkan --validation 执行真实缓冲复制；软件 ICD 必须显式传 --allow-software。均不创建窗口，不证明 GPU 绘制。

`Voryx_WITH_OPENGL` 为真时，安装包包含 `opengl_clear`。它创建无窗口设备，经同一公共 RHI 清除离屏附件并验证全部像素。`opengl_triangle` 与 GLSL 源码一并安装，示例绘制非索引三角形后核对像素。独立构建这两个示例时，使用目录中随附的 CMakeLists.txt 和头文件。自动化验收在迁移后的安装目录分别配置、编译并运行两种示例。 [OpenGL 接入与复现命令](opengl-backend.md#离屏清屏和读回)

`Voryx_WITH_WIN32_WSI` 为真时，安装包包含宿主窗口清屏示例。完整 Windows GPU 验收也会将安装前缀迁移后独立配置、编译和运行该示例。见 [窗口接入指南](win32-clear.md)。

`Voryx_WITH_X11` 为真时，Config 恢复 `X11::X11` 并提供已安装窗口示例。X11 GPU 测试独立编译并运行迁移后的示例。见 [X11 清屏指南](xlib-clear.md)。

`Voryx_WITH_WAYLAND` 为真时，安装包包含原生 Wayland 示例。示例的 xdg-shell 代码由宿主构建生成，SDK 库只需要 Vulkan。迁移消费验收会独立编译并运行。 [Wayland 接入指南](wayland-clear.md)

三个窗口示例均支持 `--triangle`、`--texture`、`--depth`、`--indexed`、`--binding` 和 `--resources`，安装包附带已编译的 Shader 夹具。应用通过同一公共 C++ 接口配置自己的 Shader 与顶点。 [三角形指南](vulkan-triangle.md) · [纹理指南](vulkan-texture.md) · [深度指南](vulkan-depth.md) · [索引指南](vulkan-indexed.md) · [绑定指南](vulkan-bindings.md) · [常量缓冲与绑定布局](vulkan-binding-layout.md)

## 5. 源码依赖接入

需要与应用一起编译时，可以 add_subdirectory 引入仓库并链接相同目标。作为子项目默认关闭测试、样例、质量工具、安装与 Null 后端；需要 Null 时，在 add_subdirectory 前显式设置 VORYX_ENABLE_NULL=ON。

启用 Vulkan 时还需在 add_subdirectory 前设置 VORYX_ENABLE_VULKAN=ON，并使 CMake 能找到 Vulkan SDK。

生产应用依赖方向始终是应用 → Runtime/RHI/Core，不要包含 src/backends 下的文件，不要依赖测试支持库，也不要从样例中复制引擎实现。

## 6. 自动化接入验收

`integration.installed_sdk` 每次使用新的安装、迁移和消费者目录。测试核对包声明的后端选项与导入位置，再只用新前缀配置并构建消费者。开启 Null 时运行 CPU 示例，开启 OpenGL/Vulkan GPU 测试时分别执行对应的真实后端流程。关闭 Null 的 SDK 也运行相同的安装消费检查。

发布门槛仍包含：真实后端与 WSI、原生 Linux GPU、支持环境及依赖版本、错误与设备丢失恢复、接口/ABI 兼容策略、对应自动化证据。SDK 接入测试通过不能代替这些渲染验收。

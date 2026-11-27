# OpenGL 无窗口设备、缓冲与离屏绘制

`0.1.0-draft.24` 提供桌面 OpenGL 3.3 Core 的无窗口缓冲操作、离屏清屏、非索引三角形绘制和同步像素读回。Windows 在私有 WGL 隐藏窗口创建上下文。Linux 使用 EGL 1.5、`EGL_MESA_platform_surfaceless` 与私有 Pbuffer。Linux 测试可在没有 X11 和 Wayland 会话的环境运行。 [OpenGL 3.3 规范](https://registry.khronos.org/OpenGL/specs/gl/glspec33.core.pdf) · [EGL 无窗口平台](https://registry.khronos.org/EGL/extensions/MESA/EGL_MESA_platform_surfaceless.txt)

## 构建并运行

Windows 使用 Visual Studio C++ 工具和支持 OpenGL 3.3 Core 的显卡驱动。Linux 需要 EGL、GLVND 开发包和支持无窗口平台的驱动。Ubuntu 对应开发包为 `libegl-dev` 与 `libopengl-dev`。构建库时设置 `VORYX_ENABLE_OPENGL=ON`。另设置 `VORYX_BUILD_OPENGL_TESTS=ON` 启用真实设备测试。

```text
python tools/environment/build.py --preset windows-opengl-debug
python3 tools/environment/build.py --preset linux-opengl-debug
```

在软件驱动环境用 `linux-opengl-software-debug`。此预设允许软件渲染器，使用 Mesa 时可再设 `LIBGL_ALWAYS_SOFTWARE=1` 指定软件执行。开发构建的缓冲样例支持 `--opengl`，软件设备另传 `--allow-software`。成功输出驱动的 renderer 名称和字节复制验证结果。 [缓冲样例源码](../samples/null_buffers/main.cpp)

## 应用接入

安装 SDK 后使用 `find_package(Voryx CONFIG REQUIRED COMPONENTS runtime)`，链接 `Voryx::runtime`。CMake 变量 `Voryx_WITH_OPENGL` 表示 SDK 包含后端。Linux 消费端需要 EGL 和 OpenGL 开发包，以解析静态库的传递依赖。公共头无需包含平台或图形 SDK。 [SDK 接入](sdk-integration.md)

```cpp
#include <voryx/runtime/device_factory.hpp>

voryx::DeviceDesc desc{voryx::BackendType::OpenGL, "offscreen buffers"};
auto device = voryx::CreateDevice(desc);
```

检查 `Result` 后，按缓冲指南创建缓冲、录制用途屏障与复制、提交并读回。共享 RHI 校验句柄、范围、用途和整批命令，OpenGL 使用 `glCopyBufferSubData` 执行复制并在同步提交返回前等待完成。合法完成令牌可用 `Wait(token, 0)` 和 `IsComplete(token)` 查询。 [缓冲调用指南](rhi-buffer-api.md)

## 离屏清屏和读回

1. 通过 `CreateTexture` 创建 `Format::RGBA8_UNorm` 颜色附件。需要主机读回时，设置 `TextureUsage::ColorAttachment | TextureUsage::CopySrc`。
2. 创建命令列表并调用 `Begin()`。用 `TextureBarrier` 将附件从 `Initial` 转为 `ColorAttachment`。再次使用已读回的附件时，从 `CopySrc` 转回 `ColorAttachment`。
3. 调用 `BeginRenderPass({color, {1, 0, 0, 1}, {}, 1})` 和 `EndRenderPass()`。此处清除为纯红，深度句柄留空。
4. 将附件转为 `CopySrc`，调用 `End()` 后以默认同步方式 `Submit()`。
5. 用 `ReadTexture(color, output)` 读回。容量须恰好等于宽×高×4 字节。每行紧密排列，第一个像素是左上角的 RGBA。读回后可继续录制或使用 `Destroy(color)` 释放纹理。

每步检查 `Result`。设备所属线程管理 OpenGL 资源。样例 `voryx_opengl_clear` 检查 64×64 附件的全部 4096 个像素，每个像素都应为 `[255, 0, 0, 255]`。在 Visual Studio 中可选择该可执行目标启动。 [完整 C++ 示例](../samples/opengl_clear/main.cpp) · [附件接口与错误规则](rhi-draw-api.md)

安装 SDK 后，可单独编译其附带示例。以下在已初始化的 Windows MSVC x64 开发环境执行，Linux 替换成对应配置和路径。

```powershell
cmake --install build/windows-opengl-debug --prefix D:/VoryxSDK/windows-opengl-debug
cmake -S D:/VoryxSDK/windows-opengl-debug/share/Voryx/examples/opengl_clear -B build/sdk-opengl-clear -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=D:/VoryxSDK/windows-opengl-debug
cmake --build build/sdk-opengl-clear
./build/sdk-opengl-clear/opengl_clear.exe
```

显式允许软件 OpenGL 时传 `--allow-software`。 [SDK 接入](sdk-integration.md)

## 离屏三角形

1. 在 `GraphicsPipelineDesc` 填写 `vertexGlsl`、`fragmentGlsl`、顶点步长和属性布局，再调用 `CreateGraphicsPipeline`。Shader 使用 `#version 330 core`，顶点输入使用与布局一致的 `layout(location = ...)`，片元颜色输出使用 location 0。
2. 创建带 `Vertex` 用途的顶点缓冲。传初始数据时给缓冲加入 `CopyDst`，录制时从 `Initial` 转到 `VertexRead`。
3. 开始颜色 Pass，设置管线、顶点缓冲、视口和裁剪，再调用 `Draw(3)`。视口的 x、y、width、height 用整数像素，坐标从附件左上角起算。
4. 结束 Pass，将颜色附件转到 `CopySrc`，同步提交后 `ReadTexture`。按 `Result` 处理失败，再销毁附件、缓冲和管线。

源码使用原生 GLSL 裁剪空间。若输入来自 RHI 裁剪坐标 `p`（Y 向下、近端 Z 为 0），顶点输出写成 `vec4(p.x, -p.y, 2.0 * p.z - p.w, p.w)`。示例按此转换。视口、裁剪与读回的窗口坐标由后端适配。 [GLSL 3.30 规范](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.3.30.pdf) · [Shader 示例](../assets/shaders/opengl/triangle_glsl.hpp)

在 Visual Studio 选择 `voryx_opengl_triangle` 启动。独立 SDK 示例位于安装前缀的 `share/Voryx/examples/opengl_triangle`，使用与离屏清屏相同的 `CMAKE_PREFIX_PATH`，构建并运行 `opengl_triangle`。成功时会报告驱动名称，示例检查红色三角形和蓝色背景在约定坐标上的像素。软件执行时传 `--allow-software`。 [完整 C++ 示例](../samples/opengl_triangle/main.cpp) · [管线契约](rhi-pipeline-api.md)

## 能力和错误

- 在创建设备的线程内调用和销毁设备或命令对象。多个设备分别拥有上下文、缓冲及颜色附件。同一线程的调用会短暂激活引擎上下文。Linux 的多个设备在私有无窗口平台上共享显示连接的生命周期。宿主使用其他 EGL 平台连接时保持各自所有权，无窗口平台的显示连接由 VORYX 管理。
- 未编入 OpenGL 后端时，显式选择返回 `BackendNotBuilt`。上下文不可用、版本过低或驱动被拒绝时返回 `BackendUnavailable`。能识别的软件驱动默认拒绝，例如 llvmpipe、softpipe、Windows 通用软件像素格式和 SwiftShader。用 `GetInfo()` 查看驱动名称及软件标记。
- 颜色附件最多 256 个，单边上限取驱动限制与 4096 的较小值。管线最多 256 个，顶点属性最多 16 个，最大步长 2048 字节。
- 当前绘制范围为 RGBA8 颜色附件、不透明、无剔除的非索引三角形。深度、绑定布局、活动 Uniform 与采样暂不支持。GLSL 编译或链接失败返回 `InvalidArgument`，驱动诊断保存在 `Error::message`。分数视口、索引绘制和异步提交返回 `Unsupported`。
- `maxPendingSubmissions` 和 `maxPendingWindowFrames` 为 0，OpenGL 窗口 Presenter 待实现。设置 `enableValidation=true` 返回 `Unsupported`。原生 GL/EGL 错误可查看 `backendCode`。
- `Auto` 沿用当前 Vulkan 选择流程。使用 OpenGL 时显式填写 `BackendType::OpenGL`。

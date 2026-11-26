# 使用 Vulkan 缓冲后端

状态：0.1.0-draft.21，已实现设备缓冲、离屏异步提交、窗口双帧提交与限时关闭。通过同一 C++ API 创建真实 Vulkan 设备、上传 Buffer、执行原生复制和同步读回。本页覆盖无窗口设备与缓冲。离屏绘制和资源绑定见专题指南。 [绘制指南](rhi-draw-api.md) · [绑定指南](rhi-binding-api.md)窗口配置另可运行 [Win32 清屏](win32-clear.md) 、[X11 清屏](xlib-clear.md) 和 [Wayland 清屏](wayland-clear.md)。

## 1. 构建前提与运行

基线为 Vulkan 1.2。构建需要可被 CMake find_package(Vulkan 1.2) 发现的开发包，运行需要 loader 与兼容驱动。测试严格启用 VK_LAYER_KHRONOS_validation 与同步验证；缺组件按失败报告，不跳过。

Windows 安装 Vulkan SDK 后，普通终端运行：

```powershell
python tools/environment/build.py --preset windows-vulkan-debug
python tools/environment/build.py --preset windows-vulkan-release
```

Linux 先安装发行版的 Vulkan 开发包、loader、驱动和验证层。在本次 Ubuntu 环境补装的包是 libvulkan-dev、vulkan-tools、vulkan-validationlayers。已有硬件 ICD 时运行：

```sh
python3 tools/environment/build.py --preset linux-vulkan-debug
python3 tools/environment/build.py --preset linux-vulkan-release
```

WSL 本次只暴露 llvmpipe 软件 ICD，使用以下显式软件配置；不能将结果计入原生 Linux 硬件验收：

```sh
python3 tools/environment/build.py --preset linux-vulkan-software-debug
python3 tools/environment/build.py --preset linux-vulkan-software-release
```

X11 与 Wayland 各有独立的 Debug 和 Release 预设，硬件版与软件版使用不同的构建目录。硬件执行器配置和复现命令见 [持续集成](ci.md#真实-vulkan-硬件)。

这些配置包含默认 CPU/Null 检查、Vulkan 契约与直接运行样例。Win32、X11 和 Wayland 单窗口配置各增加窗口测试和七项图像像素测试。安装 SDK 消费会在独立程序中执行实际的离屏异步绘制并读回像素。测试结果和复现命令见环境报告。 [环境报告](environment.md)安装消费同时带有 integration 和 gpu_vulkan 标签。仅需构建库时启用 VORYX_ENABLE_VULKAN=ON 即可，VORYX_BUILD_GPU_TESTS 不必启用。Null 与 Vulkan 独立构建；当前共享测试夹具的 GPU 测试配置要求同时启用 Null。

Windows CTest 只对 GPU 测试进程设置 VK_IMPLICIT_LAYER_PATH 为构建目录下的空目录，避免第三方隐式叠加层污染。它保留显式 Khronos 验证层，不修改系统注册表或应用部署环境。本机曾因已删除的 Epic 叠加层清单产生 loader 错误，隔离后重新执行全部检查；未过滤或忽略验证错误。手动运行遇到同类问题时，可在该终端设置同一变量，关闭终端即可撤销。加载器规则见 [Khronos 文档](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderLayerInterface.md)。

## 2. 调用入口

在应用中包含 voryx/runtime/device_factory.hpp 并链接 Voryx::runtime：

```cpp
voryx::DeviceDesc desc;
desc.backend = voryx::BackendType::Vulkan;
desc.debugName = "My application";
desc.enableValidation = true;
auto created = voryx::CreateDevice(desc);
if (!created) {
    // 记录 errorCode、operation 和 backendCode；不要继续使用失败结果。
    return 1;
}
auto device = std::move(created).Value();
```

完整可构建流程见 [示例源码](../samples/null_buffers/main.cpp)，参数、所有权、线程及错误契约见 [缓冲 API](rhi-buffer-api.md)。默认排除 CPU 类型软件适配器；仅在明确需要时设置 allowSoftwareAdapter=true。选择顺序为独立、集成、其他非 CPU、软件设备；GetInfo().name 与 softwareAdapter 反映实际选择。Auto 当前也只会尝试已编入的 Vulkan，失败不会自动改用 Null。

同一个样例默认调用 Null；在已构建的 Vulkan 配置中运行：

```powershell
.\build\windows-vulkan-debug\samples\null_buffers\voryx_null_buffers.exe --vulkan --validation
```

Linux 可执行文件没有 .exe 后缀，路径使用斜线。软件环境额外传 --allow-software。成功输出实际设备信息及 Buffer copy verified.，返回 0；字节不一致、API 错误、验证层错误均返回 1，未知命令行参数返回 2。手动运行 Windows 样例时，遇到损坏的叠加层清单可先在当前 PowerShell 执行以下命令，然后重新运行样例。

```powershell
$env:VK_IMPLICIT_LAYER_PATH = (Resolve-Path .\build\windows-vulkan-debug\test-implicit-layers).Path
```

## 3. 执行与限制

每个 Buffer 独立分配 host-visible VkDeviceMemory 并持久映射，优先 coherent 内存；非 coherent 内存读回和局部写入前刷新主机视图，写入后将修改同步到设备可见内存。当前同步范围为完整分配。未给初始数据的存储内容未定义，跨后端代码必须主动初始化。

Submit 先完成共享 RHI 的整批状态校验，再录制原生屏障与 vkCmdCopyBuffer。重叠的目标写入按列表顺序同步；末尾建立设备写到主机读的依赖，提交到单图形队列。默认提交等待 fence；显式选择 Async 的离屏提交在队列接受时更新逻辑状态并返回令牌，完成前由提交记录持有临时命令池、fence 与资源引用。 [离屏异步提交](rhi-async-submit.md)

当前上限：单 Buffer 64 MiB、最多 min(1024, maxMemoryAllocationCount/2) 个 Buffer、每列表 4096 条记录、每批 64 列表；实际以 GetCapabilities 为准。Vertex、Index 和 Constant 缓冲可用于设备级离屏绘制。 [设备绘制](rhi-draw-api.md) · [设备绑定](rhi-binding-api.md)

已接受提交的等待失败，或主机写入后的内存刷新失败，均按 DeviceLost 终止后续工作并保留原始错误。实际物理拔卡、驱动重置和非 coherent 专用路径尚无独立覆盖证据。调用方应销毁命令和设备后重建。SDK 目前静态链接，消费 Vulkan 版本 SDK 也需 loader 链接开发包，见 [SDK 接入](sdk-integration.md)。

## 4. 验收边界

Windows 硬件、WSL 软件及 CPU 基础测试的配置、日期、报告位置集中见 [环境报告](environment.md)。软件结果不替代原生 Linux GPU，缓冲执行不替代 Win32/X11/Wayland 绘制与交换链验收。Win32 硬件、X11 与 Wayland 软件清屏已验收。首批三角形见 [Vulkan 三角形](vulkan-triangle.md)。单张 RGBA8 纹理采样见 [Vulkan 纹理](vulkan-texture.md)。彩色深度测试见 [深度指南](vulkan-depth.md)。固定彩色索引绘制见 [索引指南](vulkan-indexed.md)。窗口支持可复用单槽纹理绑定与包含常量缓冲的显式布局。设备离屏绘制已支持采样纹理、独立采样器和常量缓冲，窗口 Presenter 也可通过同一套设备命令获取窗口帧、提交和呈现同步或异步帧，以及取消尚未提交的帧。 [窗口帧](window-frame-api.md) [显式绑定布局](vulkan-binding-layout.md)

# 自动化测试与持续集成方案

状态：Core、Null 缓冲契约、安装 SDK 消费及质量检查已落地，本轮 Windows 与 Linux 的默认 CPU 配置各包含 72 项测试；最终证据见 [环境报告](environment.md)。另有显式启用的 Vulkan 缓冲执行套件；Win32、X11 与 Wayland 清屏已有窗口验收，已添加 GitHub Actions 工作流，远端执行记录待确认。硬件与软件 ICD 分别记录。

## 1. 工具与责任

实现阶段采用 CTest 统一调度，GoogleTest 编写 C++ 行为测试，通过 CMake 的 gtest_discover_tests 注册测试。测试依赖固定版本与校验值，在 M1 确认获取方式；不依赖不受控的在线最新分支，也不使生产库依赖测试框架。

每个功能和缺陷修复增加或复用能证明行为的自动用例。优先验证状态机、所有权、错误恢复、边界和跨后端语义；不写只镜像实现或只断言 true 的测试。

## 2. 测试套件

| 套件 / CTest 标签 | 位置 | 环境 | 主要验证 |
| --- | --- | --- | --- |
| unit | tests/unit/<module> | 无 GPU，无窗口 | 描述校验、溢出、句柄表、布局、状态规划等独立行为 |
| contract_null | tests/contract/rhi | Null | 公共接口、帧/命令状态、依赖、错误与清理 |
| integration | tests/integration | CPU 为主；需 GPU 的另标 gpu | 模块组装、后端开关、公共头、安装消费与文档示例 |
| gpu_vulkan（已实现） | tests/contract/rhi | 指定真实设备 | 同一套适用契约在真实后端执行 |
| gpu;<backend> | tests/gpu | 有窗口会话与驱动 | 图像、上传、呈现、屏障、在途释放、重建 |
| fault | 所属套件内 | 可控故障注入 | 内存不足、设备丢失、超时、部分后端创建失败 |

当前可执行标签为 cpu、quality、unit、contract_null、integration、gpu_vulkan、gpu_opengl 及 Vulkan WSI 标签。DX12 标签随真实用例加入。Vulkan 安装消费同时带 integration 与 gpu_vulkan，过滤 CPU 时必须排除整个 gpu_vulkan 标签。

共享场景参数化执行，后端差异只出现在夹具/适配中；用例逻辑不复制三套。样例可用于 smoke 验证，但不能替代自动断言。

启用 Win32 Vulkan 时，`integration.win32_clear` 带 `gpu_vulkan` 和 `wsi_win32` 标签。它要求可用的 Windows 桌面、GPU 和验证层。迁移安装测试额外编译并执行 SDK 随附的窗口示例。

X11 以 `integration.xlib_clear` 执行同类窗口用例，标签为 `gpu_vulkan` 和 `wsi_x11`。软件 ICD 配置需显式允许软件设备。缺少显示或验证层会报告失败。

Wayland 用例 `integration.wayland_clear` 带 `gpu_vulkan` 和 `wsi_wayland` 标签。它检查合成器 configure 以及宿主显式暂停，并从迁移 SDK 独立构建示例。WSLg 证据在清除 `DISPLAY` 的环境取得。

窗口验证还覆盖三角形配置、非法替换和缩放后继续绘制。`gpu.vulkan.VulkanTrianglePixels.DrawsTriangleOverClearColor` 通过原生 Vulkan 读回检查前景与背景，标签为 `gpu_vulkan`。

纹理像素用例 `gpu.vulkan.VulkanTrianglePixels.SamplesPaddedRgbaRowsWithoutFlipping` 检查四个象限的实际 RGBA。窗口验收额外验证非法行距和保留先前有效绘制。

深度像素用例 `gpu.vulkan.VulkanTrianglePixels.NearTriangleWinsInEitherVertexOrder` 在两种顶点顺序下检查近处、远处、重叠和背景像素。窗口验收覆盖非法深度和交换链重建后的绘制。

索引用例 `gpu.vulkan.VulkanTrianglePixels.IndexedQuadSharesVerticesAndCoversTheSeam` 检查两侧和共享边像素。窗口及独立 SDK 示例检查越界索引、非法数量和失败后继续呈现。

绑定用例 `gpu.vulkan.VulkanTrianglePixels.ReusesTextureWithDifferentSamplersAndChecksLifetimes` 验证同一纹理在不同采样器下的实际像素及资源生命周期。窗口与安装 SDK 用例检查重置绘制后复用。

## 3. 每次变更的检查流水线

| 顺序 | 检查 | 不满足时的行为 |
| --- | --- | --- |
| 1 | 文档链接与追踪、根格式配置可解析、源码格式差异 | 失败即阻止合入 |
| 2 | clang-tidy 括号检查，包含缺括号负例自检 | 配置未生效或发现遗漏均失败 |
| 3 | 干净配置和 Debug / Release 构建，检查公共头及依赖 | 不能将配置失败当成跳过 |
| 4 | CPU unit、Null contract、适用 integration | 非零退出、崩溃、超时均失败 |
| 5 | 对受影响系统/WSI/后端执行 GPU contract / gpu | 在专用 Windows 或 Linux GPU runner 上运行，Linux X11/Wayland 分别覆盖 |
| 6 | 保存测试报告、验证日志和必要图像 | 发布/必需任务缺证据视为未通过 |

PR/合入始终要求 1–4；后端实现或同步语义变更还要求受影响真实后端的 5。发布要求所有声明支持后端的适用测试，普通无 GPU runner 不能替代。

GitHub Actions 的 CPU 矩阵和手动 Vulkan 硬件任务调用同一个本地构建入口。该入口检查预设对应的必需用例、套件和完整结果。 [持续集成](ci.md)

从 M1 起 Windows/Linux 均执行 1–4，不能只用 Windows 结果判定可移植性。GPU 用例增加 windows / linux 和 x11 / wayland 标签，发布覆盖 [目标矩阵](cross-platform.md) 的七个真实组合。WSL 或无显示服务器 runner 不能自动替代原生 Linux 桌面/GPU 结果。

## 4. 执行、过滤与零测试

先按环境指南构建对应 preset。下面命令以已构建的 Vulkan Debug 为例；多配置生成器额外传 -C Debug 或 -C Release。

```text
ctest --test-dir build/windows-vulkan-debug --output-on-failure --no-tests=error -LE gpu_vulkan
ctest --test-dir build/windows-vulkan-debug --output-on-failure --no-tests=error -L gpu_vulkan
```

第一个命令排除 gpu_vulkan，包含无 GPU 的 unit、Null 和适用集成测试；第二个只执行已注册的 Vulkan 用例。CI 还须核对预期标签清单，防止过滤错误导致某个必需套件没有注册却被其他套件的结果掩盖。

零测试必须失败。测试标记 skipped 与 passed 分开统计；无驱动、缺 SDK、无桌面会话或缺测试依赖不能返回成功冒充验证。可选探索任务允许显式跳过，但必需 GPU/发布任务的预期用例被跳过则该任务未满足门槛。

## 5. 确定性、超时与隔离

- 固定随机种子、输入资产与 Shader 版本；失败时记录种子，不依赖当前时间和测试执行顺序。
- 用例有超时，等待尽量基于完成标记或条件；不使用长时间 sleep 假装同步正确。
- CPU 用例可并行；窗口/GPU/驱动敏感用例按测试设备分组串行或使用 CTest 资源锁，避免相互抢占。
- 每个测试使用独立临时目录、Runtime 和资源；清理失败也报告，不污染下一用例。
- 图像差异同时保存参考、实际、差异图和阈值；更新参考图须说明功能变化并人工审查，不能自动接受当前输出。
- 注入故障的设备/工厂仅存在测试边界，不向正式 API 增加任意作弊开关。
- 内存/未定义行为检查采用工具链实际支持的配置；不将尚未运行的 sanitizer 写成已验证。

T41 使用离屏读回验证同一绑定集内的常量缓冲与独立纹理和采样器。窗口及迁移安装用例通过 `--verify` 调用公开接口，验证资源配置后可呈现、缩放和恢复。 [显式布局功能规格](features/vulkan-binding-layout.md)

T42 使用 Null/Vulkan 共享管线契约，真实 GPU 验证自定义顶点偏移、同管线复用与剔除及混合像素。安装消费测试额外编译独立管线程序。 [管线规格](features/rhi-pipeline.md)

T43/T44 覆盖公开设备绘制流程。Null/Vulkan 共享录制、附件、引用与整批校验，Vulkan 另外验证两种索引宽度、顶点偏移、裁剪与深度读回。安装后的独立程序也执行实际绘制。 [设备绘制规格](features/rhi-draw.md)

T45 复用 Null/Vulkan 契约验证设备资源绑定、生命周期和失败恢复。T46 使用独立 SDK 消费程序验证两次索引绘制的纹理颜色、常量变换和深度遮挡。 [设备绑定规格](features/rhi-binding.md)

T47 通过三个宿主窗口样例及迁移安装程序验证公开窗口帧流程。T48 验证离屏附件拒绝 Present 后的错误恢复。 [窗口帧规格](features/window-frame.md)

T49 用可控的完成状态复现 Pending、等待超时、队列满后重试，并通过 Null 验证异步拒绝。T50 使用 Vulkan 验证层执行两批依赖复制和在途释放，并通过迁移安装的独立程序验证异步绘制像素。 [异步提交规格](features/rhi-async-submit.md)

T51 扩展现有窗口集成用例与迁移安装 SDK 的窗口样例。在首帧纹理索引绘制后，连续异步清屏并呈现，直到两个帧槽都轮换复用。检查错误令牌、提前释放命令包装，以及留有异步窗口帧时切换入口和析构。 [窗口双帧规格](features/window-async-frames.md)

T52 的可控完成源确定性验证资源忙、有限超时、查询仍可用及再次关闭。窗口与迁移安装 SDK 用例验证两次尺寸变化后用新交换链异步呈现、错误线程拒绝、存在未呈现工作时关闭、关闭后拒绝新工作。 [关闭规格](features/window-shutdown.md)

T53 在 `gpu_opengl` 标签下复用缓冲与命令契约，并检查两个 OpenGL 上下文的设备隔离、销毁顺序以及当前能力边界。迁移后的 SDK 通过 `find_package` 编译并运行 `--opengl` 的字节复制。 [OpenGL 缓冲规格](features/opengl-buffers.md)

T54 在三后端复用附件契约，OpenGL 另验证两张附件的颜色隔离与销毁顺序。仓库内及迁移安装后的 `opengl_clear` 分别清除并检查 4096 个 RGBA8 像素。 [OpenGL 清屏规格](features/opengl-clear.md)

T55 验证 GLSL 编译、链接、失败恢复、非零顶点偏移与首顶点、左上角视口与裁剪，以及批次拒绝后保留颜色。仓库内和迁移安装后的示例使用同一 GLSL 与 C++ 源码验证三角形像素。 [OpenGL 绘制规格](features/opengl-draw.md)

T56 使用可控完成源验证零等待、保留帧提交和继续等待已取得图像。Win32 安装示例也通过 `CreateFramePresenter` 执行公共窗口帧流程。

T57 检查缺少首选后端时的 `Auto` 选择、零容量错误码、关闭 Null 后的 GPU 契约和全新 SDK 安装消费。 [窗口帧规格](features/window-frame.md) · [SDK 安装规格](features/sdk-package.md)

## 6. 报告与通过条件

记录测试总数、通过、失败、跳过、未执行及耗时；绑定代码版本、契约版本、配置、工具链、后端、GPU 与驱动。失败日志须含用例 ID、操作、期望和实际结果。

CTest 清单、GoogleTest XML 与 CTest 汇总报告位于 build/<preset>/reports/，详细日志位于 Testing/Temporary/。Actions 归档这两个目录。GPU 用例另存验证层日志、图像和故障信息。

格式、括号检查、无 GPU 行为测试和必需后端验证共同决定通过；不以单一覆盖率百分比或一张截图替代验收。覆盖率可用于发现遗漏，但不作为唯一质量结论。

## 7. 实施时间点

- M1：两平台建立 CMake/CTest、固定依赖、跨平台 quality 入口、CPU/Null 首批用例和 CI；增加通过与失败负例验证检查链路有效。
- M2：Vulkan 分别覆盖 Windows/Win32、Linux/X11、Linux/Wayland 的真实图形和交换链异常路径。
- M3：Windows DX12、两平台各 WSI 的 OpenGL 执行共同用例，接入每组合 GPU 验证。
- M4：在全新环境运行用户样例，检查全部发布门槛和证据。

在 M1 前，文档检查和工具探针只说明文档/配置质量，不能宣称自动化功能测试体系已经落地。

参考：[CMake GoogleTest 集成](https://cmake.org/cmake/help/latest/module/GoogleTest.html)、[CTest 调度](https://cmake.org/cmake/help/latest/manual/ctest.1.html)、[GoogleTest CMake 快速入门](https://github.com/google/googletest/blob/main/docs/quickstart-cmake.md)。

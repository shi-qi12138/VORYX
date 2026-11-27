# 持续集成与 Vulkan 硬件验收

CI 使用 GitHub Actions。工作流复用本地的 `tools/environment/build.py`，采用同一套 CMake 预设和 CTest 阈值。首次托管 CI 在初始提交上通过四套 CPU 配置。 [首次运行](https://github.com/shi-qi12138/VORYX/actions/runs/45766123021) [GitHub 执行环境](https://docs.github.com/en/actions/how-tos/write-workflows/choose-where-workflows-run/choose-the-runner-for-a-job)

## 每次代码变更

`VORYX Build and Test` 在 push 和 PR 上执行，也支持手动运行。它并行覆盖四个 CPU / Null 配置。

| GitHub 执行环境 | Debug | Release |
| --- | --- | --- |
| Windows Server 2025 / Visual Studio 2026 | `windows-debug` | `windows-release` |
| Ubuntu 26.04 | `linux-debug` | `linux-release` |

配置包括构建、公共头检查、格式与完整括号检查、CPU 单元及 Null 契约、迁移安装后的独立 SDK 消费。Linux 使用镜像内的 LLVM 21，Windows 使用 Visual Studio 附带的工具。 [Windows 镜像](https://github.com/actions/runner-images/blob/main/images/windows/Windows2025-VS2026-Readme.md) · [Ubuntu 镜像](https://github.com/actions/runner-images/blob/main/images/ubuntu/Ubuntu2604-Readme.md) · [自动化测试](automated-testing.md)

首次运行后，可在仓库合并规则中选择四个对应的状态检查。工作流定义见 [build-test.yml](../.github/workflows/build-test.yml)。

## 真实 Vulkan 硬件

`VORYX Vulkan Hardware` 从默认分支手动运行，每次选择一个 Release 预设。为使用真实桌面会话的 x64 自托管执行器添加对应标签，再在仓库的 Actions 页面选取预设。专用执行器用于受信任代码。 [硬件工作流](../.github/workflows/vulkan-hardware.yml) · [GitHub 自托管执行器安全说明](https://docs.github.com/en/actions/reference/security/secure-use#hardening-for-self-hosted-runners)

| 验收环境 | 预设 | 自托管标签 |
| --- | --- | --- |
| Windows 原生硬件，Win32 桌面 | `windows-vulkan-release` | `voryx-windows-vulkan-release` |
| 原生 Linux 硬件，X11 桌面 | `linux-vulkan-x11-release` | `voryx-linux-vulkan-x11-release` |
| 原生 Linux 硬件，Wayland 桌面 | `linux-vulkan-wayland-release` | `voryx-linux-vulkan-wayland-release` |

执行器需准备 C++ 工具链、CMake、Ninja、Python、LLVM、Vulkan 开发包、验证层与可用驱动。Win32 要在可创建窗口的登录会话中运行。X11 会话必须能访问 DISPLAY，Wayland 会话需继承 XDG_RUNTIME_DIR 和合成器套接字环境。Wayland 任务会清除 DISPLAY。硬件预设拒绝软件适配器，Linux 硬件任务也拒绝 WSL。 [Vulkan 环境](vulkan-backend.md) · [跨平台范围](cross-platform.md)

本地可通过下列命令执行相同验证。Linux 可用 `python3`，原生 Wayland 验证先清除 DISPLAY。

```text
python tools/environment/build.py --preset windows-vulkan-release
python3 tools/environment/build.py --preset linux-vulkan-x11-release
python3 tools/environment/build.py --preset linux-vulkan-wayland-release
```

用于 WSLg 或软件驱动的 Release 预设为 `linux-vulkan-x11-software-release` 和 `linux-vulkan-wayland-software-release`。验证记录应注明软件驱动及运行环境。 [环境报告](environment.md)

## 查阅验证结果

本地和 CI 均会先保存 CTest 注册清单 `build/<preset>/reports/ctest-plan.json`，确认预设包含必需测试和套件，再运行 CTest 并验证 `ctest.xml`。没有测试、必需用例缺失、被跳过、失败、超时或报告不完整均会令入口失败。Actions 归档 `reports/` 与 `Testing/Temporary/`，便于下载完整测试记录。

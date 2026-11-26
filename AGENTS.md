# VORYX 项目约束

## 工作范围与文档

- 已获授权按工程规范进入 M1 实现；Core 为首批模块，按功能规格逐步交付真实实现与测试，不生成无效果的占位实现。实际验证入口见 docs/environment.md。
- 阅读 docs/README.md 后按任务读取对应权威文档；模块边界以 docs/architecture.md 为准，目录规则以 docs/repository-layout.md 为准。
- 公共接口变更须同步接口契约、用户指南、功能规格、测试与变更记录；不得将未实现功能标为可用。

## 目录与依赖

- 公共头放 include/voryx/<module>/，私有头与实现放 src/<module>/；不得从公共头包含私有头或具体图形 SDK。
- 每个实现模块独立 CMake target，后端各自独立；RHI 不依赖 Runtime/Renderer/后端，后端互不引用。
- Renderer 不判断 BackendType，不直接调用具体图形 API。创建与选择后端集中在 Runtime。
- 不新增全局设备、万能 Manager、无边界 utils/common 目录或未使用的抽象。
- 首版同时面向 Windows/Linux；平台和 WSI 规则见 docs/cross-platform.md。Core/RHI/Renderer 不散布平台宏或包含 Windows/X11/Wayland 系统头，差异放私有适配。
- Windows 构建不能代替 Linux 验证；DX12 仅纳入 Windows，Linux Vulkan/OpenGL 分别覆盖 X11 与原生 Wayland。
- 工程交付物是可被其他项目接入的 C++ SDK；同时维护安装导出、find_package 接入和独立消费测试，不能仅用仓库内样例代替库接入验收。

## C++ 格式与结构

- 根目录 .clang-format 是 C++ 格式的唯一来源；使用 -style=file，不替换样式、不添加子目录覆盖、不为代码通过检查而修改格式配置。
- if、else、for、范围 for、while、do 等语句体即使只有一条语句也必须写完整 {}，空循环也必须有明确块与说明；允许常规 else if 链，各分支主体必须有 {}。
- 不压缩代码，不省略空格或把多个语句堆在一行。括号位置、缩进、空格、指针对齐服从现有 .clang-format。
- .clang-tidy 的 readability-braces-around-statements 检查必须通过；不能仅以 clang-format 通过代替括号检查。
- 测试、样例和工具中的 C++ 遵守相同样式；第三方和生成文件另行隔离。
- 实现原则见 docs/code-style.md；职责、所有权、线程与失败行为必须明确。
- 自有代码的说明性注释、公共接口说明及脚本说明使用中文；标识符、API 名称和格式工具生成的 namespace 结构标记保留原样，不修改第三方注释。

## 自动化验证

- 实现阶段采用 CTest 调度 GoogleTest，分离 CPU 单元测试、Null 契约测试、集成测试和真实 GPU 测试；具体要求见 docs/automated-testing.md。
- 功能与缺陷修复应增加或复用与风险匹配的自动测试，优先检查可观察行为、错误恢复与生命周期，不写仅复刻实现的测试。
- 零测试、缺测试依赖、所需 GPU 不可用均不能记为测试通过；必须区分通过、失败、跳过和未执行。
- CI 检查格式与括号、构建、CPU/Null 测试；发布必须有每个声明支持后端的 GPU 验证证据。
- 无测试框架或后端实现时，如实记录尚未执行，不用空测试或恒真断言制造通过结果。

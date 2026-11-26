# 目录、文件与构建结构

状态：Core、缓冲 RHI/Null/Vulkan、工厂、SDK 安装、样例及测试已落地；其余按实际功能创建，不批量生成占位实现。

## 1. 目标布局

```text
VORYX/
  AGENTS.md                       项目开发约束
  README.md                       项目和文档入口
  .clang-format                   现有 C++ 格式唯一来源
  .clang-tidy                     C++ 静态检查，强制控制语句括号
  CMakeLists.txt                  顶层选项与子模块组装
  CMakePresets.json               可共享的配置/构建/测试预设
  .github/workflows/              GitHub Actions 流程，调用本地构建入口
  cmake/                          编译器、依赖、安装与公共构建函数
  include/voryx/                  可安装的公共头
    core/
    platform/
    rhi/
    renderer/                     后续版本按需要引入
    runtime/
  src/
    core/
    platform/
      windows/
      linux/
        x11/
        wayland/
    rhi/
    backends/
      common/                     仅内部后端创建契约
      null/
      opengl/
        wsi/                      WGL / GLX / EGL 平台私有适配
      vulkan/
        wsi/                      Win32 / Xlib / Wayland Surface 适配
      dx12/
    render_graph/                 Renderer 内部实现，后续引入
    renderer/
    runtime/
  tools/
    environment/                  跨平台配置、构建和环境测试入口
    shader_compiler/              离线 Shader 工具
    quality/                      格式、静态检查与文档检查入口
  tests/
    environment/                  编译器/标准库真实行为验证
    unit/                         无窗口无 GPU 的单元测试，子目录对应模块
    contract/rhi/                 Null 与真实后端共用契约用例
    integration/                  Runtime、构建消费与模块组合
    gpu/                          真实绘制、呈现和同步测试
    support/                      仅测试夹具、故障注入与辅助代码
    assets/                       小型固定测试输入与基准
  samples/                        用户可独立运行的接入示例
  assets/shaders/                 样例和后端一致性共用 Shader 源码
  docs/
    templates/
  third_party/                    按依赖方案引入的第三方；不手改其样式
  build/<preset>/                 生成产物、临时 Shader、测试报告
```

根构建、质量工具、Core、缓冲 RHI/Null/Vulkan、工厂和 SDK 安装已落地；其余目录仍是目标布局。已有真实 Vulkan 缓冲执行；Vulkan 设备绘制覆盖离屏与同步窗口帧。异步帧、完整 Runtime 和远程 CI 继续推进。

系统、WSI 与后端组合见 [跨平台设计](cross-platform.md)。Linux 不生成 DX12 target；启用不受支持的组合必须在配置阶段明确报错。平台文件名、include 大小写与路径在两平台保持一致，不以拷贝整个后端实现实现移植。

## 2. 文件归属

| 文件类别 | 位置与规则 |
| --- | --- |
| 公共接口 | include/voryx/<module>/*.hpp；只依赖已声明的公共模块 |
| 私有声明 | src/<module>/*.hpp；不安装，也不通过公共 include 路径暴露 |
| 实现 | src/<module>/*.cpp；默认与主要声明同名 |
| 平台差异 | src/platform/<platform>/；图形后端内的平台细节可放该后端的 platform 子目录 |
| 单元测试 | tests/unit/<module>/*_test.cpp；以模块和行为组织 |
| 后端共用用例 | tests/contract/rhi/*_test.cpp；参数化后端，不复制三份断言 |
| GPU 场景 | tests/gpu/<scenario>_test.cpp；场景资产与期望单独存放 |
| 用户示例 | samples/<scenario>/；每个示例有运行说明，不依赖 tests/support |
| 文档 | docs/ 中按主题组织，功能专题随实际需求增加 |
| 临时结果 | build/<preset>/；不得写回 include、src、测试基准或源码资产 |

普通文件采用 snake_case，C++ 类型名保持 PascalCase；例：device.hpp / vulkan_device.cpp / resource_lifetime_test.cpp。跨文件包含使用项目相对路径，不使用层层 ../。

一个文件围绕一个主要职责；小型紧密相关的值类型可放同一头文件。达到难以独立理解或测试的程度时按职责拆分，不机械按行数拆出更多管理器。

禁止把不相关功能集中到 utils.hpp、common.hpp 或 manager.cpp。backends/common 只放创建契约，不能变成绕开模块依赖的共享实现集合；tests/support 只能被测试使用。

## 3. include 与实现组织

- 公共头自包含，使用统一的 #pragma once，不向调用方要求包含顺序。
- 实现文件先包含对应头，再包含必要依赖；include 的实际排序和间距由 .clang-format 决定。
- 前置声明仅用于合法的借用或 PImpl 关系；需要完整类型时直接包含，不用不完整声明掩盖依赖。
- 模板必需实现随声明放置；普通实现放 .cpp，避免大面积内联增加依赖和构建成本。
- SDK 类型与转换函数放具体后端私有目录；共享头不包含 windows.h、vulkan.h 或 d3d12.h。
- 不为方便创建全项目万能头；可按真实用户入口提供边界清楚的聚合头。

## 4. 构建 target 约定

| 目标名（拟定） | 范围 |
| --- | --- |
| voryx_core / voryx_platform | 基础与平台实现 |
| voryx_rhi_api | INTERFACE，公共 RHI 契约 |
| voryx_rhi | 已实现的后端无关缓冲状态、管线句柄与校验、引用和整批提交校验 |
| voryx_backend_null / opengl / vulkan / dx12 | 每个完整前缀分别为一个后端 target |
| voryx_runtime | 链接已启用的后端工厂并完成组装 |
| voryx_renderer / voryx_render_graph | 后续版本实际需要时引入 |
| voryx_shader_compiler | 离线工具，不作为 Renderer 链接依赖 |
| voryx_unit_tests / voryx_contract_tests / voryx_integration_tests / voryx_gpu_tests | 测试可执行目标，按依赖与运行环境分离 |

发布消费端使用 Voryx::<component> 命名空间别名；确切导出名在 M1 冻结。不能在 target 尚不存在时在用户指南中写成可执行命令。

测试只能链接被测模块和测试支持库，生产模块不能反向链接测试或样例。构建产物与生成头放 build 目录，禁止源码目录内构建。每个模块就近维护 CMakeLists，不在顶层集中罗列所有源码。

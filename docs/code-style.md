# C++ 代码结构与格式规范

状态：自有实现已采用根格式配置与中文说明性注释；原 [.clang-format](../.clang-format) 保持不变，括号由 [.clang-tidy](../.clang-tidy) 检查。

## 1. 格式来源与本地验证

所有自有 C++（引擎、测试、样例、工具）使用仓库根 .clang-format，不使用 IDE 默认样式、不创建子目录样式覆盖，不为解决格式问题修改现有配置。

自有代码说明性注释、公共接口注释及脚本说明统一使用中文，描述所有权、前置条件、失败与同步原因。API/标识符、诊断字符串和格式工具自动生成的 namespace 结构标记保留原样，第三方代码不擅自翻译。

2026-11-04 已使用本机 clang-format 12.0.0 成功解析当前配置，并用内存样例确认以下输出。此结果只证明当前配置兼容该工具，不代表已经验证整个 C++20 项目。

| 规则 | 当前配置行为 |
| --- | --- |
| 缩进与制表符 | 4 空格，UseTab=Never |
| 行宽 | ColumnLimit=80，由 formatter 决定必要换行 |
| 函数、类、结构体、命名空间左括号 | 独立换行 |
| 控制语句左括号 | 与 if / for / while 等所在行同行 |
| 短函数、短控制语句、短块 | 不压成单行 |
| 控制语句圆括号前空格 | if (condition)、for (...) |
| 指针对齐 | Right，例如 Device *device |
| 圆括号与方括号内部 | 不增加首尾空格 |
| 连续空行 | 最多保留 1 行 |

配置中的 Standard: Cpp11 是格式工具选项，不将编译语言从 C++20 改为 C++11。工具升级须先验证现有配置、C++20 语法和格式差异，再统一开发机与 CI 版本。当前机上的 clang-format 12 与 clang-tidy 16 为不同版本，不能将其路径当成项目已冻结的工具链。

## 2. 必须写完整括号

if / else / for / 范围 for / while / do 的主体必须有 {}，即使只有 return、continue、break 或一条赋值也不省略。空循环不得写成容易误读的孤立分号；有必要的空块要说明用途。常规 else if 链可以连写，各实际分支体仍需 {}。

不在一行拼接多条语句，不压缩运算符两侧或控制语句所需空格。实际括号位置与换行由根配置决定，不统一改成其他风格。

以下为纯风格示例，不是引擎实现：

```cpp
bool IsValid(bool ready)
{
    if (ready) {
        return true;
    } else {
        return false;
    }
}
```

现有 .clang-format 控制排版，但没有设置自动插入缺失 {}。因此增加 clang-tidy 的 readability-braces-around-statements，并将 ShortStatementLines 设为 0，相关诊断作为错误。不能靠格式检查替代此项检查。宏和预处理条件仍须人工审查，不使用宏隐藏控制流程。

2026-11-04 已用 clang-tidy 16.0.0 验证根配置：完整括号样例退出码 0，省略括号样例退出码 1 且报告对应检查错误；同一正例符合现有格式。探针源文件已清理，没有增加引擎源码。这只证明工具规则有效，不等同于引擎测试或 CI 已建立。

参考：[clang-format 选项](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)、[括号检查说明](https://clang.llvm.org/extra/clang-tidy/checks/readability/braces-around-statements.html)。

## 3. 代码职责与可读性

- 函数围绕一项可描述的操作，分离输入校验、状态规划、后端执行与状态提交；失败时保持约定状态。
- 类负责自己的不变量，不能同时承担设备发现、窗口消息、资源缓存和场景绘制。
- 早返回同样使用 {}；避免为了少写行数引入多层三元表达式和隐式副作用。
- 名称表达用途和单位；字节偏移使用 offsetBytes，时间使用 timeoutNs，不用含义不明的数字。
- RAII 管理 CPU 所有权；GPU 在途生命周期遵守提交记录与延迟回收，不能混为同一件事。
- 注释说明原因、同步前提与边界，不重复代码表面行为；公共接口说明参数、失败与生命周期。
- 不以 shared_ptr、全局单例、宏或万能上下文替代明确的依赖和所有权。
- 私有实现不为测试随意公开；通过可替换边界、契约或故障注入验证行为。

## 4. 格式与静态检查流程

统一 quality 入口已落地于 tools/quality/check.py，并由 CMake 的 voryx_quality target 和 CTest 调度，复现命令见 [环境搭建](environment.md)。它枚举自有 .hpp/.cpp/.h/.cc 文件，排除 third_party 和 build；当前只有检查模式，不自动修改源码或提交修复。

格式检查使用 -style=file 与 -fallback-style=none；以仓库为工作目录，对标准输入指定真实文件名，确保使用根配置。选择支持的版本后使用 dry-run/Werror 或格式化结果逐字比较，使格式差异返回非零状态。

括号检查使用根 .clang-tidy 配合真实 compile_commands.json（CMAKE_EXPORT_COMPILE_COMMANDS=ON 的受支持生成器），覆盖自有翻译单元以及相关自有头。统一入口须使用仓库实际绝对路径限定 header-filter，防止外部库恰好有 include/src 同名目录而被纳入。至少有一个负例证明遗漏 {} 会失败，不能仅以配置能解析作为规则生效的证据。

因宏等原因确需局部抑制时，必须写明原因并评审，不允许全局禁用括号检查或对整目录跳过。第三方与生成文件排除规则须显式列出，不能借此排除普通源码。

# Core 错误与诊断示例

状态：已实现，可构建。代码见 [main.cpp](main.cpp)。示例演示输入校验、Result 成功/失败分支以及 noexcept 日志回调，不创建 GPU 或窗口。

在项目根目录运行构建验证：

```text
python tools/environment/build.py --preset windows-debug
```

Windows 运行 build/windows-debug/samples/core_diagnostics/voryx_core_diagnostics.exe；Linux 在完成 linux-debug 构建后运行 build/linux-debug/samples/core_diagnostics/voryx_core_diagnostics。Release 替换对应目录名。

预期输出：

```text
InvalidArgument
ValidateCount
Count must be greater than zero.
Core diagnostics sample completed.
```

退出码为 0，表示已正确处理预期的非法输入，并验证有效输入；意外分支返回 1。该行为纳入 integration.core_diagnostics 用例，不用异常代替普通校验错误。

VS 2026 以文件夹模式打开仓库，选择 windows-debug 和 voryx_core_diagnostics 启动目标，在 main 或 ValidateCount 设置断点后按 F5。也可以选择 voryx_core_tests 调试单元测试。

头文件、参数与生命周期说明见 [Core API](../../docs/core-api.md)。

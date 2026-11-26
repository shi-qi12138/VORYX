# Core API 使用说明

版本：0.1.0-draft.4。状态：Core 已实现，公共头独立且只依赖 C++20。缓冲 RHI/Null 见 [相应接口](rhi-buffer-api.md)，真实 GPU 尚未实现。

## 1. 文件与模块

| 文件 | 提供内容 |
| --- | --- |
| [error.hpp](../include/voryx/core/error.hpp) | ErrorCode、Error、GetErrorCodeName |
| [result.hpp](../include/voryx/core/result.hpp) | Result<T>、Result<void> |
| [log.hpp](../include/voryx/core/log.hpp) | LogLevel、LogCallback、LogSink |

非模板实现只放 src/core；没有平台/图形 SDK、全局设备、全局 Logger 或后台线程。调用者无需安装 GoogleTest 才能使用 Core。

## 2. Error

Error 是拥有数据的值类型，字段如下：

| 字段 | 类型 / 默认值 | 规则 |
| --- | --- | --- |
| errorCode | ErrorCode / InternalError | 错误分类；完整分类见 [公共契约](api-reference.md) |
| operation | string / 空 | 操作名称，创建后不借用输入字符串 |
| message | string / 空 | UTF-8 详细说明，允许为空 |
| backendCode | optional<int64_t> / 无 | 保留原始后端码，不用 0 假装“没有码” |

ErrorCode 底层为 uint32_t，InvalidArgument 至 InternalError 的数值依公共契约顺序固定为 1–17，无 Success 项。GetErrorCodeName(ErrorCode) noexcept 返回 string_view，内容为枚举名或 Unknown，存储期为静态，不需释放。

Error 的字符串构造和复制可抛 std::bad_alloc；移动不抛异常。内存不足时允许只构造 OutOfMemory 码和空字符串，不强行分配诊断。Core 不自动把任意 C++ 异常转换成 Error，后续业务 API 边界负责转换。

## 3. Result 创建与检查

| 操作 | 签名 / 说明 |
| --- | --- |
| 值成功 | Result<T>::Success(T value) noexcept；移动持有输入值 |
| 无值成功 | Result<void>::Success() noexcept |
| 失败 | Result<T>::Failure(Error error) noexcept，void 版本相同 |
| 判断 | bool HasValue() const noexcept；explicit operator bool() const noexcept |
| 值左值访问 | T& Value() & noexcept / const T& Value() const & noexcept |
| 值右值访问 | T&& Value() && noexcept；用于移动取出 unique_ptr 等值 |
| void 确认成功 | void Value() const noexcept |
| 错误左值访问 | const Error& GetError() const & noexcept |
| 错误右值访问 | Error&& GetError() && noexcept；移交完整错误 |

所有 Result 均禁止默认构造和赋值；通过工厂一次确定分支，再为新操作创建新变量。T 必须为非 cv 的非数组对象，且无异常移动构造/析构，不允许引用；void 使用专门版本。引用借用的接口应直接返回约定的引用，不能用 Result<T&> 绕过所有权设计。

Result 可移动且不抛异常；只有 T 可复制时 Result<T> 才可复制，复制可能抛出 T/Error 的复制异常。工厂的 noexcept 不意味着调用前构造参数、或调用后复制取出的值不会抛异常。

先检查分支，再访问；Value 在失败分支、GetError 在成功分支都会在 Debug/Release 下调用 std::terminate，不返回默认对象，也不抛可恢复异常。此类错误是调用方编程错误。

引用只在对应 Result 存活且未被移动取出期间可借用；右值引用不延长临时对象生命周期。const 临时 Result 的引用访问被禁止。移动后原结果保留原分支，其字符串或指针按普通移动后语义处理。

错误传播可以使用 Result<void>::Failure(std::move(result).GetError())，完整保留 operation/message/backendCode，前提是 result 已确认失败。

## 4. LogSink

| 操作 | 契约 |
| --- | --- |
| 默认构造 | LogSink() noexcept，禁用输出 |
| 回调构造 | explicit LogSink(LogCallback, void* userData=nullptr, LogLevel minimumLevel=Debug) noexcept |
| 过滤查询 | bool IsEnabled(LogLevel) const noexcept |
| 写入 | void Write(LogLevel, string_view message) const noexcept，同步完成，不保存 message |

LogCallback 的完整类型是 void (*)(LogLevel, string_view, void*) noexcept，回调必须声明 noexcept。LogLevel 为 uint8_t 枚举 Debug=0、Info=1、Warning=2、Error=3。低于最低等级、未知等级或空回调不分发；未知最低等级禁用出口。

LogSink 不拥有 userData，也不复制 message 到内部缓存；message 可含内嵌零字节，回调按长度处理。默认无格式化、文件输出或颜色控制。Write 不分配内部内存，但回调自身的成本与资源需求由宿主负责。

配置构造后无 setter。同一个 const sink 可并发调用，宿主负责回调和 userData 的线程安全，并确保所有调用结束前 userData 有效；不能并发销毁/赋值 sink。没有异步队列需要排空，调用返回即结束本次回调。错误码仍由 Result 提供，日志不能替代错误返回。

## 5. 构建与调试

根配置默认构建 Core、样例和测试。统一命令见 [环境文档](environment.md)。在 VS 2026 打开根目录、选择 windows-debug，启动目标选择 voryx_core_diagnostics；在其 main.cpp 设置断点即可观察成功/失败处理。

独立应用通过 add_subdirectory 引入本仓库并链接 Voryx::core。作为子项目时默认关闭 VORYX_BUILD_TESTS / VORYX_BUILD_SAMPLES / VORYX_ENABLE_QUALITY_CHECKS；这三个选项也可显式 OFF。此模式不探测 Python、LLVM 或 GoogleTest。C++20 使用要求通过 target 传播，警告选项只应用于自有 target。

当前已提供可安装静态 SDK 和 find_package(Voryx CONFIG) 配置，见 [SDK 接入](sdk-integration.md)。不承诺跨编译器 ABI，MSVC 消费程序须与库的配置、运行库和工具链相容。

可运行代码与输出见 [Core 样例](../samples/core_diagnostics/README.md)，开发依据见 [F02 功能设计](features/core-foundation.md)。

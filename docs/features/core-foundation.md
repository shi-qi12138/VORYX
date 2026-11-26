# F02：Core 错误结果与诊断基础

状态：首批实现已落地，目标阶段 M1；只包含错误值、Result 和同步日志出口，不包含设备、RHI 资源或全局日志服务。验证结果见 [环境报告](../environment.md)，调用契约见 [Core API](../core-api.md)。

## 模块与依赖

公共头位于 include/voryx/core，非模板实现位于 src/core。voryx_core 是独立静态库，对外别名 Voryx::core，只依赖 C++20 标准库。测试和样例单向依赖它。

## 结果契约

- Error 拥有 operation/message 字符串，保存 ErrorCode 与可选 int64 后端码；不借用调用者临时文本。
- ErrorCode 使用固定 uint32 底层类型，错误不包含 Success 项；GetErrorCodeName 返回静态字符串视图，未知枚举返回 Unknown。
- Result<T> 仅由 Success(T) 或 Failure(Error) 创建；禁止默认构造和赋值，防止结果对象在重绑定时出现无有效分支的中间状态。
- T 为非 cv 的非数组对象，须可无异常移动构造并可无异常析构；支持 unique_ptr 等移动类型。需要复制时，T 自身必须支持复制；复制可能抛出其复制/分配异常。
- Result<void> 提供 Success() 与 Failure(Error)。两个版本均可复制（受 T 限制）与移动，移动不抛异常。
- HasValue 和显式 bool 判断分支。Value 与 GetError 使用前须判断；错误分支访问在 Debug/Release 中均调用 std::terminate，不依赖可关闭的 assert。
- Value 的左值返回借用引用，右值支持移动取出；GetError 左值只读，右值支持移交错误；对 const 临时值禁止取得引用。
- 移动后原对象仍属于原分支，但其内容是有效的移动后状态，不承诺保留原指针/字符串。
- Error/值的构造和复制是普通 C++ 值操作，可能分配失败；Result 不自动捕获异常。未来渲染操作边界负责错误转换，内存不足时可返回只含 OutOfMemory 码的空诊断。

## 诊断契约

- LogCallback 使用 noexcept 函数指针，参数为 LogLevel、仅在回调期间有效的 string_view 和借用的 void* userData。
- LogSink 是不可配置修改的轻量值对象；构造时确定回调、userdata 与最低等级，Write 同步、无内部分配，空回调为禁用。
- 等级为 Debug / Info / Warning / Error；默认最低 Debug，未知等级不分发，未知最低等级禁用出口。
- 多线程可并发调用同一个 const LogSink；回调与 userdata 的同步由宿主负责。所有 Write 返回前不得销毁 userdata，不允许并发销毁或赋值 LogSink。
- 不创建后台线程、文件、锁或单例；不持有 Runtime，也不解释渲染业务错误。

## 用户与兼容性

新增 Core 参考文档和可编译 core_diagnostics 样例。原 draft.2 未发布，补充 Result 的约束与 noexcept 日志回调，不存在已发布 ABI 迁移；运行时接口仍未实现。

## 验收

覆盖成功/失败、void、移动对象、错误文本所有权、异常复制、错误分支终止、日志过滤、禁用和并发回调；每个公共头独立编译。Windows/MSVC 与 Ubuntu WSL2/GCC 的 Debug/Release 构建、格式与括号检查全部通过。原生 Linux GPU 和远程 CI 不属于本功能的验证证据。

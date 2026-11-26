#pragma once

#include <cstdint>
#include <string_view>

namespace voryx
{
enum class LogLevel : std::uint8_t
{
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
};

/// 消息仅在本次调用期间有效；回调不得抛出异常。
using LogCallback = void (*)(LogLevel level, std::string_view message,
                             void *userData) noexcept;

/// 同步且不拥有用户数据的日志出口；用户数据须存活到所有并发调用结束。
class LogSink
{
public:
    LogSink() noexcept = default;
    explicit LogSink(LogCallback callback, void *userData = nullptr,
                     LogLevel minimumLevel = LogLevel::Debug) noexcept;

    [[nodiscard]] bool IsEnabled(LogLevel level) const noexcept;
    void Write(LogLevel level, std::string_view message) const noexcept;

private:
    LogCallback callback = nullptr;
    void *userData = nullptr;
    LogLevel minimumLevel = LogLevel::Debug;
};
} // namespace voryx

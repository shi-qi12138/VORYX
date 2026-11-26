#include <voryx/core/log.hpp>

namespace voryx
{
LogSink::LogSink(LogCallback callback, void *userData,
                 LogLevel minimumLevel) noexcept
    : callback(callback)
    , userData(userData)
    , minimumLevel(minimumLevel)
{
}

bool LogSink::IsEnabled(LogLevel level) const noexcept
{
    return callback != nullptr && minimumLevel <= LogLevel::Error
           && level <= LogLevel::Error && level >= minimumLevel;
}

void LogSink::Write(LogLevel level, std::string_view message) const noexcept
{
    if (IsEnabled(level)) {
        callback(level, message, userData);
    }
}
} // namespace voryx

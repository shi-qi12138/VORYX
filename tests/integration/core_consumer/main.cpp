#include <voryx/core/error.hpp>
#include <voryx/core/log.hpp>
#include <voryx/core/result.hpp>

int main()
{
    const auto result = voryx::Result<int>::Success(42);
    const voryx::LogSink disabled;
    if (!result || result.Value() != 42
        || disabled.IsEnabled(voryx::LogLevel::Error)) {
        return 1;
    }
    if (voryx::GetErrorCodeName(voryx::ErrorCode::Timeout) != "Timeout") {
        return 1;
    }
    return 0;
}

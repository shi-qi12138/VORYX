#include <voryx/core/log.hpp>
#include <voryx/core/result.hpp>

#include <cstddef>
#include <cstdio>
#include <string_view>

namespace
{
voryx::Result<std::size_t> ValidateCount(std::size_t count)
{
    if (count == 0) {
        return voryx::Result<std::size_t>::Failure(
            {voryx::ErrorCode::InvalidArgument,
             "ValidateCount",
             "Count must be greater than zero.",
             {}});
    }
    return voryx::Result<std::size_t>::Success(count);
}

void PrintLog(voryx::LogLevel, std::string_view message, void *) noexcept
{
    std::fwrite(message.data(), 1, message.size(), stdout);
    std::fputc('\n', stdout);
}
} // namespace

int main()
{
    const voryx::LogSink sink(PrintLog);
    const auto rejected = ValidateCount(0);
    if (rejected) {
        return 1;
    }
    const auto &error = rejected.GetError();
    sink.Write(voryx::LogLevel::Error,
               voryx::GetErrorCodeName(error.errorCode));
    sink.Write(voryx::LogLevel::Error, error.operation);
    sink.Write(voryx::LogLevel::Error, error.message);

    const auto accepted = ValidateCount(3);
    if (!accepted || accepted.Value() != 3) {
        return 1;
    }
    sink.Write(voryx::LogLevel::Info, "Core diagnostics sample completed.");
    return 0;
}

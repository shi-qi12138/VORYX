#include <voryx/core/log.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace voryx
{
namespace
{
struct Capture
{
    std::array<char, 64> bytes{};
    std::size_t size = 0;
    int count = 0;
    LogLevel level = LogLevel::Debug;
};

void CaptureLog(LogLevel level, std::string_view message, void *data) noexcept
{
    auto &capture = *static_cast<Capture *>(data);
    capture.level = level;
    capture.count += 1;
    capture.size = message.copy(capture.bytes.data(), capture.bytes.size());
}

TEST(LogTest, EmptySinkIsDisabled)
{
    const LogSink sink;
    EXPECT_FALSE(sink.IsEnabled(LogLevel::Error));
    sink.Write(LogLevel::Error, "Ignored");
}

TEST(LogTest, FiltersBeforeInvokingCallback)
{
    Capture capture;
    const LogSink sink(CaptureLog, &capture, LogLevel::Warning);
    sink.Write(LogLevel::Debug, "Hidden");
    sink.Write(LogLevel::Info, "Hidden");
    EXPECT_EQ(capture.count, 0);
    sink.Write(LogLevel::Warning, "Visible");
    sink.Write(LogLevel::Error, "Failure");
    EXPECT_EQ(capture.count, 2);
    EXPECT_EQ(capture.level, LogLevel::Error);
    EXPECT_EQ(std::string_view(capture.bytes.data(), capture.size), "Failure");
}

TEST(LogTest, MessageIsLengthDelimitedIncludingEmbeddedNull)
{
    Capture capture;
    const LogSink sink(CaptureLog, &capture);
    const std::string_view message("a\0b", 3);
    sink.Write(LogLevel::Info, message);
    EXPECT_EQ(capture.count, 1);
    EXPECT_EQ(std::string_view(capture.bytes.data(), capture.size), message);
}

TEST(LogTest, UnknownLevelsDoNotInvokeUserCode)
{
    Capture capture;
    const LogSink sink(CaptureLog, &capture);
    sink.Write(static_cast<LogLevel>(255), "Invalid");
    const LogSink invalid(CaptureLog, &capture, static_cast<LogLevel>(255));
    invalid.Write(LogLevel::Error, "Invalid minimum");
    EXPECT_EQ(capture.count, 0);
}

void CountLog(LogLevel, std::string_view, void *data) noexcept
{
    static_cast<std::atomic<int> *>(data)->fetch_add(1,
                                                     std::memory_order_relaxed);
}

TEST(LogTest, ConcurrentWritesUseCallerSynchronizedData)
{
    std::atomic<int> count = 0;
    const LogSink sink(CountLog, &count);
    std::vector<std::jthread> threads;
    for (int index = 0; index < 4; ++index) {
        threads.emplace_back([&sink]() {
            for (int entry = 0; entry < 100; ++entry) {
                sink.Write(LogLevel::Info, "Message");
            }
        });
    }
    threads.clear();
    EXPECT_EQ(count.load(std::memory_order_relaxed), 400);
}
} // namespace
} // namespace voryx

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <latch>
#include <ranges>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace voryx
{
namespace
{
TEST(Toolchain, SpanAndRanges)
{
    const std::array values = {1, 2, 3, 4, 5, 6};
    const std::span<const int> view(values);
    auto even = view.subspan(1) | std::views::filter([](int value) {
                    return value % 2 == 0;
                });
    std::vector<int> actual;
    std::ranges::copy(even, std::back_inserter(actual));
    EXPECT_EQ(actual, (std::vector<int>{2, 4, 6}));
}

TEST(Toolchain, ThreadSynchronization)
{
    std::latch completed(1);
    int result = 0;
    std::jthread worker([&]() {
        result = 42;
        completed.count_down();
    });
    completed.wait();
    EXPECT_EQ(result, 42);
}

TEST(Toolchain, Utf8FilesystemRoundTrip)
{
    const std::filesystem::path name(u8"环境验证-路径.txt");
    const auto file = std::filesystem::current_path() / name;
    {
        std::ofstream stream(file, std::ios::binary);
        ASSERT_TRUE(stream.is_open());
        stream << "VORYX C++20";
        ASSERT_TRUE(stream.good());
    }
    {
        std::ifstream stream(file, std::ios::binary);
        std::string contents;
        std::getline(stream, contents);
        EXPECT_EQ(contents, "VORYX C++20");
    }
    std::error_code error;
    EXPECT_TRUE(std::filesystem::remove(file, error));
    EXPECT_FALSE(error);
}
} // namespace
} // namespace voryx

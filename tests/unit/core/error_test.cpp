#include <voryx/core/error.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

namespace voryx
{
namespace
{
TEST(ErrorTest, OwnsDiagnosticTextAndPreservesBackendCode)
{
    std::string operation = "CreateBuffer";
    std::string message = "Allocation failed on device";
    const Error error{ErrorCode::OutOfMemory, operation, message, -17};
    operation.clear();
    message.clear();
    EXPECT_EQ(error.operation, "CreateBuffer");
    EXPECT_EQ(error.message, "Allocation failed on device");
    ASSERT_TRUE(error.backendCode.has_value());
    EXPECT_EQ(*error.backendCode, -17);
}

TEST(ErrorTest, SupportsCodeOnlyDiagnostics)
{
    const Error error{ErrorCode::OutOfMemory, {}, {}, {}};
    EXPECT_TRUE(error.operation.empty());
    EXPECT_TRUE(error.message.empty());
    EXPECT_FALSE(error.backendCode.has_value());
}

TEST(ErrorTest, AllDefinedCodesHaveStableNames)
{
    using Entry = std::pair<ErrorCode, std::string_view>;
    const std::array entries = {
        Entry{ErrorCode::InvalidArgument, "InvalidArgument"},
        Entry{ErrorCode::InvalidHandle, "InvalidHandle"},
        Entry{ErrorCode::WrongDevice, "WrongDevice"},
        Entry{ErrorCode::WrongThread, "WrongThread"},
        Entry{ErrorCode::InvalidState, "InvalidState"},
        Entry{ErrorCode::ResourceBusy, "ResourceBusy"},
        Entry{ErrorCode::BackendNotBuilt, "BackendNotBuilt"},
        Entry{ErrorCode::BackendUnavailable, "BackendUnavailable"},
        Entry{ErrorCode::SurfaceIncompatible, "SurfaceIncompatible"},
        Entry{ErrorCode::Unsupported, "Unsupported"},
        Entry{ErrorCode::OutOfMemory, "OutOfMemory"},
        Entry{ErrorCode::NotReady, "NotReady"},
        Entry{ErrorCode::Timeout, "Timeout"},
        Entry{ErrorCode::SurfaceUnavailable, "SurfaceUnavailable"},
        Entry{ErrorCode::SwapchainOutOfDate, "SwapchainOutOfDate"},
        Entry{ErrorCode::DeviceLost, "DeviceLost"},
        Entry{ErrorCode::InternalError, "InternalError"},
    };
    for (const auto &[code, name] : entries) {
        EXPECT_EQ(GetErrorCodeName(code), name);
    }
}

TEST(ErrorTest, UnknownNumericCodeIsDiagnosable)
{
    EXPECT_EQ(GetErrorCodeName(static_cast<ErrorCode>(0)), "Unknown");
    EXPECT_EQ(GetErrorCodeName(static_cast<ErrorCode>(UINT32_MAX)), "Unknown");
}
} // namespace
} // namespace voryx

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace voryx
{
enum class ErrorCode : std::uint32_t
{
    InvalidArgument = 1,
    InvalidHandle = 2,
    WrongDevice = 3,
    WrongThread = 4,
    InvalidState = 5,
    ResourceBusy = 6,
    BackendNotBuilt = 7,
    BackendUnavailable = 8,
    SurfaceIncompatible = 9,
    Unsupported = 10,
    OutOfMemory = 11,
    NotReady = 12,
    Timeout = 13,
    SurfaceUnavailable = 14,
    SwapchainOutOfDate = 15,
    DeviceLost = 16,
    InternalError = 17,
};

/// 错误对象自行持有诊断文本；内存不足时允许仅保留错误码。
struct Error
{
    ErrorCode errorCode = ErrorCode::InternalError;
    std::string operation;
    std::string message;
    std::optional<std::int64_t> backendCode;
};

/// 返回静态存储期的名称视图；未知错误码返回 "Unknown"。
[[nodiscard]] std::string_view GetErrorCodeName(ErrorCode code) noexcept;
} // namespace voryx

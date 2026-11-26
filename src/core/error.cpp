#include <voryx/core/error.hpp>

namespace voryx
{
std::string_view GetErrorCodeName(ErrorCode code) noexcept
{
    switch (code) {
        case ErrorCode::InvalidArgument:
            return "InvalidArgument";
        case ErrorCode::InvalidHandle:
            return "InvalidHandle";
        case ErrorCode::WrongDevice:
            return "WrongDevice";
        case ErrorCode::WrongThread:
            return "WrongThread";
        case ErrorCode::InvalidState:
            return "InvalidState";
        case ErrorCode::ResourceBusy:
            return "ResourceBusy";
        case ErrorCode::BackendNotBuilt:
            return "BackendNotBuilt";
        case ErrorCode::BackendUnavailable:
            return "BackendUnavailable";
        case ErrorCode::SurfaceIncompatible:
            return "SurfaceIncompatible";
        case ErrorCode::Unsupported:
            return "Unsupported";
        case ErrorCode::OutOfMemory:
            return "OutOfMemory";
        case ErrorCode::NotReady:
            return "NotReady";
        case ErrorCode::Timeout:
            return "Timeout";
        case ErrorCode::SurfaceUnavailable:
            return "SurfaceUnavailable";
        case ErrorCode::SwapchainOutOfDate:
            return "SwapchainOutOfDate";
        case ErrorCode::DeviceLost:
            return "DeviceLost";
        case ErrorCode::InternalError:
            return "InternalError";
    }
    return "Unknown";
}
} // namespace voryx

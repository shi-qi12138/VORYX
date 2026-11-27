#pragma once

#include <error_context.hpp>
#include <voryx/core/result.hpp>
#include <voryx/rhi/types.hpp>

#include <array>
#include <new>
#include <string_view>
#include <utility>

namespace voryx::detail
{
inline bool MayTryNextBackend(ErrorCode code) noexcept
{
    return code == ErrorCode::BackendNotBuilt
           || code == ErrorCode::BackendUnavailable
           || code == ErrorCode::Unsupported
           || code == ErrorCode::SurfaceIncompatible;
}

template<typename T, typename Create>
Result<T> SelectBackend(BackendType requested, std::string_view operation,
                        Create create) noexcept
{
    if (requested > BackendType::DX12) {
        return Result<T>::Failure(
            WithOperation({ErrorCode::InvalidArgument, {}, {}, {}}, operation));
    }
    if (requested != BackendType::Auto) {
        return create(requested);
    }

    constexpr std::array candidates{
        std::pair{BackendType::Vulkan, std::string_view{"Vulkan"}},
#if VORYX_PLATFORM_WINDOWS
        std::pair{BackendType::DX12, std::string_view{"DX12"}},
#endif
        std::pair{BackendType::OpenGL, std::string_view{"OpenGL"}},
    };
    Error primary{ErrorCode::BackendNotBuilt, {}, {}, {}};
    std::array<ErrorCode, candidates.size()> failures{};
    for (std::size_t slot = 0; slot < candidates.size(); ++slot) {
        auto result = create(candidates[slot].first);
        if (result) {
            return result;
        }
        auto error = std::move(result).GetError();
        if (!MayTryNextBackend(error.errorCode)) {
            return Result<T>::Failure(std::move(error));
        }
        failures[slot] = error.errorCode;
        if (primary.errorCode == ErrorCode::BackendNotBuilt) {
            primary = std::move(error);
        }
    }
    try {
        if (!primary.message.empty()) {
            primary.message.append(". ");
        }
        primary.message.append("Auto: ");
        for (std::size_t slot = 0; slot < candidates.size(); ++slot) {
            if (slot != 0) {
                primary.message.append(", ");
            }
            primary.message.append(candidates[slot].second)
                .append("=")
                .append(GetErrorCodeName(failures[slot]));
        }
    } catch (const std::bad_alloc &) {
        // 保留首个有效诊断和原始错误码。
    }
    return Result<T>::Failure(WithOperation(std::move(primary), operation));
}
} // namespace voryx::detail

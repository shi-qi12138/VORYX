#pragma once

#include <error_context.hpp>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <voryx/core/result.hpp>

namespace voryx::rhi_detail
{
template<typename T>
Result<T> Fail(ErrorCode code) noexcept
{
    return Result<T>::Failure({code, {}, {}, {}});
}

template<typename T, typename Action>
Result<T> Invoke(std::string_view operation, Action &&action) noexcept
{
    try {
        auto result = action();
        if (result) {
            return result;
        }
        return Result<T>::Failure(
            detail::WithOperation(std::move(result).GetError(), operation));
    } catch (const std::bad_alloc &) {
        return Fail<T>(ErrorCode::OutOfMemory);
    } catch (const std::length_error &) {
        return Fail<T>(ErrorCode::OutOfMemory);
    }
}

} // namespace voryx::rhi_detail

#pragma once

#include <error_context.hpp>
#include <voryx/core/result.hpp>

#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace voryx::opengl_backend
{
struct Failure
{
    ErrorCode code;
    std::optional<std::int64_t> nativeCode = {};
    std::string message{};
};

template<typename T>
Result<T> Fail(ErrorCode code) noexcept
{
    return Result<T>::Failure({code, {}, {}, {}});
}

template<typename T, typename Action>
Result<T> Invoke(std::string_view operation, Action &&action) noexcept
{
    try {
        return action();
    } catch (Failure &error) {
        return Result<T>::Failure(detail::WithOperation(
            {error.code, {}, std::move(error.message), error.nativeCode},
            operation));
    } catch (const std::bad_alloc &) {
        return Fail<T>(ErrorCode::OutOfMemory);
    } catch (const std::length_error &) {
        return Fail<T>(ErrorCode::OutOfMemory);
    }
}
} // namespace voryx::opengl_backend

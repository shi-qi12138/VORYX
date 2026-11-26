#pragma once

#include <voryx/core/error.hpp>

#include <exception>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace voryx
{
/// 一次操作对应一个固定结果分支；访问前须判断，不支持赋值重绑定。
template<typename T>
class [[nodiscard("Check the operation result before continuing")]] Result
{
    static_assert(std::is_object_v<T> && !std::is_array_v<T>);
    static_assert(!std::is_const_v<T> && !std::is_volatile_v<T>);
    static_assert(std::is_nothrow_move_constructible_v<T>);
    static_assert(std::is_nothrow_destructible_v<T>);

public:
    Result(const Result &) = default;
    Result(Result &&) noexcept = default;
    Result &operator=(const Result &) = delete;
    Result &operator=(Result &&) = delete;
    ~Result() = default;

    [[nodiscard]] static Result Success(T value) noexcept
    {
        return Result(std::in_place_index<0>, std::move(value));
    }

    [[nodiscard]] static Result Failure(Error error) noexcept
    {
        return Result(std::in_place_index<1>, std::move(error));
    }

    [[nodiscard]] bool HasValue() const noexcept
    {
        return storage.index() == 0;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return HasValue();
    }

    [[nodiscard]] T &Value() & noexcept
    {
        RequireValue();
        return std::get<0>(storage);
    }

    [[nodiscard]] const T &Value() const & noexcept
    {
        RequireValue();
        return std::get<0>(storage);
    }

    [[nodiscard]] T &&Value() && noexcept
    {
        RequireValue();
        return std::get<0>(std::move(storage));
    }

    const T &&Value() const && = delete;

    [[nodiscard]] const Error &GetError() const & noexcept
    {
        RequireError();
        return std::get<1>(storage);
    }

    [[nodiscard]] Error &&GetError() && noexcept
    {
        RequireError();
        return std::get<1>(std::move(storage));
    }

    const Error &&GetError() const && = delete;

private:
    explicit Result(std::in_place_index_t<0>, T value) noexcept
        : storage(std::in_place_index<0>, std::move(value))
    {
    }

    explicit Result(std::in_place_index_t<1>, Error error) noexcept
        : storage(std::in_place_index<1>, std::move(error))
    {
    }

    void RequireValue() const noexcept
    {
        if (!HasValue()) {
            std::terminate();
        }
    }

    void RequireError() const noexcept
    {
        if (HasValue()) {
            std::terminate();
        }
    }

    std::variant<T, Error> storage;
};

template<>
class [[nodiscard("Check the operation result before continuing")]] Result<void>
{
public:
    Result(const Result &) = default;
    Result(Result &&) noexcept = default;
    Result &operator=(const Result &) = delete;
    Result &operator=(Result &&) = delete;
    ~Result() = default;

    [[nodiscard]] static Result Success() noexcept
    {
        return Result();
    }

    [[nodiscard]] static Result Failure(Error error) noexcept
    {
        return Result(std::move(error));
    }

    [[nodiscard]] bool HasValue() const noexcept
    {
        return !error.has_value();
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return HasValue();
    }

    void Value() const noexcept
    {
        if (!HasValue()) {
            std::terminate();
        }
    }

    [[nodiscard]] const Error &GetError() const & noexcept
    {
        RequireError();
        return *error;
    }

    [[nodiscard]] Error &&GetError() && noexcept
    {
        RequireError();
        return *std::move(error);
    }

    const Error &&GetError() const && = delete;

private:
    Result() noexcept = default;

    explicit Result(Error error) noexcept
        : error(std::move(error))
    {
    }

    void RequireError() const noexcept
    {
        if (HasValue()) {
            std::terminate();
        }
    }

    std::optional<Error> error;
};
} // namespace voryx

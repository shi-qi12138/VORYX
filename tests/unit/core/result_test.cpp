#include <voryx/core/result.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

namespace voryx
{
namespace
{
static_assert(!std::is_default_constructible_v<Result<int>>);
static_assert(!std::is_copy_assignable_v<Result<int>>);
static_assert(!std::is_move_assignable_v<Result<int>>);
static_assert(
    std::is_nothrow_move_constructible_v<Result<std::unique_ptr<int>>>);
static_assert(!std::is_copy_constructible_v<Result<std::unique_ptr<int>>>);
static_assert(std::is_nothrow_move_constructible_v<Result<void>>);
static_assert(!std::is_default_constructible_v<Result<void>>);
static_assert(!std::is_copy_assignable_v<Result<void>>);

TEST(ResultTest, SuccessProvidesMutableAndConstBorrowing)
{
    auto result = Result<int>::Success(7);
    ASSERT_TRUE(result.HasValue());
    EXPECT_TRUE(static_cast<bool>(result));
    result.Value() = 9;
    const auto &borrowed = result;
    EXPECT_EQ(borrowed.Value(), 9);
}

TEST(ResultTest, MoveOnlyValueTransfersOwnership)
{
    auto original =
        Result<std::unique_ptr<int>>::Success(std::make_unique<int>(42));
    auto moved = std::move(original);
    EXPECT_TRUE(original.HasValue());
    EXPECT_EQ(original.Value(), nullptr);
    auto value = std::move(moved).Value();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 42);
}

TEST(ResultTest, CopiedSuccessHasIndependentPayload)
{
    auto original = Result<std::string>::Success("first");
    auto copy = original;
    original.Value() = "changed";
    EXPECT_EQ(copy.Value(), "first");
}

TEST(ResultTest, FailureCanBePropagatedWithoutLosingMetadata)
{
    auto original = Result<int>::Failure(
        {ErrorCode::DeviceLost, "Submit", "Device removed", -100});
    ASSERT_FALSE(original);
    auto propagated = Result<void>::Failure(std::move(original).GetError());
    ASSERT_FALSE(propagated.HasValue());
    EXPECT_EQ(propagated.GetError().errorCode, ErrorCode::DeviceLost);
    EXPECT_EQ(propagated.GetError().operation, "Submit");
    EXPECT_EQ(propagated.GetError().message, "Device removed");
    EXPECT_EQ(propagated.GetError().backendCode, -100);
}

TEST(ResultTest, ErrorCanAlsoBeASuccessfulValue)
{
    const auto result = Result<Error>::Success(
        {ErrorCode::Timeout, "Wait", "Diagnostic payload", {}});
    ASSERT_TRUE(result);
    EXPECT_EQ(result.Value().errorCode, ErrorCode::Timeout);
}

TEST(ResultTest, VoidDistinguishesSuccessAndFailure)
{
    const auto success = Result<void>::Success();
    ASSERT_TRUE(success);
    success.Value();
    const auto failure =
        Result<void>::Failure({ErrorCode::Timeout, {}, {}, {}});
    EXPECT_FALSE(failure);
    EXPECT_EQ(failure.GetError().errorCode, ErrorCode::Timeout);
}

class ThrowingCopy
{
public:
    ThrowingCopy() = default;
    ThrowingCopy(const ThrowingCopy &)
    {
        throw std::runtime_error("copy failure");
    }
    ThrowingCopy(ThrowingCopy &&) noexcept = default;
};

TEST(ResultTest, ThrowingCopyDoesNotInvalidateSourceOutcome)
{
    const auto original = Result<ThrowingCopy>::Success(ThrowingCopy{});
    EXPECT_THROW((void)Result<ThrowingCopy>(original), std::runtime_error);
    EXPECT_TRUE(original.HasValue());
}

TEST(ResultTest, CopiedFailureOwnsItsDiagnostic)
{
    const auto copy = []() {
        const auto original = Result<int>::Failure(
            {ErrorCode::InvalidArgument, "Validate", "Bad size", {}});
        return Result<int>(original);
    }();
    ASSERT_FALSE(copy);
    EXPECT_EQ(copy.GetError().message, "Bad size");
}

TEST(ResultDeathTest, ValueOnFailureTerminates)
{
    const auto failure = Result<int>::Failure({ErrorCode::Timeout, {}, {}, {}});
    EXPECT_DEATH((void)failure.Value(), "");
}

TEST(ResultDeathTest, ErrorOnSuccessTerminates)
{
    const auto success = Result<int>::Success(1);
    EXPECT_DEATH((void)success.GetError(), "");
}

TEST(ResultDeathTest, VoidValueOnFailureTerminates)
{
    const auto failure =
        Result<void>::Failure({ErrorCode::Timeout, {}, {}, {}});
    EXPECT_DEATH(failure.Value(), "");
}

TEST(ResultDeathTest, VoidErrorOnSuccessTerminates)
{
    const auto success = Result<void>::Success();
    EXPECT_DEATH((void)success.GetError(), "");
}
} // namespace
} // namespace voryx

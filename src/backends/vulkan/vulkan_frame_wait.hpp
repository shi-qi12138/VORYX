#pragma once

#include <voryx/rhi/types.hpp>

#include <algorithm>
#include <chrono>
#include <limits>
#include <optional>

namespace voryx::vulkan_backend
{
/// 获取与关闭复用同一时间预算。预算用尽后仍可作零等待查询。
class FrameWaitBudget
{
public:
    explicit FrameWaitBudget(TimeoutNs timeout) noexcept
        : timeout(timeout)
    {
    }

    [[nodiscard]] TimeoutNs Remaining() const noexcept
    {
        if (timeout == std::numeric_limits<TimeoutNs>::max()) {
            return timeout;
        }
        const auto elapsed = static_cast<TimeoutNs>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start)
                .count());
        return timeout - std::min(timeout, elapsed);
    }

private:
    const TimeoutNs timeout;
    const std::chrono::steady_clock::time_point start =
        std::chrono::steady_clock::now();
};

/// 已取得图像但尚未完成同步时保留索引。下次只等待同一张图像。
template<typename WaitSlot, typename AcquireImage, typename WaitImage>
std::uint32_t AcquireImageWithinBudget(std::optional<std::uint32_t> &image,
                                       const FrameWaitBudget &budget,
                                       WaitSlot waitSlot, AcquireImage acquire,
                                       WaitImage waitImage)
{
    if (!image) {
        waitSlot(budget.Remaining());
        image = acquire(budget.Remaining());
    }
    waitImage(budget.Remaining());
    return *image;
}
} // namespace voryx::vulkan_backend

#pragma once

#include <atomic>
#include <cstdint>
#include <limits>

namespace voryx::detail
{
/// 窗口与无窗口设备共用唯一标识来源，防止相同槽位的句柄跨设备别名。
inline std::uint64_t AllocateDeviceIdentity() noexcept
{
    static std::atomic<std::uint64_t> next{1};
    auto current = next.load(std::memory_order_relaxed);
    while (current != std::numeric_limits<std::uint64_t>::max()) {
        if (next.compare_exchange_weak(current, current + 1,
                                       std::memory_order_relaxed)) {
            return current;
        }
    }
    return 0;
}
} // namespace voryx::detail

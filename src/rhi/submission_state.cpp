#include "buffer_state.hpp"

#include <algorithm>
#include <exception>
#include <limits>

namespace voryx::rhi_detail
{
namespace
{
BackendSubmission &FindCompletion(State &state, std::uint64_t serial) noexcept
{
    const auto found =
        std::ranges::find(state.pending, serial, &PendingSubmission::serial);
    if (found == state.pending.end()) {
        std::terminate();
    }
    return *found->native;
}
} // namespace

void State::CompleteThrough(std::uint64_t serial) noexcept
{
    while (!pending.empty() && pending.front().serial <= serial) {
        auto &record = pending.front();
        record.native->AcknowledgeCompletion();
        record.native.reset();
        for (const auto &operation : record.operations) {
            operation.ReleaseInFlightRefs();
        }
        pending.pop_front();
    }
    completedSubmission = serial;
}

Result<bool> State::PollSubmission(std::uint64_t serial) noexcept
{
    if (serial <= completedSubmission) {
        return Result<bool>::Success(true);
    }
    auto complete = FindCompletion(*this, serial).Poll();
    if (!complete) {
        if (complete.GetError().errorCode == ErrorCode::DeviceLost) {
            lost = true;
        }
        return complete;
    }
    if (complete.Value()) {
        CompleteThrough(serial);
    }
    return complete;
}

Result<void> State::WaitForSubmission(std::uint64_t serial,
                                      TimeoutNs timeoutNs) noexcept
{
    if (serial <= completedSubmission) {
        return Result<void>::Success();
    }
    auto complete = FindCompletion(*this, serial).Wait(timeoutNs);
    if (complete) {
        CompleteThrough(serial);
    } else if (complete.GetError().errorCode == ErrorCode::DeviceLost) {
        lost = true;
    }
    return complete;
}

void State::DrainSubmissions() noexcept
{
    if (pending.empty()) {
        return;
    }
    const auto finished =
        pending.back().native->Wait(std::numeric_limits<TimeoutNs>::max());
    if (finished) {
        CompleteThrough(lastSubmission);
    } else {
        // 每条原生记录的兜底清理先于记录持有的资源释放。
        pending.clear();
    }
}
} // namespace voryx::rhi_detail

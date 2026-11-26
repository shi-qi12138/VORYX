#include "buffer_command_list.hpp"

#include <exception>
#include <thread>
#include <utility>

namespace voryx::rhi_detail
{
BufferCommandList::BufferCommandList(std::shared_ptr<State> device)
    : device(std::move(device))
{
    ++this->device->openCommandLists;
}

BufferCommandList::~BufferCommandList()
{
    if (std::this_thread::get_id() != device->owner) {
        std::terminate();
    }
    ReleaseReferences();
    --device->openCommandLists;
}

void Operation::ReleaseRecordingRefs() const noexcept
{
    if (src) {
        --src->recordingRefs;
    }
    if (dst) {
        --dst->recordingRefs;
    }
    if (texture) {
        --texture->recordingRefs;
    }
    if (depth) {
        --depth->recordingRefs;
    }
    if (bindingSet) {
        bindingSet->ReleaseRecordingRefs();
    }
    if (pipeline) {
        --pipeline->recordingRefs;
    }
}

void Operation::TransferToSubmission(std::uint64_t serial) const noexcept
{
    ReleaseRecordingRefs();
    for (const auto *reference : {&src, &dst}) {
        if (*reference) {
            ++(*reference)->inFlightRefs;
            (*reference)->lastUseSubmission = serial;
        }
    }
    for (const auto *reference : {&texture, &depth}) {
        if (*reference) {
            ++(*reference)->inFlightRefs;
            (*reference)->lastUseSubmission = serial;
        }
    }
    if (bindingSet) {
        ++bindingSet->inFlightRefs;
        for (const auto &buffer : bindingSet->buffers) {
            buffer->lastUseSubmission = serial;
        }
        for (const auto &sample : bindingSet->textures) {
            sample->lastUseSubmission = serial;
        }
    }
}

void Operation::ReleaseInFlightRefs() const noexcept
{
    for (const auto *reference : {&src, &dst}) {
        if (*reference) {
            --(*reference)->inFlightRefs;
        }
    }
    for (const auto *reference : {&texture, &depth}) {
        if (*reference) {
            --(*reference)->inFlightRefs;
        }
    }
    if (bindingSet) {
        --bindingSet->inFlightRefs;
    }
}

void BufferCommandList::ReleaseReferences() noexcept
{
    for (const auto &operation : operations) {
        operation.ReleaseRecordingRefs();
    }
    operations.clear();
    color = nullptr;
    depth = nullptr;
    ClearBindings();
}

std::optional<BufferUse>
BufferCommandList::RecordedUse(const Buffer *buffer) const noexcept
{
    for (auto entry = operations.rbegin(); entry != operations.rend();
         ++entry) {
        if (entry->kind == Operation::Kind::Barrier
            && entry->src.get() == buffer) {
            return entry->after;
        }
    }
    return std::nullopt;
}

Result<CommandState> BufferCommandList::GetState() const noexcept
{
    return Invoke<CommandState>("GetState", [&]() {
        if (const auto error = device->CheckAccess(DeviceAccess::Cleanup)) {
            return Fail<CommandState>(*error);
        }
        if (state == CommandState::Pending) {
            auto complete = device->PollSubmission(pendingSerial);
            if (!complete) {
                return Result<CommandState>::Failure(
                    std::move(complete).GetError());
            }
            return Result<CommandState>::Success(complete.Value()
                                                     ? CommandState::Completed
                                                     : CommandState::Pending);
        }
        return Result<CommandState>::Success(state);
    });
}

Result<void> BufferCommandList::Begin() noexcept
{
    return Invoke<void>("Begin", [&]() {
        if (const auto error = device->CheckAccess()) {
            return Fail<void>(*error);
        }
        if (state != CommandState::Initial) {
            return Fail<void>(ErrorCode::InvalidState);
        }
        state = CommandState::Recording;
        return Result<void>::Success();
    });
}

Result<void>
BufferCommandList::Barrier(std::span<const BufferBarrier> barriers) noexcept
{
    return Invoke<void>("Barrier", [&]() {
        if (const auto error = device->CheckAccess()) {
            return Fail<void>(*error);
        }
        if (state != CommandState::Recording || color != nullptr) {
            return Fail<void>(ErrorCode::InvalidState);
        }
        if (barriers.size()
            > device->capabilities.maxCommandsPerList - operations.size()) {
            return Fail<void>(ErrorCode::Unsupported);
        }
        std::vector<Operation> additions;
        additions.reserve(barriers.size());
        for (const auto &barrier : barriers) {
            auto found = device->Lookup(barrier.buffer);
            if (!found) {
                return Fail<void>(found.GetError().errorCode);
            }
            const auto &buffer = found.Value();
            if (!ValidUse(barrier.beforeUse) || !ValidUse(barrier.afterUse)
                || barrier.afterUse == BufferUse::Initial
                || !Allows(buffer->usage, barrier.beforeUse)
                || !Allows(buffer->usage, barrier.afterUse)) {
                return Fail<void>(ErrorCode::InvalidArgument);
            }
            auto previous = RecordedUse(buffer.get());
            for (const auto &added : additions) {
                if (added.src == buffer) {
                    previous = added.after;
                }
            }
            if (previous && *previous != barrier.beforeUse) {
                return Fail<void>(ErrorCode::InvalidState);
            }
            additions.push_back({Operation::Kind::Barrier,
                                 buffer,
                                 {},
                                 barrier.beforeUse,
                                 barrier.afterUse,
                                 0,
                                 0,
                                 0});
        }
        operations.reserve(operations.size() + additions.size());
        for (auto &operation : additions) {
            operations.push_back(std::move(operation));
            ++operations.back().src->recordingRefs;
        }
        return Result<void>::Success();
    });
}

Result<void> BufferCommandList::CopyBuffer(const BufferCopyDesc &copy) noexcept
{
    return Invoke<void>("CopyBuffer", [&]() {
        if (const auto error = device->CheckAccess()) {
            return Fail<void>(*error);
        }
        if (state != CommandState::Recording || color != nullptr) {
            return Fail<void>(ErrorCode::InvalidState);
        }
        auto source = device->Lookup(copy.src);
        auto destination = device->Lookup(copy.dst);
        if (!source) {
            return Fail<void>(source.GetError().errorCode);
        }
        if (!destination) {
            return Fail<void>(destination.GetError().errorCode);
        }
        if (copy.src == copy.dst) {
            return Fail<void>(ErrorCode::Unsupported);
        }
        const auto &src = source.Value();
        const auto &dst = destination.Value();
        if (copy.sizeBytes == 0 || !HasUsage(src->usage, BufferUsage::CopySrc)
            || !HasUsage(dst->usage, BufferUsage::CopyDst)
            || !InRange(copy.srcOffsetBytes, copy.sizeBytes, src->sizeBytes)
            || !InRange(copy.dstOffsetBytes, copy.sizeBytes, dst->sizeBytes)) {
            return Fail<void>(ErrorCode::InvalidArgument);
        }
        const auto srcUse = RecordedUse(src.get());
        const auto dstUse = RecordedUse(dst.get());
        if ((srcUse && *srcUse != BufferUse::CopySrc)
            || (dstUse && *dstUse != BufferUse::CopyDst)) {
            return Fail<void>(ErrorCode::InvalidState);
        }
        if (operations.size() == device->capabilities.maxCommandsPerList) {
            return Fail<void>(ErrorCode::Unsupported);
        }
        operations.push_back({Operation::Kind::Copy, src, dst,
                              BufferUse::Initial, BufferUse::Initial,
                              copy.srcOffsetBytes, copy.dstOffsetBytes,
                              copy.sizeBytes});
        ++src->recordingRefs;
        ++dst->recordingRefs;
        return Result<void>::Success();
    });
}

Result<void> BufferCommandList::End() noexcept
{
    return Invoke<void>("End", [&]() {
        if (const auto error = device->CheckAccess()) {
            return Fail<void>(*error);
        }
        if (state != CommandState::Recording || color != nullptr) {
            return Fail<void>(ErrorCode::InvalidState);
        }
        state = CommandState::Executable;
        return Result<void>::Success();
    });
}

Result<void> BufferCommandList::Reset() noexcept
{
    return Invoke<void>("Reset", [&]() {
        if (const auto error = device->CheckAccess(DeviceAccess::Cleanup)) {
            return Fail<void>(*error);
        }
        if (state == CommandState::Pending) {
            auto complete = device->PollSubmission(pendingSerial);
            if (!complete) {
                return Result<void>::Failure(std::move(complete).GetError());
            }
            if (!complete.Value()) {
                return Fail<void>(ErrorCode::ResourceBusy);
            }
        }
        ReleaseReferences();
        pendingSerial = 0;
        state = CommandState::Initial;
        return Result<void>::Success();
    });
}
} // namespace voryx::rhi_detail

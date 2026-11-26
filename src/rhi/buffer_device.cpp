#include "buffer_device.hpp"

#include "buffer_command_list.hpp"

#include "handle_access.hpp"

#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace voryx::rhi_detail
{
BufferDevice::BufferDevice(std::shared_ptr<State> state)
    : state(std::move(state))
{
}

BufferDevice::~BufferDevice()
{
    if (std::this_thread::get_id() != state->owner) {
        std::terminate();
    }
    state->alive = false;
    state->DrainSubmissions();
    FinishShutdown();
}

void BufferDevice::FinishShutdown() noexcept
{
    state->alive = false;
    ConsumeFrame();
    state->bindingSets.clear();
    state->slots.clear();
    state->pipelines.clear();
    state->textures.clear();
    state->bindingLayouts.clear();
    state->samplers.clear();
}

const DeviceInfo &BufferDevice::GetInfo() const noexcept
{
    return state->info;
}

const DeviceCapabilities &BufferDevice::GetCapabilities() const noexcept
{
    return state->capabilities;
}

Result<BufferHandle> BufferDevice::CreateBuffer(const BufferDesc &desc,
                                                ByteSpan initialData) noexcept
{
    return Invoke<BufferHandle>("CreateBuffer", [&]() {
        if (const auto error = state->CheckAccess()) {
            return Fail<BufferHandle>(*error);
        }
        const auto bits = static_cast<std::uint32_t>(desc.usage);
        if (desc.sizeBytes == 0 || bits == 0 || (bits & ~31u) != 0
            || (!initialData.empty()
                && (initialData.size() != desc.sizeBytes
                    || !HasUsage(desc.usage, BufferUsage::CopyDst)))) {
            return Fail<BufferHandle>(ErrorCode::InvalidArgument);
        }
        if (desc.sizeBytes > state->capabilities.maxBufferSizeBytes) {
            return Fail<BufferHandle>(ErrorCode::Unsupported);
        }
        std::size_t index = 0;
        for (; index < state->slots.size(); ++index) {
            const auto &slot = state->slots[index];
            if (!slot.buffer
                && slot.generation
                       != std::numeric_limits<std::uint64_t>::max()) {
                break;
            }
        }
        if (index == state->slots.size()
            && index == state->capabilities.maxBuffers) {
            return Fail<BufferHandle>(ErrorCode::OutOfMemory);
        }
        auto buffer = std::make_shared<Buffer>();
        buffer->usage = desc.usage;
        buffer->debugName.assign(desc.debugName);
        buffer->sizeBytes = desc.sizeBytes;
        auto storage = state->backend->CreateStorage(desc);
        if (!storage) {
            if (storage.GetError().errorCode == ErrorCode::DeviceLost) {
                state->lost = true;
            }
            return Result<BufferHandle>::Failure(std::move(storage).GetError());
        }
        buffer->storage = std::move(storage).Value();
        if (!initialData.empty()) {
            auto written = buffer->storage->Write(0, initialData);
            if (!written) {
                if (written.GetError().errorCode == ErrorCode::DeviceLost) {
                    state->lost = true;
                }
                return Result<BufferHandle>::Failure(
                    std::move(written).GetError());
            }
        }
        if (index == state->slots.size()) {
            state->slots.emplace_back();
        }
        auto &slot = state->slots[index];
        slot.buffer = std::move(buffer);
        return Result<BufferHandle>::Success(detail::RhiHandleAccess::Buffer(
            state->identity, static_cast<std::uint32_t>(index),
            slot.generation));
    });
}

Result<void> BufferDevice::WriteBuffer(BufferHandle handle,
                                       std::uint64_t offsetBytes,
                                       ByteSpan data) noexcept
{
    return Invoke<void>("WriteBuffer", [&]() {
        if (const auto error = state->CheckAccess()) {
            return Fail<void>(*error);
        }
        auto found = state->Lookup(handle);
        if (!found) {
            return Fail<void>(found.GetError().errorCode);
        }
        auto &buffer = *found.Value();
        if (!HasUsage(buffer.usage, BufferUsage::CopyDst)
            || !InRange(offsetBytes, data.size(), buffer.sizeBytes)) {
            return Fail<void>(ErrorCode::InvalidArgument);
        }
        if (buffer.recordingRefs != 0
            || buffer.lastUseSubmission > state->completedSubmission) {
            return Fail<void>(ErrorCode::ResourceBusy);
        }
        auto result = buffer.storage->Write(offsetBytes, data);
        if (!result && result.GetError().errorCode == ErrorCode::DeviceLost) {
            state->lost = true;
        }
        return result;
    });
}

Result<void> BufferDevice::ReadBuffer(BufferHandle handle,
                                      std::uint64_t offsetBytes,
                                      MutableByteSpan output) noexcept
{
    return Invoke<void>("ReadBuffer", [&]() {
        if (const auto error = state->CheckAccess()) {
            return Fail<void>(*error);
        }
        auto found = state->Lookup(handle);
        if (!found) {
            return Fail<void>(found.GetError().errorCode);
        }
        const auto &buffer = *found.Value();
        if (!HasUsage(buffer.usage, BufferUsage::CopySrc)
            || !InRange(offsetBytes, output.size(), buffer.sizeBytes)) {
            return Fail<void>(ErrorCode::InvalidArgument);
        }
        if (buffer.recordingRefs != 0
            || buffer.lastUseSubmission > state->completedSubmission) {
            return Fail<void>(ErrorCode::ResourceBusy);
        }
        auto result = buffer.storage->Read(offsetBytes, output);
        if (!result && result.GetError().errorCode == ErrorCode::DeviceLost) {
            state->lost = true;
        }
        return result;
    });
}

Result<void> BufferDevice::Destroy(BufferHandle handle) noexcept
{
    return Invoke<void>("Destroy", [&]() {
        if (const auto error = state->CheckAccess(DeviceAccess::Cleanup)) {
            return Fail<void>(*error);
        }
        auto found = state->Lookup(handle);
        if (!found) {
            return Fail<void>(found.GetError().errorCode);
        }
        if (found.Value()->recordingRefs != 0
            || static_cast<std::size_t>(found.Value().use_count())
                   > 2 + found.Value()->inFlightRefs) {
            return Fail<void>(ErrorCode::ResourceBusy);
        }
        auto &slot = state->slots[detail::RhiHandleAccess::Slot(handle)];
        slot.buffer.reset();
        if (slot.generation != std::numeric_limits<std::uint64_t>::max()) {
            ++slot.generation;
        }
        return Result<void>::Success();
    });
}

Result<std::unique_ptr<ICommandList>> BufferDevice::CreateCommandList() noexcept
{
    return Invoke<std::unique_ptr<ICommandList>>("CreateCommandList", [&]() {
        if (const auto error = state->CheckAccess()) {
            return Fail<std::unique_ptr<ICommandList>>(*error);
        }
        std::unique_ptr<ICommandList> command =
            std::make_unique<BufferCommandList>(state);
        return Result<std::unique_ptr<ICommandList>>::Success(
            std::move(command));
    });
}

Result<SubmissionToken> BufferDevice::Submit(const SubmitDesc &desc) noexcept
{
    return Invoke<SubmissionToken>("Submit", [&]() {
        if (const auto error = state->CheckAccess()) {
            return Fail<SubmissionToken>(*error);
        }
        if (desc.commandLists.empty()) {
            return Fail<SubmissionToken>(ErrorCode::InvalidArgument);
        }
        if (desc.commandLists.size()
            > state->capabilities.maxCommandListsPerSubmit) {
            return Fail<SubmissionToken>(ErrorCode::Unsupported);
        }
        if (state->lastSubmission
            == std::numeric_limits<std::uint64_t>::max()) {
            return Fail<SubmissionToken>(ErrorCode::InternalError);
        }
        const bool submitsFrame = desc.frame.IsValid();
        if (desc.mode > SubmitMode::Async) {
            return Fail<SubmissionToken>(ErrorCode::InvalidArgument);
        }
        const bool asynchronous = desc.mode == SubmitMode::Async;
        if (asynchronous
            && (state->capabilities.maxPendingSubmissions == 0
                || (submitsFrame
                    && state->capabilities.maxPendingWindowFrames == 0))) {
            return Fail<SubmissionToken>(ErrorCode::Unsupported);
        }
        if (submitsFrame) {
            if (const auto error = state->CheckFrame(desc.frame)) {
                return Fail<SubmissionToken>(*error);
            }
            if (state->frameSubmission.IsValid()) {
                return Fail<SubmissionToken>(ErrorCode::InvalidState);
            }
        }
        bool rendersFrame = false;
        bool frameFinalized = false;
        std::vector<BufferCommandList *> lists;
        lists.reserve(desc.commandLists.size());
        std::unordered_map<Buffer *, BufferUse> predicted;
        std::unordered_map<Texture *, TextureUse> textureUses;
        std::vector<Texture *> initialized;
        const auto textureUse =
            [&](const std::shared_ptr<Texture> &texture) -> TextureUse & {
            return textureUses.try_emplace(texture.get(), texture->use)
                .first->second;
        };
        const auto useOf =
            [&](const std::shared_ptr<Buffer> &buffer) -> BufferUse & {
            return predicted.try_emplace(buffer.get(), buffer->use)
                .first->second;
        };
        // 先验证整批提交，任何失败都不能修改资源内容或已提交状态。
        for (auto *candidate : desc.commandLists) {
            if (candidate == nullptr) {
                return Fail<SubmissionToken>(ErrorCode::InvalidArgument);
            }
            auto *list = dynamic_cast<BufferCommandList *>(candidate);
            if (list == nullptr || list->device.get() != state.get()) {
                return Fail<SubmissionToken>(ErrorCode::WrongDevice);
            }
            if (list->state != CommandState::Executable) {
                return Fail<SubmissionToken>(ErrorCode::InvalidState);
            }
            for (const auto *previous : lists) {
                if (previous == list) {
                    return Fail<SubmissionToken>(ErrorCode::InvalidArgument);
                }
            }
            lists.push_back(list);
            for (const auto &operation : list->operations) {
                if (operation.texture && operation.texture->windowImage
                    && (!submitsFrame || frameFinalized
                        || operation.texture != state->frameTexture)) {
                    return Fail<SubmissionToken>(ErrorCode::InvalidState);
                }
                if (operation.kind == Operation::Kind::Barrier) {
                    auto &use = useOf(operation.src);
                    if (use != operation.before) {
                        return Fail<SubmissionToken>(ErrorCode::InvalidState);
                    }
                    use = operation.after;
                } else if (operation.kind == OperationKind::Copy) {
                    if (useOf(operation.src) != BufferUse::CopySrc
                        || useOf(operation.dst) != BufferUse::CopyDst) {
                        return Fail<SubmissionToken>(ErrorCode::InvalidState);
                    }
                } else if (operation.kind == OperationKind::TextureBarrier) {
                    auto &use = textureUse(operation.texture);
                    if (use != operation.native.textureBefore) {
                        return Fail<SubmissionToken>(ErrorCode::InvalidState);
                    }
                    use = operation.native.textureAfter;
                    if (operation.texture->windowImage
                        && use == TextureUse::Present) {
                        frameFinalized = true;
                    }
                } else if (operation.kind == OperationKind::BindingSet) {
                    for (const auto &buffer : operation.bindingSet->buffers) {
                        if (useOf(buffer) != BufferUse::ConstantRead) {
                            return Fail<SubmissionToken>(
                                ErrorCode::InvalidState);
                        }
                    }
                    for (const auto &texture : operation.bindingSet->textures) {
                        if (textureUse(texture) != TextureUse::ShaderRead) {
                            return Fail<SubmissionToken>(
                                ErrorCode::InvalidState);
                        }
                    }
                } else if (operation.kind == OperationKind::BeginPass) {
                    if (textureUse(operation.texture)
                            != TextureUse::ColorAttachment
                        || (operation.depth
                            && textureUse(operation.depth)
                                   != TextureUse::DepthWrite)) {
                        return Fail<SubmissionToken>(ErrorCode::InvalidState);
                    }
                    rendersFrame =
                        rendersFrame || operation.texture->windowImage;
                    initialized.push_back(operation.texture.get());
                    if (operation.depth) {
                        initialized.push_back(operation.depth.get());
                    }
                } else if (operation.kind == OperationKind::Vertex
                           || operation.kind == OperationKind::Index) {
                    const auto expected =
                        operation.kind == OperationKind::Vertex
                            ? BufferUse::VertexRead
                            : BufferUse::IndexRead;
                    if (useOf(operation.src) != expected) {
                        return Fail<SubmissionToken>(ErrorCode::InvalidState);
                    }
                }
            }
        }
        if (submitsFrame
            && (!rendersFrame
                || textureUse(state->frameTexture) != TextureUse::Present)) {
            return Fail<SubmissionToken>(ErrorCode::InvalidState);
        }
        std::vector<BackendOperation> native;
        for (const auto *list : lists) {
            for (const auto &operation : list->operations) {
                auto command = operation.native;
                command.kind = operation.kind;
                command.src =
                    operation.src ? operation.src->storage.get() : nullptr;
                command.dst =
                    operation.dst ? operation.dst->storage.get() : nullptr;
                command.texture = operation.texture
                                      ? operation.texture->storage.get()
                                      : nullptr;
                command.depth =
                    operation.depth ? operation.depth->storage.get() : nullptr;
                command.pipeline = operation.pipeline
                                       ? operation.pipeline->storage.get()
                                       : nullptr;
                command.bindingSet = operation.bindingSet
                                         ? operation.bindingSet->storage.get()
                                         : nullptr;
                command.before = operation.before;
                command.after = operation.after;
                command.srcOffset = operation.srcOffset;
                command.dstOffset = operation.dstOffset;
                command.size = operation.size;
                native.push_back(command);
            }
        }
        const auto serial = state->lastSubmission + 1;
        if (asynchronous) {
            if (state->pending.size()
                == state->capabilities.maxPendingSubmissions) {
                auto ready =
                    state->PollSubmission(state->pending.front().serial);
                if (!ready) {
                    return Result<SubmissionToken>::Failure(
                        std::move(ready).GetError());
                }
                if (!ready.Value()) {
                    return Fail<SubmissionToken>(ErrorCode::ResourceBusy);
                }
            }
            // GPU 接受前完成所有可能抛出异常的分配。
            PendingSubmission pending{serial, {}, {}};
            pending.operations.reserve(native.size());
            state->pending.push_back(std::move(pending));
            auto enqueued = submitsFrame
                                ? state->backend->EnqueueFrame(
                                      native, *state->frameTexture->storage)
                                : state->backend->Enqueue(native);
            if (!enqueued) {
                state->pending.pop_back();
                if (enqueued.GetError().errorCode == ErrorCode::DeviceLost) {
                    state->lost = true;
                }
                return Result<SubmissionToken>::Failure(
                    std::move(enqueued).GetError());
            }
            state->pending.back().native = std::move(enqueued).Value();
        } else {
            auto executed = state->backend->Execute(native);
            if (!executed) {
                if (executed.GetError().errorCode == ErrorCode::DeviceLost) {
                    state->lost = true;
                }
                return Result<SubmissionToken>::Failure(
                    std::move(executed).GetError());
            }
            state->CompleteThrough(serial);
        }
        for (const auto &[buffer, use] : predicted) {
            buffer->use = use;
        }
        for (const auto &[texture, use] : textureUses) {
            texture->use = use;
        }
        for (auto *texture : initialized) {
            texture->initialized = true;
        }
        state->lastSubmission = serial;
        for (auto *list : lists) {
            if (asynchronous) {
                auto &retained = state->pending.back().operations;
                for (auto &operation : list->operations) {
                    operation.TransferToSubmission(serial);
                    retained.push_back(std::move(operation));
                }
                list->pendingSerial = serial;
                list->state = CommandState::Pending;
            } else {
                list->state = CommandState::Completed;
            }
            list->ReleaseReferences();
        }
        const auto submitted =
            detail::RhiHandleAccess::Token(state->identity, serial);
        if (submitsFrame) {
            state->frameSubmission = submitted;
            state->frameSubmitMode = desc.mode;
            if (asynchronous) {
                state->lastAsyncFrameSubmission = serial;
            }
        }
        return Result<SubmissionToken>::Success(submitted);
    });
}

Result<bool> BufferDevice::IsComplete(SubmissionToken token) const noexcept
{
    return Invoke<bool>("IsComplete", [&]() {
        if (const auto error = state->CheckAccess(DeviceAccess::Cleanup)) {
            return Fail<bool>(*error);
        }
        if (const auto error = state->CheckToken(token)) {
            return Fail<bool>(*error);
        }
        return state->PollSubmission(detail::RhiHandleAccess::Serial(token));
    });
}

Result<void> BufferDevice::Wait(SubmissionToken token,
                                TimeoutNs timeoutNs) noexcept
{
    return Invoke<void>("Wait", [&]() {
        if (const auto error = state->CheckAccess(DeviceAccess::Cleanup)) {
            return Fail<void>(*error);
        }
        if (const auto error = state->CheckToken(token)) {
            return Fail<void>(*error);
        }
        return state->WaitForSubmission(detail::RhiHandleAccess::Serial(token),
                                        timeoutNs);
    });
}
} // namespace voryx::rhi_detail

namespace voryx::rhi_detail
{
Result<std::unique_ptr<IDevice>>
CreateBufferDevice(std::uint64_t identity, DeviceInfo info,
                   DeviceCapabilities capabilities,
                   std::unique_ptr<BufferBackend> backend) noexcept
{
    return Invoke<std::unique_ptr<IDevice>>("CreateBufferDevice", [&]() {
        auto state = std::make_shared<State>(identity, std::move(info),
                                             capabilities, std::move(backend));
        std::unique_ptr<IDevice> result =
            std::make_unique<BufferDevice>(std::move(state));
        return Result<std::unique_ptr<IDevice>>::Success(std::move(result));
    });
}
} // namespace voryx::rhi_detail

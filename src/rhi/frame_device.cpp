#include "buffer_device.hpp"
#include "handle_access.hpp"

#include <limits>

namespace voryx::rhi_detail
{
Result<void> BufferDevice::CheckAcquireFrame() const noexcept
{
    if (const auto error = state->CheckAccess()) {
        return Fail<void>(*error);
    }
    if (state->frameTexture) {
        return Fail<void>(ErrorCode::InvalidState);
    }
    if (state->frameSerial == std::numeric_limits<std::uint64_t>::max()) {
        return Fail<void>(ErrorCode::InternalError);
    }
    return Result<void>::Success();
}
Result<FrameInfo>
BufferDevice::PublishFrame(Extent2D extent, Format format,
                           std::unique_ptr<TextureStorage> image) noexcept
{
    return Invoke<FrameInfo>("AcquireFrame", [&]() {
        auto texture = std::make_shared<Texture>();
        texture->extent = extent;
        texture->format = format;
        texture->usage = TextureUsage::ColorAttachment;
        texture->use = TextureUse::Present;
        texture->windowImage = true;
        texture->storage = std::move(image);
        state->frameTexture = std::move(texture);
        const auto serial = ++state->frameSerial;
        using Access = detail::RhiHandleAccess;
        return Result<FrameInfo>::Success(
            {Access::Frame(state->identity, serial),
             Access::Texture(state->identity, windowTextureSlot, serial),
             extent, format});
    });
}

Result<void> BufferDevice::CheckFinishFrame(FrameToken token) const noexcept
{
    if (const auto error = state->CheckAccess()) {
        return Fail<void>(*error);
    }
    if (const auto error = state->CheckFrame(token)) {
        return Fail<void>(*error);
    }
    if (state->frameTexture->recordingRefs != 0) {
        return Fail<void>(ErrorCode::ResourceBusy);
    }
    return Result<void>::Success();
}
Result<void>
BufferDevice::CheckPresent(FrameToken token,
                           SubmissionToken submission) const noexcept
{
    auto checked = CheckFinishFrame(token);
    if (!checked) {
        return checked;
    }
    if (const auto error = state->CheckToken(submission)) {
        return Fail<void>(*error);
    }
    if (submission != state->frameSubmission) {
        return Fail<void>(ErrorCode::InvalidState);
    }
    return Result<void>::Success();
}
Result<void> BufferDevice::CheckCancel(FrameToken token) const noexcept
{
    auto checked = CheckFinishFrame(token);
    if (!checked) {
        return checked;
    }
    if (state->frameSubmission.IsValid()) {
        return Fail<void>(ErrorCode::InvalidState);
    }
    return Result<void>::Success();
}
bool BufferDevice::IsFrameAsynchronous() const noexcept
{
    return state->frameSubmitMode == SubmitMode::Async;
}
void BufferDevice::ConsumeFrame() noexcept
{
    state->frameTexture.reset();
    state->frameSubmission = {};
    state->frameSubmitMode = SubmitMode::Synchronous;
}
void BufferDevice::DrainFrameSubmissions() noexcept
{
    const auto serial = state->lastAsyncFrameSubmission;
    if (serial <= state->completedSubmission) {
        return;
    }
    const auto complete =
        state->WaitForSubmission(serial, std::numeric_limits<TimeoutNs>::max());
    if (!complete) {
        state->lost = true;
        state->DrainSubmissions();
    }
    state->lastAsyncFrameSubmission = 0;
}
Result<void> BufferDevice::PrepareShutdown(TimeoutNs timeoutNs) noexcept
{
    state->draining = true;
    if (state->openCommandLists != 0) {
        return Fail<void>(ErrorCode::ResourceBusy);
    }
    if (state->pending.empty()) {
        return Result<void>::Success();
    }
    auto waited = state->WaitForSubmission(state->lastSubmission, timeoutNs);
    if (!waited && waited.GetError().errorCode == ErrorCode::DeviceLost) {
        // 后端等待确认设备丢失，挂起的原生命令可以回收。
        state->CompleteThrough(state->lastSubmission);
    }
    return waited;
}
void BufferDevice::MarkLost() noexcept
{
    state->lost = true;
}
} // namespace voryx::rhi_detail

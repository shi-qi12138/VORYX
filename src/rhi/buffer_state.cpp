#include "buffer_state.hpp"

#include "handle_access.hpp"

namespace voryx::rhi_detail
{
State::State(std::uint64_t identity, DeviceInfo info,
             DeviceCapabilities capabilities,
             std::unique_ptr<BufferBackend> backend)
    : identity(identity)
    , info(std::move(info))
    , capabilities(capabilities)
    , backend(std::move(backend))
{
}

std::optional<ErrorCode> State::CheckAccess(DeviceAccess access) const noexcept
{
    if (std::this_thread::get_id() != owner) {
        return ErrorCode::WrongThread;
    }
    if (!alive || (draining && access == DeviceAccess::Work)) {
        return ErrorCode::InvalidState;
    }
    if (lost) {
        return ErrorCode::DeviceLost;
    }
    return std::nullopt;
}

Result<std::shared_ptr<Buffer>> State::Lookup(BufferHandle handle) const
{
    using Access = detail::RhiHandleAccess;
    if (!handle.IsValid()) {
        return Fail<std::shared_ptr<Buffer>>(ErrorCode::InvalidHandle);
    }
    if (Access::Owner(handle) != identity) {
        return Fail<std::shared_ptr<Buffer>>(ErrorCode::WrongDevice);
    }
    const auto index = Access::Slot(handle);
    if (index >= slots.size() || !slots[index].buffer
        || slots[index].generation != Access::Generation(handle)) {
        return Fail<std::shared_ptr<Buffer>>(ErrorCode::InvalidHandle);
    }
    return Result<std::shared_ptr<Buffer>>::Success(slots[index].buffer);
}

Result<std::shared_ptr<Pipeline>> State::Lookup(PipelineHandle handle) const
{
    using Access = detail::RhiHandleAccess;
    if (!handle.IsValid()) {
        return Fail<std::shared_ptr<Pipeline>>(ErrorCode::InvalidHandle);
    }
    if (Access::Owner(handle) != identity) {
        return Fail<std::shared_ptr<Pipeline>>(ErrorCode::WrongDevice);
    }
    const auto index = Access::Slot(handle);
    if (index >= pipelines.size() || !pipelines[index].pipeline
        || pipelines[index].generation != Access::Generation(handle)) {
        return Fail<std::shared_ptr<Pipeline>>(ErrorCode::InvalidHandle);
    }
    return Result<std::shared_ptr<Pipeline>>::Success(
        pipelines[index].pipeline);
}

Result<std::shared_ptr<Texture>> State::Lookup(TextureHandle handle) const
{
    using Access = detail::RhiHandleAccess;
    if (!handle.IsValid()) {
        return Fail<std::shared_ptr<Texture>>(ErrorCode::InvalidHandle);
    }
    if (Access::Owner(handle) != identity) {
        return Fail<std::shared_ptr<Texture>>(ErrorCode::WrongDevice);
    }
    const auto index = Access::Slot(handle);
    if (index == windowTextureSlot) {
        if (!frameTexture || frameSerial != Access::Generation(handle)) {
            return Fail<std::shared_ptr<Texture>>(ErrorCode::InvalidHandle);
        }
        return Result<std::shared_ptr<Texture>>::Success(frameTexture);
    }
    if (index >= textures.size() || !textures[index].texture
        || textures[index].generation != Access::Generation(handle)) {
        return Fail<std::shared_ptr<Texture>>(ErrorCode::InvalidHandle);
    }
    return Result<std::shared_ptr<Texture>>::Success(textures[index].texture);
}

std::optional<ErrorCode> State::CheckFrame(FrameToken token) const noexcept
{
    using Access = detail::RhiHandleAccess;
    if (!token.IsValid()) {
        return ErrorCode::InvalidHandle;
    }
    if (Access::Owner(token) != identity) {
        return ErrorCode::WrongDevice;
    }
    if (!frameTexture || Access::Serial(token) != frameSerial) {
        return ErrorCode::InvalidHandle;
    }
    return std::nullopt;
}

std::optional<ErrorCode> State::CheckToken(SubmissionToken token) const noexcept
{
    using Access = detail::RhiHandleAccess;
    if (!token.IsValid()) {
        return ErrorCode::InvalidHandle;
    }
    if (Access::Owner(token) != identity) {
        return ErrorCode::WrongDevice;
    }
    if (Access::Serial(token) > lastSubmission) {
        return ErrorCode::InvalidHandle;
    }
    return std::nullopt;
}

bool HasUsage(BufferUsage usage, BufferUsage required) noexcept
{
    return (static_cast<std::uint32_t>(usage)
            & static_cast<std::uint32_t>(required))
           == static_cast<std::uint32_t>(required);
}

bool ValidUse(BufferUse use) noexcept
{
    return use <= BufferUse::CopyDst;
}

bool Allows(BufferUsage usage, BufferUse use) noexcept
{
    switch (use) {
        case BufferUse::Initial:
            return true;
        case BufferUse::VertexRead:
            return HasUsage(usage, BufferUsage::Vertex);
        case BufferUse::IndexRead:
            return HasUsage(usage, BufferUsage::Index);
        case BufferUse::ConstantRead:
            return HasUsage(usage, BufferUsage::Constant);
        case BufferUse::CopySrc:
            return HasUsage(usage, BufferUsage::CopySrc);
        case BufferUse::CopyDst:
            return HasUsage(usage, BufferUsage::CopyDst);
    }
    return false;
}

bool InRange(std::uint64_t offset, std::uint64_t size,
             std::uint64_t total) noexcept
{
    return offset <= total && size <= total - offset;
}
} // namespace voryx::rhi_detail

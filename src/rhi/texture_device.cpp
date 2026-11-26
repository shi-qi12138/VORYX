#include "binding_validation.hpp"
#include "buffer_device.hpp"
#include "handle_access.hpp"
#include <limits>

namespace voryx::rhi_detail
{
bool HasUsage(TextureUsage usage, TextureUsage required) noexcept
{
    return (static_cast<std::uint32_t>(usage)
            & static_cast<std::uint32_t>(required))
           == static_cast<std::uint32_t>(required);
}
bool Allows(const Texture &texture, TextureUse use) noexcept
{
    switch (use) {
        case TextureUse::Initial:
            return true;
        case TextureUse::ColorAttachment:
            return HasUsage(texture.usage, TextureUsage::ColorAttachment);
        case TextureUse::DepthWrite:
            return HasUsage(texture.usage, TextureUsage::DepthAttachment);
        case TextureUse::CopySrc:
            return HasUsage(texture.usage, TextureUsage::CopySrc);
        case TextureUse::Present:
            return texture.windowImage;
        default:
            return false;
    }
}
Result<TextureHandle>
BufferDevice::CreateTexture(const TextureDesc &desc) noexcept
{
    return Invoke<TextureHandle>("CreateTexture", [&]() {
        return CreateTextureImpl(desc, nullptr);
    });
}
Result<TextureHandle>
BufferDevice::CreateTexture2D(const Rgba8TextureDesc &desc) noexcept
{
    return Invoke<TextureHandle>("CreateTexture2D", [&]() {
        return CreateTextureImpl(
            {desc.extent, Format::RGBA8_UNorm, TextureUsage::Sampled, {}},
            &desc);
    });
}
Result<TextureHandle>
BufferDevice::CreateTextureImpl(const TextureDesc &desc,
                                const Rgba8TextureDesc *upload)
{
    if (const auto error = state->CheckAccess()) {
        return Fail<TextureHandle>(*error);
    }
    if (upload && !ValidSampledTexture(*upload)) {
        return Fail<TextureHandle>(ErrorCode::InvalidArgument);
    }
    const auto bits = static_cast<std::uint32_t>(desc.usage);
    if (desc.extent.width == 0 || desc.extent.height == 0 || bits == 0
        || (bits & ~31u) != 0 || desc.format == Format::Auto
        || desc.format > Format::D32_Float
        || (HasUsage(desc.usage, TextureUsage::ColorAttachment)
            && HasUsage(desc.usage, TextureUsage::DepthAttachment))) {
        return Fail<TextureHandle>(ErrorCode::InvalidArgument);
    }
    if (desc.extent.width > state->capabilities.maxTexture2DSize
        || desc.extent.height > state->capabilities.maxTexture2DSize) {
        return Fail<TextureHandle>(ErrorCode::Unsupported);
    }
    const bool color =
        desc.format == Format::RGBA8_UNorm
        && (desc.usage == TextureUsage::ColorAttachment
            || desc.usage
                   == (TextureUsage::ColorAttachment | TextureUsage::CopySrc));
    const bool depth = desc.format == Format::D32_Float
                       && desc.usage == TextureUsage::DepthAttachment;
    if (!color && !depth && !upload) {
        return Fail<TextureHandle>(ErrorCode::Unsupported);
    }
    std::size_t index = 0;
    for (; index < state->textures.size(); ++index) {
        if (!state->textures[index].texture
            && state->textures[index].generation
                   != std::numeric_limits<std::uint64_t>::max()) {
            break;
        }
    }
    if (index == state->textures.size()
        && index == state->capabilities.maxTextures) {
        return Fail<TextureHandle>(ErrorCode::OutOfMemory);
    }
    auto texture = std::make_shared<Texture>();
    texture->extent = desc.extent;
    texture->format = desc.format;
    texture->usage = desc.usage;
    texture->debugName.assign(desc.debugName);
    if (upload) {
        texture->initialized = true;
        texture->use = TextureUse::ShaderRead;
    }
    auto created = upload ? state->backend->CreateSampledTexture(*upload)
                          : state->backend->CreateTexture(desc);
    if (!created) {
        if (created.GetError().errorCode == ErrorCode::DeviceLost) {
            state->lost = true;
        }
        return Result<TextureHandle>::Failure(std::move(created).GetError());
    }
    texture->storage = std::move(created).Value();
    if (index == state->textures.size()) {
        state->textures.emplace_back();
    }
    auto &slot = state->textures[index];
    slot.texture = std::move(texture);
    return Result<TextureHandle>::Success(detail::RhiHandleAccess::Texture(
        state->identity, static_cast<std::uint32_t>(index), slot.generation));
}
Result<void> BufferDevice::ReadTexture(TextureHandle handle,
                                       MutableByteSpan output) noexcept
{
    return Invoke<void>("ReadTexture", [&]() {
        if (const auto error = state->CheckAccess()) {
            return Fail<void>(*error);
        }
        auto found = state->Lookup(handle);
        if (!found) {
            return Fail<void>(found.GetError().errorCode);
        }
        auto &texture = *found.Value();
        if (texture.format != Format::RGBA8_UNorm
            || !HasUsage(texture.usage, TextureUsage::CopySrc)
            || output.size()
                   != std::uint64_t(texture.extent.width)
                          * texture.extent.height * 4) {
            return Fail<void>(ErrorCode::InvalidArgument);
        }
        if (texture.recordingRefs != 0
            || texture.lastUseSubmission > state->completedSubmission) {
            return Fail<void>(ErrorCode::ResourceBusy);
        }
        if (!texture.initialized || texture.use != TextureUse::CopySrc) {
            return Fail<void>(ErrorCode::InvalidState);
        }
        auto read = state->backend->ReadTexture(*texture.storage, output);
        if (!read && read.GetError().errorCode == ErrorCode::DeviceLost) {
            state->lost = true;
        }
        return read;
    });
}
Result<void> BufferDevice::Destroy(TextureHandle handle) noexcept
{
    return Invoke<void>("DestroyTexture", [&]() {
        if (const auto error = state->CheckAccess(DeviceAccess::Cleanup)) {
            return Fail<void>(*error);
        }
        auto found = state->Lookup(handle);
        if (!found) {
            return Fail<void>(found.GetError().errorCode);
        }
        if (found.Value()->windowImage) {
            return Fail<void>(ErrorCode::Unsupported);
        }
        if (found.Value()->recordingRefs != 0
            || static_cast<std::size_t>(found.Value().use_count())
                   > 2 + found.Value()->inFlightRefs) {
            return Fail<void>(ErrorCode::ResourceBusy);
        }
        auto &slot = state->textures[detail::RhiHandleAccess::Slot(handle)];
        slot.texture.reset();
        if (slot.generation != std::numeric_limits<std::uint64_t>::max()) {
            ++slot.generation;
        }
        return Result<void>::Success();
    });
}
} // namespace voryx::rhi_detail

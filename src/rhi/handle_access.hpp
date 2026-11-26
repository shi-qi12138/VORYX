#pragma once

#include <voryx/rhi/pipeline.hpp>
#include <voryx/rhi/texture.hpp>

#include <cstdint>

namespace voryx::detail
{
struct RhiHandleAccess
{
    static BufferHandle Buffer(std::uint64_t device, std::uint32_t slot,
                               std::uint64_t generation) noexcept
    {
        BufferHandle result;
        result.device = device;
        result.slot = slot;
        result.generation = generation;
        return result;
    }
    static std::uint64_t Owner(BufferHandle handle) noexcept
    {
        return handle.device;
    }
    static std::uint32_t Slot(BufferHandle handle) noexcept
    {
        return handle.slot;
    }
    static std::uint64_t Generation(BufferHandle handle) noexcept
    {
        return handle.generation;
    }
    static PipelineHandle Pipeline(std::uint64_t device, std::uint32_t slot,
                                   std::uint64_t generation) noexcept
    {
        PipelineHandle result;
        result.device = device;
        result.slot = slot;
        result.generation = generation;
        return result;
    }
    static std::uint64_t Owner(PipelineHandle handle) noexcept
    {
        return handle.device;
    }
    static std::uint32_t Slot(PipelineHandle handle) noexcept
    {
        return handle.slot;
    }
    static std::uint64_t Generation(PipelineHandle handle) noexcept
    {
        return handle.generation;
    }
    static TextureHandle Texture(std::uint64_t device, std::uint32_t slot,
                                 std::uint64_t generation) noexcept
    {
        TextureHandle result;
        result.device = device;
        result.slot = slot;
        result.generation = generation;
        return result;
    }
    static std::uint64_t Owner(TextureHandle handle) noexcept
    {
        return handle.device;
    }
    static std::uint32_t Slot(TextureHandle handle) noexcept
    {
        return handle.slot;
    }
    static std::uint64_t Generation(TextureHandle handle) noexcept
    {
        return handle.generation;
    }
    template<typename Handle>
    static Handle Binding(std::uint64_t owner, std::uint64_t id) noexcept
    {
        Handle handle;
        handle.owner = owner;
        handle.id = id;
        return handle;
    }
    template<typename Tag>
    static std::uint64_t Owner(DeviceBindingHandle<Tag> handle) noexcept
    {
        return handle.owner;
    }
    template<typename Tag>
    static std::uint64_t Id(DeviceBindingHandle<Tag> handle) noexcept
    {
        return handle.id;
    }
    static FrameToken Frame(std::uint64_t device, std::uint64_t serial) noexcept
    {
        FrameToken result;
        result.device = device;
        result.serial = serial;
        return result;
    }
    static std::uint64_t Owner(FrameToken token) noexcept
    {
        return token.device;
    }
    static std::uint64_t Serial(FrameToken token) noexcept
    {
        return token.serial;
    }
    static SubmissionToken Token(std::uint64_t device,
                                 std::uint64_t serial) noexcept
    {
        SubmissionToken result;
        result.device = device;
        result.serial = serial;
        return result;
    }
    static std::uint64_t Owner(SubmissionToken token) noexcept
    {
        return token.device;
    }
    static std::uint64_t Serial(SubmissionToken token) noexcept
    {
        return token.serial;
    }
};
} // namespace voryx::detail

#pragma once

#include "vulkan_context.hpp"

#include <voryx/core/result.hpp>

namespace voryx::vulkan_backend
{
/// 设备拥有的持久映射缓冲，借用 Context，且不可复制。
class MappedBuffer
{
public:
    explicit MappedBuffer(Context &context) noexcept
        : context(context)
    {
    }
    ~MappedBuffer();
    MappedBuffer(const MappedBuffer &) = delete;
    MappedBuffer &operator=(const MappedBuffer &) = delete;

    void Initialize(VkDeviceSize size, VkBufferUsageFlags usage);
    Result<void> Write(std::uint64_t offset, ByteSpan input) noexcept;
    Result<void> Read(std::uint64_t offset, MutableByteSpan output) noexcept;
    VkBuffer buffer = VK_NULL_HANDLE;

private:
    VkMappedMemoryRange MappedRange() const noexcept;
    Context &context;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mapped = nullptr;
    bool coherent = false;
};
} // namespace voryx::vulkan_backend

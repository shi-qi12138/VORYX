#include "vulkan_mapped_buffer.hpp"

#include <cstring>

namespace voryx::vulkan_backend
{
MappedBuffer::~MappedBuffer()
{
    if (mapped != nullptr) {
        vkUnmapMemory(context.device, memory);
    }
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context.device, buffer, nullptr);
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(context.device, memory, nullptr);
    }
}

void MappedBuffer::Initialize(VkDeviceSize size, VkBufferUsageFlags usage)
{
    VkBufferCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create.size = size;
    create.usage = usage;
    create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer createdBuffer = VK_NULL_HANDLE;
    Check(vkCreateBuffer(context.device, &create, nullptr, &createdBuffer));
    buffer = createdBuffer;
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(context.device, buffer, &requirements);
    std::uint32_t selected = UINT32_MAX;
    for (int pass = 0; pass < 2 && selected == UINT32_MAX; ++pass) {
        for (std::uint32_t index = 0;
             index < context.memoryProperties.memoryTypeCount; ++index) {
            const auto flags =
                context.memoryProperties.memoryTypes[index].propertyFlags;
            if ((requirements.memoryTypeBits & (1u << index)) != 0
                && (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0
                && (pass == 1
                    || (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0)) {
                selected = index;
                coherent = (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
                break;
            }
        }
    }
    if (selected == UINT32_MAX) {
        throw NativeFailure{VK_ERROR_FEATURE_NOT_PRESENT};
    }
    VkMemoryAllocateInfo allocate{};
    allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = selected;
    VkDeviceMemory createdMemory = VK_NULL_HANDLE;
    Check(vkAllocateMemory(context.device, &allocate, nullptr, &createdMemory));
    memory = createdMemory;
    Check(vkBindBufferMemory(context.device, buffer, memory, 0));
    void *createdMapped = nullptr;
    Check(vkMapMemory(context.device, memory, 0, VK_WHOLE_SIZE, 0,
                      &createdMapped));
    mapped = createdMapped;
}

Result<void> MappedBuffer::Write(std::uint64_t offset, ByteSpan input) noexcept
{
    if (input.empty()) {
        return Result<void>::Success();
    }
    if (!coherent) {
        // 局部写入前先取回设备字节，避免刷新同一原子范围时覆盖相邻数据。
        const auto range = MappedRange();
        const auto result =
            vkInvalidateMappedMemoryRanges(context.device, 1, &range);
        if (result != VK_SUCCESS) {
            return Result<void>::Failure(
                NativeError(result, "PrepareBufferWrite"));
        }
    }
    std::memcpy(static_cast<std::byte *>(mapped) + offset, input.data(),
                input.size());
    if (!coherent) {
        const auto range = MappedRange();
        const auto result =
            vkFlushMappedMemoryRanges(context.device, 1, &range);
        if (result != VK_SUCCESS) {
            // 已修改映射内存。刷新失败后不能继续使用状态不确定的存储。
            auto error = NativeError(result, "FlushBufferMemory");
            error.errorCode = ErrorCode::DeviceLost;
            return Result<void>::Failure(std::move(error));
        }
    }
    return Result<void>::Success();
}

Result<void> MappedBuffer::Read(std::uint64_t offset,
                                MutableByteSpan output) noexcept
{
    if (output.empty()) {
        return Result<void>::Success();
    }
    if (!coherent) {
        const auto range = MappedRange();
        const auto result =
            vkInvalidateMappedMemoryRanges(context.device, 1, &range);
        if (result != VK_SUCCESS) {
            return Result<void>::Failure(
                NativeError(result, "InvalidateBufferMemory"));
        }
    }
    std::memcpy(output.data(), static_cast<const std::byte *>(mapped) + offset,
                output.size());
    return Result<void>::Success();
}

VkMappedMemoryRange MappedBuffer::MappedRange() const noexcept
{
    // 映射整个独立分配，WHOLE_SIZE 满足非 coherent 原子范围对齐要求。
    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = memory;
    range.offset = 0;
    range.size = VK_WHOLE_SIZE;
    return range;
}
} // namespace voryx::vulkan_backend

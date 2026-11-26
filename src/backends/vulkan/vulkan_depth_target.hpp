#pragma once

#include "vulkan_context.hpp"

namespace voryx::vulkan_backend
{
VkFormat ChooseDepthFormat(VkPhysicalDevice physical);

/// 交换链尺寸下的单采样深度附件。使用者先销毁引用它的帧缓冲。
class DepthTarget
{
public:
    explicit DepthTarget(Context &context) noexcept;
    ~DepthTarget();
    DepthTarget(const DepthTarget &) = delete;
    DepthTarget &operator=(const DepthTarget &) = delete;
    void Initialize(VkFormat format, VkExtent2D extent);
    VkImageView view = VK_NULL_HANDLE;

private:
    Context &context;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};
} // namespace voryx::vulkan_backend

#pragma once
#include "vulkan_graphics_pipeline.hpp"
namespace voryx::vulkan_backend
{
class DeviceTexture final : public rhi_detail::TextureStorage
{
public:
    DeviceTexture(Context &context, const TextureDesc &desc) noexcept;
    /// 图像、视图和呈现信号量由交换链持有。
    DeviceTexture(Context &context, Extent2D extent, VkFormat format,
                  VkImage image, VkImageView view,
                  VkSemaphore presentReady) noexcept;
    ~DeviceTexture() override;
    DeviceTexture(const DeviceTexture &) = delete;
    DeviceTexture &operator=(const DeviceTexture &) = delete;
    void Initialize();
    const Extent2D extent;
    const VkFormat format;
    const VkImageAspectFlags aspect;
    const bool windowImage;
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    const VkSemaphore presentReady = VK_NULL_HANDLE;

private:
    Context &context;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};
class DevicePass
{
public:
    DevicePass(Context &context, DeviceTexture &color,
               DeviceTexture *depth) noexcept;
    ~DevicePass();
    DevicePass(const DevicePass &) = delete;
    DevicePass &operator=(const DevicePass &) = delete;
    void Initialize();
    void Begin(VkCommandBuffer command, ClearColor clear,
               float clearDepth) const noexcept;

private:
    Context &context;
    DeviceTexture &color;
    DeviceTexture *depth;
    VkRenderPass pass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
};
void TextureBarrier(VkCommandBuffer command, DeviceTexture &texture,
                    TextureUse before, TextureUse after) noexcept;
} // namespace voryx::vulkan_backend

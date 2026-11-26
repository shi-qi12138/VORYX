#include "vulkan_depth_target.hpp"

namespace voryx::vulkan_backend
{
VkFormat ChooseDepthFormat(VkPhysicalDevice physical)
{
    for (const auto format : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM}) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physical, format, &properties);
        if ((properties.optimalTilingFeatures
             & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            != 0) {
            return format;
        }
    }
    throw NativeFailure{VK_ERROR_FORMAT_NOT_SUPPORTED};
}

DepthTarget::DepthTarget(Context &context) noexcept
    : context(context)
{
}

DepthTarget::~DepthTarget()
{
    if (view != VK_NULL_HANDLE) {
        vkDestroyImageView(context.device, view, nullptr);
    }
    if (image != VK_NULL_HANDLE) {
        vkDestroyImage(context.device, image, nullptr);
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(context.device, memory, nullptr);
    }
}

void DepthTarget::Initialize(VkFormat format, VkExtent2D extent)
{
    constexpr VkImageUsageFlags usage =
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VkImageFormatProperties support{};
    Check(vkGetPhysicalDeviceImageFormatProperties(
        context.physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
        usage, 0, &support));
    if (extent.width > support.maxExtent.width
        || extent.height > support.maxExtent.height
        || (support.sampleCounts & VK_SAMPLE_COUNT_1_BIT) == 0) {
        throw NativeFailure{VK_ERROR_FORMAT_NOT_SUPPORTED};
    }
    VkImageCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    create.imageType = VK_IMAGE_TYPE_2D;
    create.format = format;
    create.extent = {extent.width, extent.height, 1};
    create.mipLevels = 1;
    create.arrayLayers = 1;
    create.samples = VK_SAMPLE_COUNT_1_BIT;
    create.tiling = VK_IMAGE_TILING_OPTIMAL;
    create.usage = usage;
    VkImage createdImage = VK_NULL_HANDLE;
    Check(vkCreateImage(context.device, &create, nullptr, &createdImage));
    image = createdImage;
    memory = AllocateImageMemory(context, image);
    Check(vkBindImageMemory(context.device, image, memory, 0));
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    VkImageView createdView = VK_NULL_HANDLE;
    Check(vkCreateImageView(context.device, &viewInfo, nullptr, &createdView));
    view = createdView;
}
} // namespace voryx::vulkan_backend

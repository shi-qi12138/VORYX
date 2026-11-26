#include "vulkan_device_texture.hpp"
#include <array>
#include <exception>
namespace voryx::vulkan_backend
{
DeviceTexture::DeviceTexture(Context &context, const TextureDesc &desc) noexcept
    : extent(desc.extent)
    , format(desc.format == Format::D32_Float ? VK_FORMAT_D32_SFLOAT
                                              : VK_FORMAT_R8G8B8A8_UNORM)
    , aspect(desc.format == Format::D32_Float ? VK_IMAGE_ASPECT_DEPTH_BIT
                                              : VK_IMAGE_ASPECT_COLOR_BIT)
    , windowImage(false)
    , context(context)
{
}
DeviceTexture::DeviceTexture(Context &context, Extent2D extent, VkFormat format,
                             VkImage image, VkImageView view,
                             VkSemaphore presentReady) noexcept
    : extent(extent)
    , format(format)
    , aspect(VK_IMAGE_ASPECT_COLOR_BIT)
    , windowImage(true)
    , image(image)
    , view(view)
    , presentReady(presentReady)
    , context(context)
{
}
DeviceTexture::~DeviceTexture()
{
    if (windowImage) {
        return;
    }
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
void DeviceTexture::Initialize()
{
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(context.physical, format, &properties);
    const VkFormatFeatureFlags required =
        aspect == VK_IMAGE_ASPECT_DEPTH_BIT
            ? VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
            : (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
               | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);
    if ((properties.optimalTilingFeatures & required) != required) {
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
    create.usage = aspect == VK_IMAGE_ASPECT_DEPTH_BIT
                       ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                       : (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                          | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    // 创建成功后才接管句柄，失败时不让 RAII 读取原生输出中的无效值。
    VkImage createdImage = VK_NULL_HANDLE;
    Check(vkCreateImage(context.device, &create, nullptr, &createdImage));
    image = createdImage;
    memory = AllocateImageMemory(context, image);
    Check(vkBindImageMemory(context.device, image, memory, 0));
    VkImageViewCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = format;
    info.subresourceRange = {aspect, 0, 1, 0, 1};
    VkImageView createdView = VK_NULL_HANDLE;
    Check(vkCreateImageView(context.device, &info, nullptr, &createdView));
    view = createdView;
}
DevicePass::DevicePass(Context &context, DeviceTexture &color,
                       DeviceTexture *depth) noexcept
    : context(context)
    , color(color)
    , depth(depth)
{
}
DevicePass::~DevicePass()
{
    if (framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(context.device, framebuffer, nullptr);
    }
    if (pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context.device, pass, nullptr);
    }
}
void DevicePass::Initialize()
{
    pass = CreateRenderPass(context, color.format,
                            depth ? depth->format : VK_FORMAT_UNDEFINED);
    const std::array views{color.view, depth ? depth->view : VK_NULL_HANDLE};
    VkFramebufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = pass;
    info.attachmentCount = depth ? 2u : 1u;
    info.pAttachments = views.data();
    info.width = color.extent.width;
    info.height = color.extent.height;
    info.layers = 1;
    VkFramebuffer createdFramebuffer = VK_NULL_HANDLE;
    Check(vkCreateFramebuffer(context.device, &info, nullptr,
                              &createdFramebuffer));
    framebuffer = createdFramebuffer;
}
void DevicePass::Begin(VkCommandBuffer command, ClearColor clear,
                       float clearDepth) const noexcept
{
    std::array<VkClearValue, 2> values{};
    values[0].color = {{clear.red, clear.green, clear.blue, clear.alpha}};
    values[1].depthStencil = {clearDepth, 0};
    VkRenderPassBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = pass;
    info.framebuffer = framebuffer;
    info.renderArea = {{0, 0}, {color.extent.width, color.extent.height}};
    info.clearValueCount = depth ? 2u : 1u;
    info.pClearValues = values.data();
    vkCmdBeginRenderPass(command, &info, VK_SUBPASS_CONTENTS_INLINE);
}
namespace
{
struct ImageAccess
{
    VkPipelineStageFlags stage;
    VkAccessFlags access;
    VkImageLayout layout;
};
ImageAccess Access(TextureUse use) noexcept
{
    switch (use) {
        case TextureUse::Initial:
            return {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
                    VK_IMAGE_LAYOUT_UNDEFINED};
        case TextureUse::ColorAttachment:
            return {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        case TextureUse::DepthWrite:
            return {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                        | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        case TextureUse::CopySrc:
            return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
        case TextureUse::Present:
            return {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
        default:
            std::terminate();
    }
}
} // namespace
void TextureBarrier(VkCommandBuffer command, DeviceTexture &texture,
                    TextureUse before, TextureUse after) noexcept
{
    // 帧只支持清除式 Pass。丢弃前一帧图像内容，也覆盖首次获取尚无布局的图像。
    const auto source =
        Access(before == TextureUse::Present && texture.windowImage
                   ? TextureUse::Initial
                   : before);
    const auto destination = Access(after);
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = source.access;
    barrier.dstAccessMask = destination.access;
    barrier.oldLayout = source.layout;
    barrier.newLayout = destination.layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = texture.image;
    barrier.subresourceRange = {texture.aspect, 0, 1, 0, 1};
    vkCmdPipelineBarrier(command, source.stage, destination.stage, 0, 0,
                         nullptr, 0, nullptr, 1, &barrier);
}
} // namespace voryx::vulkan_backend

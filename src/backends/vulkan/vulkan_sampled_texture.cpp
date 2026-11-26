#include "vulkan_sampled_texture.hpp"

#include <binding_validation.hpp>
#include <cstring>
#include <vector>

namespace voryx::vulkan_backend
{
namespace
{
constexpr VkFormat textureFormat = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkImageSubresourceRange colorRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0,
                                             1};
constexpr std::size_t bytesPerPixel = 4;

struct UploadCommands
{
    explicit UploadCommands(Context &context)
        : context(context)
    {
    }
    ~UploadCommands()
    {
        if (pending) {
            vkQueueWaitIdle(context.queue);
        }
        if (pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(context.device, pool, nullptr);
        }
    }
    UploadCommands(const UploadCommands &) = delete;
    UploadCommands &operator=(const UploadCommands &) = delete;
    Context &context;
    VkCommandPool pool = VK_NULL_HANDLE;
    bool pending = false;
};
} // namespace

bool ValidTexture(const Rgba8TextureDesc &desc) noexcept
{
    return rhi_detail::ValidSampledTexture(desc);
}

SampledTexture::SampledTexture(Context &context) noexcept
    : context(context)
{
}

SampledTexture::~SampledTexture()
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

Result<void> SampledTexture::Initialize(const Rgba8TextureDesc &desc)
{
    const auto limit = context.properties.limits.maxImageDimension2D;
    if (desc.extent.width > limit || desc.extent.height > limit) {
        throw NativeFailure{VK_ERROR_FORMAT_NOT_SUPPORTED};
    }
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(context.physical, textureFormat,
                                        &properties);
    constexpr auto required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
                              | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if ((properties.optimalTilingFeatures & required) != required) {
        throw NativeFailure{VK_ERROR_FORMAT_NOT_SUPPORTED};
    }
    AllocateImage(desc.extent);
    auto uploaded = Upload(desc);
    if (!uploaded) {
        return uploaded;
    }
    CreateView();
    return Result<void>::Success();
}

void SampledTexture::AllocateImage(Extent2D extent)
{
    VkImageCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    create.imageType = VK_IMAGE_TYPE_2D;
    create.format = textureFormat;
    create.extent = {extent.width, extent.height, 1};
    create.mipLevels = 1;
    create.arrayLayers = 1;
    create.samples = VK_SAMPLE_COUNT_1_BIT;
    create.tiling = VK_IMAGE_TILING_OPTIMAL;
    create.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImage created = VK_NULL_HANDLE;
    Check(vkCreateImage(context.device, &create, nullptr, &created));
    image = created;
    memory = AllocateImageMemory(context, image);
    Check(vkBindImageMemory(context.device, image, memory, 0));
}

Result<void> SampledTexture::Upload(const Rgba8TextureDesc &desc)
{
    const auto rowBytes = desc.extent.width * bytesPerPixel;
    const auto size = rowBytes * desc.extent.height;
    const auto pitch = desc.rowPitchBytes == 0 ? rowBytes : desc.rowPitchBytes;
    ByteSpan bytes;
    std::vector<std::byte> packed;
    if (pitch == rowBytes) {
        bytes = desc.pixels.first(size);
    } else {
        // Vulkan 复制按整像素表达行距。统一打包以支持任意字节填充。
        packed.resize(size);
        for (std::size_t row = 0; row < desc.extent.height; ++row) {
            std::memcpy(packed.data() + row * rowBytes,
                        desc.pixels.data() + row * pitch, rowBytes);
        }
        bytes = packed;
    }
    MappedBuffer staging(context);
    staging.Initialize(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    auto written = staging.Write(0, bytes);
    if (!written) {
        return written;
    }
    UploadCommands upload(context);
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = context.queueFamily;
    VkCommandPool createdPool = VK_NULL_HANDLE;
    Check(
        vkCreateCommandPool(context.device, &poolInfo, nullptr, &createdPool));
    upload.pool = createdPool;
    VkCommandBufferAllocateInfo allocate{};
    allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate.commandPool = upload.pool;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1;
    VkCommandBuffer command = VK_NULL_HANDLE;
    Check(vkAllocateCommandBuffers(context.device, &allocate, &command));
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    Check(vkBeginCommandBuffer(command, &begin));
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = colorRange;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {desc.extent.width, desc.extent.height, 1};
    vkCmdCopyBufferToImage(command, staging.buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    // 传输写入对顶点和片元采样可见，后续提交沿用同一队列。
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                             | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    Check(vkEndCommandBuffer(command));
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command;
    Check(vkQueueSubmit(context.queue, 1, &submit, VK_NULL_HANDLE));
    upload.pending = true;
    Check(vkQueueWaitIdle(context.queue));
    upload.pending = false;
    return Result<void>::Success();
}

void SampledTexture::CreateView()
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = textureFormat;
    viewInfo.subresourceRange = colorRange;
    VkImageView createdView = VK_NULL_HANDLE;
    Check(vkCreateImageView(context.device, &viewInfo, nullptr, &createdView));
    view = createdView;
}
} // namespace voryx::vulkan_backend

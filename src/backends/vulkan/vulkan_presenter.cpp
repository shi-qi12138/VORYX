#include "vulkan_presenter.hpp"
#include "vulkan_depth_target.hpp"
#include "vulkan_device_texture.hpp"
#include "vulkan_frame_wait.hpp"
#include "vulkan_presenter_resources.hpp"
#include "vulkan_rhi_device.hpp"
#include "vulkan_triangle.hpp"
#include <buffer_device.hpp>

#include <error_context.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace voryx::vulkan_backend
{
namespace
{
constexpr auto forever = std::numeric_limits<std::uint64_t>::max();
constexpr std::uint64_t acquireTimeoutNs = 50'000'000;

bool SupportsPresentFence(VkPhysicalDevice physical)
{
    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT maintenance{};
    maintenance.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &maintenance;
    vkGetPhysicalDeviceFeatures2(physical, &features);
    return maintenance.swapchainMaintenance1 == VK_TRUE;
}

VkSurfaceFormatKHR ChooseFormat(const Context &context,
                                VkFormat required = VK_FORMAT_UNDEFINED)
{
    std::uint32_t count = 0;
    Check(vkGetPhysicalDeviceSurfaceFormatsKHR(
        context.physical, context.surface, &count, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(count);
    Check(vkGetPhysicalDeviceSurfaceFormatsKHR(
        context.physical, context.surface, &count, formats.data()));
    formats.resize(count);
    for (auto format : {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM}) {
        if (required != VK_FORMAT_UNDEFINED && format != required) {
            continue;
        }
        for (const auto &candidate : formats) {
            if (candidate.format == format
                && candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return candidate;
            }
        }
    }
    throw NativeFailure{VK_ERROR_FORMAT_NOT_SUPPORTED};
}

VkCompositeAlphaFlagBitsKHR ChooseAlpha(VkCompositeAlphaFlagsKHR supported)
{
    for (auto mode : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                      VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                      VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                      VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR}) {
        if ((supported & mode) != 0) {
            return mode;
        }
    }
    throw NativeFailure{VK_ERROR_FORMAT_NOT_SUPPORTED};
}

void RecordClear(VkCommandBuffer command, VkImage image, ClearColor color)
{
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = range;
    // CPU 已等待获取 fence，可丢弃旧内容并转换布局。
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
    VkClearColorValue clear{};
    clear.float32[0] = color.red;
    clear.float32[1] = color.green;
    clear.float32[2] = color.blue;
    clear.float32[3] = color.alpha;
    vkCmdClearColorImage(command, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &clear, 1, &range);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = 0;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
}

void PrepareTrianglePresentation(VkCommandBuffer command, VkImage image)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
}
} // namespace

struct Swapchain
{
    Swapchain(Context &context, VkExtent2D requested, const TriangleDraw *draw,
              bool deviceFrames = false)
        : context(context)
        , requested(requested)
        , draw(draw)
        , deviceFrames(deviceFrames)
    {
    }

    ~Swapchain()
    {
        const auto status = WaitForRetirement(forever);
        if (status != VK_SUCCESS && status != VK_ERROR_DEVICE_LOST) {
            // 析构兜底。显式关闭只使用可限时等待的 fence。
            vkDeviceWaitIdle(context.device);
        }
        if (renderDone != VK_NULL_HANDLE) {
            vkDestroyFence(context.device, renderDone, nullptr);
        }
        for (const auto &slot : presentSlots) {
            if (slot.done != VK_NULL_HANDLE) {
                vkDestroyFence(context.device, slot.done, nullptr);
            }
            if (slot.ready != VK_NULL_HANDLE) {
                vkDestroySemaphore(context.device, slot.ready, nullptr);
            }
        }
        if (pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(context.device, pool, nullptr);
        }
        for (auto framebuffer : framebuffers) {
            if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(context.device, framebuffer, nullptr);
            }
        }
        for (auto view : views) {
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(context.device, view, nullptr);
            }
        }
        if (handle != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(context.device, handle, nullptr);
        }
    }

    void Initialize()
    {
        VkSurfaceCapabilitiesKHR caps{};
        Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            context.physical, context.surface, &caps));
        VkImageUsageFlags usage = deviceFrames
                                      ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                      : VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (draw != nullptr || deviceFrames) {
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        if ((caps.supportedUsageFlags & usage) != usage) {
            throw NativeFailure{VK_ERROR_FORMAT_NOT_SUPPORTED};
        }
        extent = caps.currentExtent;
        if (extent.width == std::numeric_limits<std::uint32_t>::max()) {
            extent = {std::clamp(requested.width, caps.minImageExtent.width,
                                 caps.maxImageExtent.width),
                      std::clamp(requested.height, caps.minImageExtent.height,
                                 caps.maxImageExtent.height)};
        }
        if (extent.width == 0 || extent.height == 0) {
            throw NativeFailure{VK_ERROR_OUT_OF_DATE_KHR};
        }
        const auto format = ChooseFormat(
            context, draw != nullptr ? draw->format : VK_FORMAT_UNDEFINED);
        VkSwapchainCreateInfoKHR create{};
        create.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create.surface = context.surface;
        create.minImageCount = caps.minImageCount;
        if (deviceFrames
            && caps.minImageCount != std::numeric_limits<std::uint32_t>::max()
            && (caps.maxImageCount == 0
                || caps.minImageCount < caps.maxImageCount)) {
            ++create.minImageCount;
        }
        create.imageFormat = format.format;
        create.imageColorSpace = format.colorSpace;
        imageFormat = format.format;
        create.imageExtent = extent;
        create.imageArrayLayers = 1;
        create.imageUsage = usage;
        create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create.preTransform = caps.currentTransform;
        create.compositeAlpha = ChooseAlpha(caps.supportedCompositeAlpha);
        create.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        create.clipped = VK_TRUE;
        VkSwapchainKHR created = VK_NULL_HANDLE;
        Check(vkCreateSwapchainKHR(context.device, &create, nullptr, &created));
        handle = created;
        std::uint32_t count = 0;
        Check(vkGetSwapchainImagesKHR(context.device, handle, &count, nullptr));
        images.resize(count);
        Check(vkGetSwapchainImagesKHR(context.device, handle, &count,
                                      images.data()));
        images.resize(count);
        if (deviceFrames || draw != nullptr) {
            CreateViews();
        }
        if (draw != nullptr) {
            CreateFramebuffers();
        }
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = context.queueFamily;
        VkCommandPool createdPool = VK_NULL_HANDLE;
        Check(vkCreateCommandPool(context.device, &poolInfo, nullptr,
                                  &createdPool));
        pool = createdPool;
        VkCommandBufferAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        Check(vkAllocateCommandBuffers(context.device, &allocate, &command));
        VkFenceCreateInfo fence{};
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence createdFence = VK_NULL_HANDLE;
        Check(vkCreateFence(context.device, &fence, nullptr, &createdFence));
        renderDone = createdFence;
        VkSemaphoreCreateInfo semaphore{};
        semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        const auto countSlots = deviceFrames ? windowFrameSlots : 1u;
        for (std::uint32_t index = 0; index < countSlots; ++index) {
            auto &slot = presentSlots[index];
            VkSemaphore createdSemaphore = VK_NULL_HANDLE;
            Check(vkCreateSemaphore(context.device, &semaphore, nullptr,
                                    &createdSemaphore));
            slot.ready = createdSemaphore;
            Check(
                vkCreateFence(context.device, &fence, nullptr, &createdFence));
            slot.done = createdFence;
        }
    }

    void CreateViews()
    {
        views.resize(images.size());
        for (std::size_t index = 0; index < images.size(); ++index) {
            VkImageViewCreateInfo view{};
            view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view.image = images[index];
            view.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view.format = imageFormat;
            view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            VkImageView createdView = VK_NULL_HANDLE;
            Check(vkCreateImageView(context.device, &view, nullptr,
                                    &createdView));
            views[index] = createdView;
        }
    }

    void CreateFramebuffers()
    {
        if (draw->depthFormat != VK_FORMAT_UNDEFINED) {
            // 同一帧同步完成后才重用深度附件，各交换链图像可共享它。
            depth = std::make_unique<DepthTarget>(context);
            depth->Initialize(draw->depthFormat, extent);
        }
        framebuffers.resize(images.size());
        for (std::size_t index = 0; index < images.size(); ++index) {
            VkFramebufferCreateInfo target{};
            target.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            target.renderPass = draw->pass;
            const std::array attachments{views[index],
                                         depth ? depth->view : VK_NULL_HANDLE};
            target.attachmentCount = depth ? 2u : 1u;
            target.pAttachments = attachments.data();
            target.width = extent.width;
            target.height = extent.height;
            target.layers = 1;
            VkFramebuffer createdTarget = VK_NULL_HANDLE;
            Check(vkCreateFramebuffer(context.device, &target, nullptr,
                                      &createdTarget));
            framebuffers[index] = createdTarget;
        }
    }

    struct PresentSlot
    {
        VkSemaphore ready = VK_NULL_HANDLE;
        VkFence done = VK_NULL_HANDLE;
        bool pending = false;
    };

    void CompleteRetirement() noexcept
    {
        renderPending = false;
        for (auto &slot : presentSlots) {
            slot.pending = false;
        }
    }

    VkResult WaitForRetirement(TimeoutNs timeoutNs) noexcept
    {
        std::array<VkFence, windowFrameSlots + 1> fences{};
        std::uint32_t count = 0;
        if (renderPending) {
            fences[count++] = renderDone;
        }
        for (const auto &slot : presentSlots) {
            if (slot.pending) {
                fences[count++] = slot.done;
            }
        }
        if (count == 0) {
            return VK_SUCCESS;
        }
        const auto result = vkWaitForFences(context.device, count,
                                            fences.data(), VK_TRUE, timeoutNs);
        if (result == VK_SUCCESS || result == VK_ERROR_DEVICE_LOST) {
            CompleteRetirement();
        }
        return result;
    }

    void WaitForPresentSlot(PresentSlot &slot, TimeoutNs timeoutNs)
    {
        if (!slot.pending) {
            return;
        }
        const auto result =
            vkWaitForFences(context.device, 1, &slot.done, VK_TRUE, timeoutNs);
        Check(result == VK_TIMEOUT && timeoutNs == 0 ? VK_NOT_READY : result);
        Check(vkResetFences(context.device, 1, &slot.done));
        slot.pending = false;
    }

    std::uint32_t Acquire(const FrameWaitBudget &budget)
    {
        return AcquireImageWithinBudget(
            acquired, budget,
            [&](TimeoutNs remaining) {
                WaitForPresentSlot(presentSlots[nextPresentSlot], remaining);
            },
            [&](TimeoutNs remaining) {
                std::uint32_t index = 0;
                const auto result =
                    vkAcquireNextImageKHR(context.device, handle, remaining,
                                          VK_NULL_HANDLE, renderDone, &index);
                if (result != VK_SUBOPTIMAL_KHR) {
                    Check(result);
                }
                acquireSuboptimal = result == VK_SUBOPTIMAL_KHR;
                renderPending = true;
                return index;
            },
            [&](TimeoutNs remaining) {
                WaitForRender(remaining);
            });
    }

    void WaitForRender(TimeoutNs remaining = forever)
    {
        const auto result =
            vkWaitForFences(context.device, 1, &renderDone, VK_TRUE, remaining);
        Check(result == VK_TIMEOUT && remaining == 0 ? VK_NOT_READY : result);
        renderPending = false;
        Check(vkResetFences(context.device, 1, &renderDone));
    }

    void ReleaseAcquired()
    {
        const auto release = reinterpret_cast<PFN_vkReleaseSwapchainImagesEXT>(
            vkGetDeviceProcAddr(context.device, "vkReleaseSwapchainImagesEXT"));
        if (!release) {
            throw NativeFailure{VK_ERROR_EXTENSION_NOT_PRESENT};
        }
        VkReleaseSwapchainImagesInfoEXT info{};
        info.sType = VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_EXT;
        info.swapchain = handle;
        info.imageIndexCount = 1;
        info.pImageIndices = &*acquired;
        Check(release(context.device, &info));
        acquired.reset();
    }

    void Present(ClearColor color, bool drawTriangle)
    {
        const FrameWaitBudget budget(acquireTimeoutNs);
        const auto index = Acquire(budget);
        Check(vkResetCommandPool(context.device, pool, 0));
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        Check(vkBeginCommandBuffer(command, &begin));
        if (drawTriangle) {
            draw->Record(command, framebuffers[index], extent, color);
            PrepareTrianglePresentation(command, images[index]);
        } else {
            RecordClear(command, images[index], color);
        }
        Check(vkEndCommandBuffer(command));
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &presentSlots[nextPresentSlot].ready;
        Check(vkQueueSubmit(context.queue, 1, &submit, renderDone));
        renderPending = true;
        PresentAcquired(true);
    }

    void PresentAcquired(bool legacySubmission, bool asynchronous = false)
    {
        auto &slot = presentSlots[nextPresentSlot];
        VkSwapchainPresentFenceInfoEXT completion{};
        completion.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT;
        completion.swapchainCount = 1;
        completion.pFences = &slot.done;
        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.pNext = &completion;
        if (legacySubmission || asynchronous) {
            present.waitSemaphoreCount = 1;
            present.pWaitSemaphores = &slot.ready;
        }
        present.swapchainCount = 1;
        present.pSwapchains = &handle;
        present.pImageIndices = &*acquired;
        const auto presented = vkQueuePresentKHR(context.queue, &present);
        // 过期和表面丢失仍会入队等待信号量，因此先回收这次呈现。
        if (presented != VK_SUCCESS && presented != VK_SUBOPTIMAL_KHR
            && presented != VK_ERROR_OUT_OF_DATE_KHR
            && presented != VK_ERROR_SURFACE_LOST_KHR) {
            Check(presented);
        }
        slot.pending = true;
        acquired.reset();
        if (asynchronous) {
            nextPresentSlot = (nextPresentSlot + 1) % windowFrameSlots;
        } else {
            WaitForPresentSlot(slot, forever);
            if (legacySubmission) {
                WaitForRender();
            }
        }
        if (presented != VK_SUBOPTIMAL_KHR) {
            Check(presented);
        }
        if (acquireSuboptimal || presented == VK_SUBOPTIMAL_KHR) {
            throw NativeFailure{VK_ERROR_OUT_OF_DATE_KHR};
        }
    }

    Context &context;
    VkExtent2D requested;
    const TriangleDraw *draw;
    const bool deviceFrames;
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    std::optional<std::uint32_t> acquired;
    bool acquireSuboptimal = false;
    VkExtent2D extent{};
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkFence renderDone = VK_NULL_HANDLE;
    std::array<PresentSlot, windowFrameSlots> presentSlots{};
    bool retireRequested = false;
    std::uint32_t nextPresentSlot = 0;
    std::unique_ptr<DepthTarget> depth;
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> framebuffers;
    bool renderPending = false;
};

Presenter::Presenter(std::shared_ptr<Context> contextOwner,
                     std::unique_ptr<IDevice> device, NativeWindowDesc window,
                     ReadExtent readExtent)
    : contextOwner(std::move(contextOwner))
    , context(*this->contextOwner)
    , device(std::move(device))
    , window(window)
    , readExtent(readExtent)
    , resources(std::make_unique<PresenterResources>(context))
    , owner(std::this_thread::get_id())
{
}

Result<std::unique_ptr<IPresenter>>
CreateSurfacePresenter(std::uint64_t identity, const DeviceDesc &device,
                       NativeWindowDesc window, const char *platformExtension,
                       VkSurfaceKHR (*makeSurface)(VkInstance, const void *),
                       Presenter::ReadExtent readExtent) noexcept
{
    using Outcome = Result<std::unique_ptr<IPresenter>>;
    try {
        const char *extensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME, platformExtension,
            VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
            VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME};
        const char *deviceExtensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME};
        VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT maintenance{};
        maintenance.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
        maintenance.swapchainMaintenance1 = VK_TRUE;
        const SurfaceSource source{extensions,   makeSurface,
                                   &window,      deviceExtensions,
                                   &maintenance, SupportsPresentFence};
        auto context = std::make_shared<Context>(device);
        context->Initialize(device, &source);
        auto created = CreateVulkanRhiDevice(identity, device, context);
        if (!created) {
            return Outcome::Failure(std::move(created).GetError());
        }
        return Outcome::Success(std::make_unique<Presenter>(
            std::move(context), std::move(created).Value(), window,
            readExtent));
    } catch (const NativeFailure &error) {
        return Outcome::Failure(NativeError(error.result, "CreatePresenter"));
    } catch (const std::bad_alloc &) {
        return Outcome::Failure(detail::WithOperation(
            {ErrorCode::OutOfMemory, {}, {}, {}}, "CreatePresenter"));
    }
}

Presenter::~Presenter()
{
    if (std::this_thread::get_id() != owner) {
        std::terminate();
    }
    // 晚析构命令可以保留设备，但窗口与交换链必须在宿主释放窗口前结束。
    ResetChain();
    device.reset();
    DestroySurface();
}

void Presenter::DestroySurface() noexcept
{
    if (context.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
        context.surface = VK_NULL_HANDLE;
    }
}

VkExtent2D Presenter::WindowExtent() const
{
    return readExtent
               ? readExtent(window)
               : VkExtent2D{framebufferExtent.width, framebufferExtent.height};
}

IDevice &Presenter::GetDevice() noexcept
{
    return *device;
}

Result<void> Presenter::Shutdown(TimeoutNs timeoutNs) noexcept
{
    constexpr std::string_view operation = "Shutdown";
    if (std::this_thread::get_id() != owner) {
        return Result<void>::Failure(detail::WithOperation(
            {ErrorCode::WrongThread, {}, {}, {}}, operation));
    }
    if (context.device == VK_NULL_HANDLE) {
        return Result<void>::Success();
    }
    const FrameWaitBudget budget(timeoutNs);
    closing = true;
    auto &rhi = static_cast<rhi_detail::BufferDevice &>(*device);
    auto drained = rhi.PrepareShutdown(budget.Remaining());
    if (!drained && drained.GetError().errorCode != ErrorCode::DeviceLost) {
        return Result<void>::Failure(
            detail::WithOperation(std::move(drained).GetError(), operation));
    }

    VkResult retired = VK_SUCCESS;
    if (chain) {
        if (drained) {
            retired = chain->WaitForRetirement(budget.Remaining());
            if (retired != VK_SUCCESS && retired != VK_ERROR_DEVICE_LOST) {
                return Result<void>::Failure(NativeError(retired, operation));
            }
        } else {
            // 原生等待已确认设备丢失，跳过普通 fence 等待。
            chain->CompleteRetirement();
        }
    }

    rhi.FinishShutdown();
    chain.reset();
    triangle.reset();
    resources.reset();
    context.DestroyDevice();
    DestroySurface();
    if (!drained) {
        return Result<void>::Failure(
            detail::WithOperation(std::move(drained).GetError(), operation));
    }
    if (retired != VK_SUCCESS) {
        return Result<void>::Failure(NativeError(retired, operation));
    }
    return Result<void>::Success();
}

void Presenter::ResetChain() noexcept
{
    if (chain && chain->deviceFrames) {
        // 原生命令持有借用视图的帧缓冲，先回收命令，再销毁交换链。
        static_cast<rhi_detail::BufferDevice &>(*device)
            .DrainFrameSubmissions();
    }
    chain.reset();
}

Result<void> Presenter::RetireChain(const FrameWaitBudget &budget) noexcept
{
    if (!chain) {
        return Result<void>::Success();
    }
    auto &rhi = static_cast<rhi_detail::BufferDevice &>(*device);
    if (chain->deviceFrames) {
        auto completed = rhi.WaitForFrameSubmissions(budget.Remaining());
        if (!completed) {
            return completed;
        }
    }
    const auto retired = chain->WaitForRetirement(budget.Remaining());
    if (retired != VK_SUCCESS) {
        MarkFailure(retired);
        return Result<void>::Failure(NativeError(retired, "AcquireFrame"));
    }
    // 相关 fence 已确认完成，析构路径不会继续等待它们。
    chain.reset();
    return Result<void>::Success();
}

void Presenter::MarkFailure(VkResult result) noexcept
{
    if (result == VK_ERROR_DEVICE_LOST) {
        static_cast<rhi_detail::BufferDevice &>(*device).MarkLost();
    }
    if (result == VK_ERROR_DEVICE_LOST || result == VK_ERROR_SURFACE_LOST_KHR) {
        failed = true;
    }
}

void Presenter::ResetFailedFrame(VkResult result) noexcept
{
    MarkFailure(result);
    if ((result != VK_NOT_READY && result != VK_TIMEOUT)
        || (chain && chain->acquired)) {
        ResetChain();
    }
}

Result<void> Presenter::ClearAndPresent(ClearColor color) noexcept
{
    return PresentFrame(color, false, "ClearAndPresent");
}

Result<void> Presenter::DrawAndPresent(ClearColor background) noexcept
{
    return PresentFrame(background, true, "DrawAndPresent");
}

Result<void> Presenter::PresentFrame(ClearColor color, bool drawTriangle,
                                     std::string_view operation) noexcept
{
    const auto fail = [operation](ErrorCode code) {
        return Result<void>::Failure(
            detail::WithOperation({code, {}, {}, {}}, operation));
    };
    if (std::this_thread::get_id() != owner) {
        return fail(ErrorCode::WrongThread);
    }
    if (!closing && static_cast<rhi_detail::BufferDevice &>(*device).IsLost()) {
        return fail(ErrorCode::DeviceLost);
    }
    if (closing || failed || (drawTriangle && !triangle)
        || (chain && chain->deviceFrames && chain->acquired
            && !static_cast<rhi_detail::BufferDevice &>(*device)
                    .CheckAcquireFrame())) {
        return fail(ErrorCode::InvalidState);
    }
    for (float channel : {color.red, color.green, color.blue, color.alpha}) {
        if (!std::isfinite(channel) || channel < 0.0f || channel > 1.0f) {
            return fail(ErrorCode::InvalidArgument);
        }
    }
    try {
        const auto *draw = drawTriangle ? triangle.get() : nullptr;
        const auto extent = WindowExtent();
        if (extent.width == 0 || extent.height == 0) {
            ResetChain();
            return fail(ErrorCode::SurfaceUnavailable);
        }
        if (!chain || chain->retireRequested || chain->deviceFrames
            || extent.width != chain->requested.width
            || extent.height != chain->requested.height
            || (draw != nullptr && draw != chain->draw)) {
            ResetChain();
            auto next = std::make_unique<Swapchain>(context, extent, draw);
            next->Initialize();
            chain = std::move(next);
        }
        chain->Present(color, drawTriangle);
        return Result<void>::Success();
    } catch (const NativeFailure &error) {
        MarkFailure(error.result);
        if (error.result == VK_ERROR_OUT_OF_DATE_KHR) {
            ResetChain();
        } else if (error.result != VK_TIMEOUT && error.result != VK_NOT_READY) {
            failed = true;
        }
        return Result<void>::Failure(NativeError(
            error.result == VK_NOT_READY ? VK_TIMEOUT : error.result,
            operation));
    } catch (const std::bad_alloc &) {
        return fail(ErrorCode::OutOfMemory);
    }
}

template<typename T, typename Action>
Result<T> Presenter::AccessResources(std::string_view operation,
                                     Action &&action) noexcept
{
    const auto fail = [operation](ErrorCode code) {
        return Result<T>::Failure(
            detail::WithOperation({code, {}, {}, {}}, operation));
    };
    if (std::this_thread::get_id() != owner) {
        return fail(ErrorCode::WrongThread);
    }
    if (!closing && static_cast<rhi_detail::BufferDevice &>(*device).IsLost()) {
        return fail(ErrorCode::DeviceLost);
    }
    if (closing || failed) {
        return fail(ErrorCode::InvalidState);
    }
    try {
        auto result = action();
        if (!result && result.GetError().errorCode == ErrorCode::DeviceLost) {
            MarkFailure(VK_ERROR_DEVICE_LOST);
        }
        return result;
    } catch (const NativeFailure &error) {
        MarkFailure(error.result);
        return Result<T>::Failure(NativeError(error.result, operation));
    } catch (const std::bad_alloc &) {
        return fail(ErrorCode::OutOfMemory);
    }
}

Result<FrameInfo> Presenter::AcquireFrame(TimeoutNs timeoutNs) noexcept
{
    return AccessResources<FrameInfo>("AcquireFrame", [&]() {
        auto &rhi = static_cast<rhi_detail::BufferDevice &>(*device);
        auto checked = rhi.CheckAcquireFrame();
        if (!checked) {
            return Result<FrameInfo>::Failure(detail::WithOperation(
                std::move(checked).GetError(), "AcquireFrame"));
        }
        if (timeoutNs == forever) {
            return rhi_detail::Fail<FrameInfo>(ErrorCode::InvalidArgument);
        }
        const FrameWaitBudget budget(timeoutNs);
        try {
            const auto extent = WindowExtent();
            if (extent.width == 0 || extent.height == 0) {
                if (chain) {
                    chain->retireRequested = true;
                }
                return rhi_detail::Fail<FrameInfo>(
                    ErrorCode::SurfaceUnavailable);
            }
            if (!chain || chain->retireRequested || !chain->deviceFrames
                || extent.width != chain->requested.width
                || extent.height != chain->requested.height) {
                if (chain) {
                    chain->retireRequested = true;
                }
                auto retired = RetireChain(budget);
                if (!retired) {
                    auto error = std::move(retired).GetError();
                    if (timeoutNs == 0
                        && error.errorCode == ErrorCode::Timeout) {
                        error.errorCode = ErrorCode::NotReady;
                    }
                    return Result<FrameInfo>::Failure(detail::WithOperation(
                        std::move(error), "AcquireFrame"));
                }
                auto next =
                    std::make_unique<Swapchain>(context, extent, nullptr, true);
                next->Initialize();
                chain = std::move(next);
            }
            const auto index = chain->Acquire(budget);
            const Extent2D actual{chain->extent.width, chain->extent.height};
            const auto format = chain->imageFormat == VK_FORMAT_B8G8R8A8_UNORM
                                    ? Format::BGRA8_UNorm
                                    : Format::RGBA8_UNorm;
            auto image = std::make_unique<DeviceTexture>(
                context, actual, chain->imageFormat, chain->images[index],
                chain->views[index],
                chain->presentSlots[chain->nextPresentSlot].ready);
            auto frame = rhi.PublishFrame(actual, format, std::move(image));
            if (!frame) {
                chain->retireRequested = true;
            }
            return frame;
        } catch (const NativeFailure &error) {
            MarkFailure(error.result);
            if (chain && error.result != VK_NOT_READY
                && error.result != VK_TIMEOUT) {
                chain->retireRequested = true;
            }
            return Result<FrameInfo>::Failure(
                NativeError(error.result, "AcquireFrame"));
        } catch (const std::bad_alloc &) {
            if (chain && chain->acquired) {
                chain->retireRequested = true;
            }
            return rhi_detail::Fail<FrameInfo>(ErrorCode::OutOfMemory);
        }
    });
}

Result<void> Presenter::Present(FrameToken frame,
                                SubmissionToken submission) noexcept
{
    return AccessResources<void>("Present", [&]() {
        auto &rhi = static_cast<rhi_detail::BufferDevice &>(*device);
        auto checked = rhi.CheckPresent(frame, submission);
        if (!checked) {
            return Result<void>::Failure(detail::WithOperation(
                std::move(checked).GetError(), "Present"));
        }
        try {
            chain->PresentAcquired(false, rhi.IsFrameAsynchronous());
            rhi.ConsumeFrame();
            return Result<void>::Success();
        } catch (const NativeFailure &error) {
            // 尚未入队的呈现可使用同一帧与提交令牌重试。
            if (chain->acquired
                && (error.result == VK_ERROR_OUT_OF_HOST_MEMORY
                    || error.result == VK_ERROR_OUT_OF_DEVICE_MEMORY)) {
                return Result<void>::Failure(
                    NativeError(error.result, "Present"));
            }
            rhi.ConsumeFrame();
            ResetFailedFrame(error.result);
            return Result<void>::Failure(NativeError(error.result, "Present"));
        }
    });
}

Result<void> Presenter::CancelFrame(FrameToken frame) noexcept
{
    return AccessResources<void>("CancelFrame", [&]() {
        auto &rhi = static_cast<rhi_detail::BufferDevice &>(*device);
        auto checked = rhi.CheckCancel(frame);
        if (!checked) {
            return Result<void>::Failure(detail::WithOperation(
                std::move(checked).GetError(), "CancelFrame"));
        }
        try {
            chain->ReleaseAcquired();
            rhi.ConsumeFrame();
            return Result<void>::Success();
        } catch (const NativeFailure &error) {
            rhi.ConsumeFrame();
            ResetFailedFrame(error.result);
            return Result<void>::Failure(
                NativeError(error.result, "CancelFrame"));
        }
    });
}

template<typename Desc>
Result<void> Presenter::ConfigureDraw(const Desc &desc,
                                      std::string_view operation) noexcept
{
    return AccessResources<void>(operation, [&]() {
        const auto fail = [operation](ErrorCode code) {
            return Result<void>::Failure(
                detail::WithOperation({code, {}, {}, {}}, operation));
        };
        if (!ValidTriangleList(desc)) {
            return fail(ErrorCode::InvalidArgument);
        }
        std::shared_ptr<TextureBinding> binding;
        std::shared_ptr<BindingSet> bindings;
        if constexpr (std::is_same_v<Desc, BoundTexturedTriangleListDesc>) {
            auto found = resources->GetBinding(desc.binding, operation);
            if (!found) {
                return Result<void>::Failure(std::move(found).GetError());
            }
            binding = std::move(found).Value();
        } else if constexpr (std::is_same_v<Desc, ResourceTriangleListDesc>) {
            auto found = resources->GetBindingSet(desc.bindings, operation);
            if (!found) {
                return Result<void>::Failure(std::move(found).GetError());
            }
            bindings = std::move(found).Value();
        }
        VkSurfaceCapabilitiesKHR caps{};
        Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            context.physical, context.surface, &caps));
        if ((caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            == 0) {
            return fail(ErrorCode::Unsupported);
        }
        auto next = std::make_unique<TriangleDraw>(
            context, ChooseFormat(context).format);
        auto created = [&]() {
            if constexpr (std::is_same_v<Desc, BoundTexturedTriangleListDesc>) {
                return next->Initialize(desc, std::move(binding));
            } else if constexpr (std::is_same_v<Desc,
                                                ResourceTriangleListDesc>) {
                return next->Initialize(desc, std::move(bindings));
            } else {
                return next->Initialize(desc);
            }
        }();
        if (!created) {
            return created;
        }
        if (chain && chain->draw != nullptr) {
            ResetChain();
        }
        triangle = std::move(next);
        return Result<void>::Success();
    });
}

Result<void> Presenter::SetTriangleList(const TriangleListDesc &desc) noexcept
{
    return ConfigureDraw(desc, "SetTriangleList");
}

Result<void> Presenter::SetTexturedTriangleList(
    const TexturedTriangleListDesc &desc) noexcept
{
    return ConfigureDraw(desc, "SetTexturedTriangleList");
}

Result<void>
Presenter::SetDepthTriangleList(const DepthTriangleListDesc &desc) noexcept
{
    return ConfigureDraw(desc, "SetDepthTriangleList");
}

Result<void>
Presenter::SetIndexedTriangleList(const IndexedTriangleListDesc &desc) noexcept
{
    return ConfigureDraw(desc, "SetIndexedTriangleList");
}

Result<Texture2DHandle>
Presenter::CreateTexture2D(const Rgba8TextureDesc &desc) noexcept
{
    return AccessResources<Texture2DHandle>("CreateTexture2D", [&]() {
        return resources->CreateTexture(desc);
    });
}

Result<SamplerHandle> Presenter::CreateSampler(const SamplerDesc &desc) noexcept
{
    return AccessResources<SamplerHandle>("CreateSampler", [&]() {
        return resources->CreateSampler(desc);
    });
}

Result<TextureBindingHandle>
Presenter::CreateTextureBinding(const TextureBindingDesc &desc) noexcept
{
    return AccessResources<TextureBindingHandle>("CreateTextureBinding", [&]() {
        return resources->CreateBinding(desc);
    });
}

Result<void> Presenter::SetBoundTexturedTriangleList(
    const BoundTexturedTriangleListDesc &desc) noexcept
{
    return ConfigureDraw(desc, "SetBoundTexturedTriangleList");
}

Result<UniformBufferHandle>
Presenter::CreateUniformBuffer(ByteSpan bytes) noexcept
{
    return AccessResources<UniformBufferHandle>("CreateUniformBuffer", [&]() {
        return resources->CreateUniformBuffer(bytes);
    });
}

Result<BindingLayoutHandle>
Presenter::CreateBindingLayout(const BindingLayoutDesc &desc) noexcept
{
    return AccessResources<BindingLayoutHandle>("CreateBindingLayout", [&]() {
        return resources->CreateBindingLayout(desc);
    });
}

Result<BindingSetHandle>
Presenter::CreateBindingSet(const BindingSetDesc &desc) noexcept
{
    return AccessResources<BindingSetHandle>("CreateBindingSet", [&]() {
        return resources->CreateBindingSet(desc);
    });
}

Result<void> Presenter::SetResourceTriangleList(
    const ResourceTriangleListDesc &desc) noexcept
{
    return ConfigureDraw(desc, "SetResourceTriangleList");
}

Result<void> Presenter::Destroy(UniformBufferHandle handle) noexcept
{
    return AccessResources<void>("DestroyUniformBuffer", [&]() {
        return resources->Destroy(handle);
    });
}

Result<void> Presenter::Destroy(BindingLayoutHandle handle) noexcept
{
    return AccessResources<void>("DestroyBindingLayout", [&]() {
        return resources->Destroy(handle);
    });
}

Result<void> Presenter::Destroy(BindingSetHandle handle) noexcept
{
    return AccessResources<void>("DestroyBindingSet", [&]() {
        return resources->Destroy(handle);
    });
}

Result<void> Presenter::ResetDraw() noexcept
{
    return AccessResources<void>("ResetDraw", [&]() {
        if (chain && chain->draw != nullptr) {
            ResetChain();
        }
        triangle.reset();
        return Result<void>::Success();
    });
}

Result<void> Presenter::Destroy(TextureBindingHandle handle) noexcept
{
    return AccessResources<void>("DestroyTextureBinding", [&]() {
        return resources->Destroy(handle);
    });
}

Result<void> Presenter::Destroy(Texture2DHandle handle) noexcept
{
    return AccessResources<void>("DestroyTexture2D", [&]() {
        return resources->Destroy(handle);
    });
}

Result<void> Presenter::Destroy(SamplerHandle handle) noexcept
{
    return AccessResources<void>("DestroySampler", [&]() {
        return resources->Destroy(handle);
    });
}

Result<void> Presenter::SetFramebufferExtent(Extent2D extent) noexcept
{
    const auto fail = [](ErrorCode code) {
        return Result<void>::Failure(
            detail::WithOperation({code, {}, {}, {}}, "SetFramebufferExtent"));
    };
    if (std::this_thread::get_id() != owner) {
        return fail(ErrorCode::WrongThread);
    }
    if (closing || failed) {
        return fail(ErrorCode::InvalidState);
    }
    if (readExtent != nullptr) {
        return fail(ErrorCode::Unsupported);
    }
    framebufferExtent = extent;
    return Result<void>::Success();
}
} // namespace voryx::vulkan_backend

#include "vulkan_binding_set.hpp"
#include "vulkan_device_texture.hpp"
#include "vulkan_graphics_pipeline.hpp"
#include "vulkan_mapped_buffer.hpp"
#include "vulkan_rhi_device.hpp"

#include <buffer_backend.hpp>
#include <buffer_guard.hpp>
#include <vulkan_factory.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace voryx::vulkan_backend
{
namespace
{
template<typename T, typename Action>
Result<T> Invoke(std::string_view operation, Action &&action) noexcept
{
    return rhi_detail::Invoke<T>(operation, [&]() {
        try {
            return action();
        } catch (const NativeFailure &failure) {
            return Result<T>::Failure(NativeError(failure.result, operation));
        }
    });
}

class Storage final : public rhi_detail::BufferStorage
{
public:
    explicit Storage(std::shared_ptr<Context> context)
        : context(std::move(context))
        , allocation(*this->context)
    {
    }

    void Initialize(const BufferDesc &desc)
    {
        VkBufferUsageFlags usage = 0;
        const auto contains = [&](BufferUsage flag) {
            return (static_cast<std::uint32_t>(desc.usage)
                    & static_cast<std::uint32_t>(flag))
                   != 0;
        };
        if (contains(BufferUsage::Vertex)) {
            usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        }
        if (contains(BufferUsage::Index)) {
            usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        }
        if (contains(BufferUsage::Constant)) {
            usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }
        if (contains(BufferUsage::CopySrc)) {
            usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }
        if (contains(BufferUsage::CopyDst)) {
            usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }
        allocation.Initialize(desc.sizeBytes, usage);
    }

    Result<void> Write(std::uint64_t offset, ByteSpan input) noexcept override
    {
        return allocation.Write(offset, input);
    }

    Result<void> Read(std::uint64_t offset,
                      MutableByteSpan output) noexcept override
    {
        return allocation.Read(offset, output);
    }

    VkBuffer Buffer() const noexcept
    {
        return allocation.buffer;
    }

private:
    std::shared_ptr<Context> context;
    MappedBuffer allocation;
};

struct Access
{
    VkPipelineStageFlags stage;
    VkAccessFlags access;
};

Access GetAccess(BufferUse use)
{
    switch (use) {
        case BufferUse::Initial:
            return {VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_WRITE_BIT};
        case BufferUse::VertexRead:
            return {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT};
        case BufferUse::IndexRead:
            return {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                    VK_ACCESS_INDEX_READ_BIT};
        case BufferUse::ConstantRead:
            return {VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                    VK_ACCESS_UNIFORM_READ_BIT};
        case BufferUse::CopySrc:
            return {VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT};
        case BufferUse::CopyDst:
            return {VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT};
    }
    std::terminate();
}

struct Submission final : public rhi_detail::BackendSubmission
{
    explicit Submission(std::shared_ptr<Context> context)
        : context(std::move(context))
    {
    }
    ~Submission() override
    {
        if (pending) {
            vkDeviceWaitIdle(context->device);
        }
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(context->device, fence, nullptr);
        }
        if (pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(context->device, pool, nullptr);
        }
    }
    Result<bool> Poll() noexcept override
    {
        const auto result = vkGetFenceStatus(context->device, fence);
        if (result == VK_SUCCESS || result == VK_NOT_READY) {
            return Result<bool>::Success(result == VK_SUCCESS);
        }
        return Result<bool>::Failure(
            NativeError(result, "PollVulkanSubmission"));
    }
    Result<void> Wait(TimeoutNs timeoutNs) noexcept override
    {
        const auto result =
            vkWaitForFences(context->device, 1, &fence, VK_TRUE, timeoutNs);
        if (result == VK_SUCCESS) {
            pending = false;
            return Result<void>::Success();
        }
        return Result<void>::Failure(
            NativeError(result, "WaitVulkanSubmission"));
    }
    void AcknowledgeCompletion() noexcept override
    {
        pending = false;
    }

    std::shared_ptr<Context> context;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    bool pending = false;
    std::vector<std::unique_ptr<DevicePass>> passes;
};

class Backend final : public rhi_detail::BufferBackend
{
public:
    explicit Backend(std::shared_ptr<Context> context)
        : context(std::move(context))
    {
    }
    Result<std::unique_ptr<rhi_detail::BufferStorage>>
    CreateStorage(const BufferDesc &desc) override
    {
        return Invoke<std::unique_ptr<rhi_detail::BufferStorage>>(
            "CreateVulkanBuffer", [&]() {
                auto storage = std::make_unique<Storage>(context);
                storage->Initialize(desc);
                std::unique_ptr<rhi_detail::BufferStorage> result =
                    std::move(storage);
                return Result<std::unique_ptr<rhi_detail::BufferStorage>>::
                    Success(std::move(result));
            });
    }

    Result<std::unique_ptr<rhi_detail::PipelineStorage>>
    CreatePipeline(const GraphicsPipelineDesc &desc,
                   rhi_detail::BindingLayoutStorage *layout) override
    {
        return Invoke<std::unique_ptr<rhi_detail::PipelineStorage>>(
            "CreateVulkanPipeline", [&]() {
                constexpr std::array formats{VK_FORMAT_R8G8B8A8_UNORM,
                                             VK_FORMAT_R8G8B8A8_SRGB,
                                             VK_FORMAT_B8G8R8A8_UNORM};
                const auto format =
                    formats[static_cast<std::size_t>(desc.colorFormat)];
                VkFormatProperties properties{};
                vkGetPhysicalDeviceFormatProperties(context->physical, format,
                                                    &properties);
                VkFormatFeatureFlags required =
                    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
                if (desc.blendMode != BlendMode::Opaque) {
                    required |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
                }
                if ((properties.optimalTilingFeatures & required) != required) {
                    return rhi_detail::Fail<
                        std::unique_ptr<rhi_detail::PipelineStorage>>(
                        ErrorCode::Unsupported);
                }
                if (desc.depthTest) {
                    vkGetPhysicalDeviceFormatProperties(
                        context->physical, VK_FORMAT_D32_SFLOAT, &properties);
                    if ((properties.optimalTilingFeatures
                         & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                        == 0) {
                        return rhi_detail::Fail<
                            std::unique_ptr<rhi_detail::PipelineStorage>>(
                            ErrorCode::Unsupported);
                    }
                }
                auto pipeline = std::make_unique<GraphicsPipeline>(
                    *context, format,
                    desc.depthTest ? VK_FORMAT_D32_SFLOAT
                                   : VK_FORMAT_UNDEFINED);
                const auto nativeLayout =
                    layout ? static_cast<BindingLayout *>(layout)->handle
                           : VK_NULL_HANDLE;
                pipeline->Initialize(desc, nativeLayout);
                std::unique_ptr<rhi_detail::PipelineStorage> result =
                    std::move(pipeline);
                return Result<std::unique_ptr<rhi_detail::PipelineStorage>>::
                    Success(std::move(result));
            });
    }

    Result<std::unique_ptr<rhi_detail::TextureStorage>>
    CreateTexture(const TextureDesc &desc) override
    {
        return Invoke<std::unique_ptr<rhi_detail::TextureStorage>>(
            "CreateVulkanTexture", [&]() {
                auto texture = std::make_unique<DeviceTexture>(*context, desc);
                texture->Initialize();
                std::unique_ptr<rhi_detail::TextureStorage> result =
                    std::move(texture);
                return Result<std::unique_ptr<rhi_detail::TextureStorage>>::
                    Success(std::move(result));
            });
    }
    Result<std::unique_ptr<rhi_detail::TextureStorage>>
    CreateSampledTexture(const Rgba8TextureDesc &desc) override
    {
        using Value = std::unique_ptr<rhi_detail::TextureStorage>;
        return Invoke<Value>("CreateVulkanSampledTexture", [&]() {
            auto texture = std::make_unique<SampledTexture>(*context);
            auto uploaded = texture->Initialize(desc);
            if (!uploaded) {
                return Result<Value>::Failure(std::move(uploaded).GetError());
            }
            return Result<Value>::Success(std::move(texture));
        });
    }

    Result<std::unique_ptr<rhi_detail::SamplerStorage>>
    CreateSampler(const SamplerDesc &desc) override
    {
        using Value = std::unique_ptr<rhi_detail::SamplerStorage>;
        return Invoke<Value>("CreateVulkanSampler", [&]() {
            auto sampler = std::make_unique<TextureSampler>(*context);
            sampler->Initialize(desc);
            return Result<Value>::Success(std::move(sampler));
        });
    }

    Result<std::unique_ptr<rhi_detail::BindingLayoutStorage>>
    CreateBindingLayout(const BindingLayoutDesc &desc) override
    {
        using Value = std::unique_ptr<rhi_detail::BindingLayoutStorage>;
        return Invoke<Value>("CreateVulkanBindingLayout", [&]() {
            auto layout = std::make_unique<BindingLayout>(*context, desc);
            layout->Initialize();
            return Result<Value>::Success(std::move(layout));
        });
    }

    Result<std::unique_ptr<rhi_detail::BindingSetStorage>> CreateBindingSet(
        const rhi_detail::BindingLayoutStorage &layout,
        std::span<const rhi_detail::BackendBinding> resources) override
    {
        using Value = std::unique_ptr<rhi_detail::BindingSetStorage>;
        return Invoke<Value>("CreateVulkanBindingSet", [&]() {
            std::array<NativeBinding, maxBindingSlots> native{};
            for (std::size_t index = 0; index < resources.size(); ++index) {
                const auto &resource = resources[index];
                if (const auto *range =
                        std::get_if<rhi_detail::BackendUniformBinding>(
                            &resource)) {
                    native[index] = VkDescriptorBufferInfo{
                        static_cast<Storage *>(range->buffer)->Buffer(),
                        range->offset, range->size};
                } else if (const auto *texture =
                               std::get_if<rhi_detail::TextureStorage *>(
                                   &resource)) {
                    native[index] = VkDescriptorImageInfo{
                        VK_NULL_HANDLE,
                        static_cast<SampledTexture *>(*texture)->view,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                } else {
                    const auto *sampler = static_cast<TextureSampler *>(
                        std::get<rhi_detail::SamplerStorage *>(resource));
                    native[index] =
                        VkDescriptorImageInfo{sampler->handle, VK_NULL_HANDLE,
                                              VK_IMAGE_LAYOUT_UNDEFINED};
                }
            }
            auto set = std::make_unique<BindingSet>(
                *context, static_cast<const BindingLayout &>(layout));
            set->Initialize(std::span(native).first(resources.size()));
            return Result<Value>::Success(std::move(set));
        });
    }

    Result<void> ReadTexture(rhi_detail::TextureStorage &texture,
                             MutableByteSpan output) noexcept override
    {
        return Invoke<void>("ReadVulkanTexture", [&]() {
            Storage readback(context);
            readback.Initialize({output.size(), BufferUsage::CopyDst, {}});
            rhi_detail::BackendOperation op{};
            op.kind = rhi_detail::OperationKind::ReadTexture;
            op.texture = &texture;
            op.dst = &readback;
            auto executed = Execute(std::span(&op, 1));
            if (!executed) {
                return executed;
            }
            return readback.Read(0, output);
        });
    }
    Result<void> Execute(std::span<const rhi_detail::BackendOperation>
                             operations) noexcept override
    {
        auto queued = Enqueue(operations);
        if (!queued) {
            return Result<void>::Failure(std::move(queued).GetError());
        }
        auto waited =
            queued.Value()->Wait(std::numeric_limits<TimeoutNs>::max());
        if (!waited) {
            // 已接受的同步提交不能回滚。让调用方重建设备，保留原生错误。
            auto error = std::move(waited).GetError();
            error.errorCode = ErrorCode::DeviceLost;
            return Result<void>::Failure(std::move(error));
        }
        return waited;
    }

    Result<std::unique_ptr<rhi_detail::BackendSubmission>>
    Enqueue(std::span<const rhi_detail::BackendOperation> operations) noexcept
        override
    {
        return EnqueueCommands(operations, VK_NULL_HANDLE);
    }

    Result<std::unique_ptr<rhi_detail::BackendSubmission>>
    EnqueueFrame(std::span<const rhi_detail::BackendOperation> operations,
                 rhi_detail::TextureStorage &image) noexcept override
    {
        return EnqueueCommands(
            operations, static_cast<DeviceTexture &>(image).presentReady);
    }

private:
    Result<std::unique_ptr<rhi_detail::BackendSubmission>>
    EnqueueCommands(std::span<const rhi_detail::BackendOperation> operations,
                    VkSemaphore presentReady) noexcept
    {
        using Completion = std::unique_ptr<rhi_detail::BackendSubmission>;
        return Invoke<Completion>("SubmitVulkan", [&]() {
            auto submission = std::make_unique<Submission>(context);
            VkCommandPoolCreateInfo pool{};
            pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool.queueFamilyIndex = context->queueFamily;
            pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            VkCommandPool createdPool = VK_NULL_HANDLE;
            Check(vkCreateCommandPool(context->device, &pool, nullptr,
                                      &createdPool));
            submission->pool = createdPool;
            VkCommandBufferAllocateInfo allocation{};
            allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocation.commandPool = submission->pool;
            allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocation.commandBufferCount = 1;
            VkCommandBuffer command = VK_NULL_HANDLE;
            Check(vkAllocateCommandBuffers(context->device, &allocation,
                                           &command));
            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            Check(vkBeginCommandBuffer(command, &begin));
            VkPipelineLayout boundLayout = VK_NULL_HANDLE;
            for (const auto &operation : operations) {
                if (operation.kind
                    == rhi_detail::BackendOperation::Kind::Barrier) {
                    const auto before = GetAccess(operation.before);
                    const auto after = GetAccess(operation.after);
                    VkBufferMemoryBarrier barrier{};
                    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                    barrier.srcAccessMask = before.access;
                    barrier.dstAccessMask = after.access;
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.buffer =
                        static_cast<Storage *>(operation.src)->Buffer();
                    barrier.size = VK_WHOLE_SIZE;
                    vkCmdPipelineBarrier(command, before.stage, after.stage, 0,
                                         0, nullptr, 1, &barrier, 0, nullptr);
                } else if (operation.kind == rhi_detail::OperationKind::Copy) {
                    const VkBufferCopy copy{operation.srcOffset,
                                            operation.dstOffset,
                                            operation.size};
                    const auto destination =
                        static_cast<Storage *>(operation.dst)->Buffer();
                    // CopyDst 用途可以连续复用，重叠写入仍须按提交顺序完成。
                    VkBufferMemoryBarrier writes{};
                    writes.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                    writes.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    writes.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    writes.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    writes.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    writes.buffer = destination;
                    writes.offset = copy.dstOffset;
                    writes.size = copy.size;
                    vkCmdPipelineBarrier(command,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                                         nullptr, 1, &writes, 0, nullptr);
                    vkCmdCopyBuffer(
                        command,
                        static_cast<Storage *>(operation.src)->Buffer(),
                        destination, 1, &copy);
                } else {
                    RecordGraphics(command, operation, *submission,
                                   boundLayout);
                }
            }
            // fence 负责完成通知，屏障负责把设备写入变成主机可见。
            VkMemoryBarrier host{};
            host.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            host.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0,
                                 nullptr, 0, nullptr);
            Check(vkEndCommandBuffer(command));
            VkFenceCreateInfo fence{};
            fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            VkFence createdFence = VK_NULL_HANDLE;
            Check(
                vkCreateFence(context->device, &fence, nullptr, &createdFence));
            submission->fence = createdFence;
            VkSubmitInfo submit{};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &command;
            if (presentReady != VK_NULL_HANDLE) {
                submit.signalSemaphoreCount = 1;
                submit.pSignalSemaphores = &presentReady;
            }
            Check(vkQueueSubmit(context->queue, 1, &submit, submission->fence));
            submission->pending = true;
            return Result<Completion>::Success(std::move(submission));
        });
    }

    void RecordGraphics(VkCommandBuffer command,
                        const rhi_detail::BackendOperation &op,
                        Submission &submission, VkPipelineLayout &boundLayout)
    {
        using Kind = rhi_detail::OperationKind;
        switch (op.kind) {
            case Kind::TextureBarrier:
                TextureBarrier(command,
                               static_cast<DeviceTexture &>(*op.texture),
                               op.textureBefore, op.textureAfter);
                break;
            case Kind::BeginPass: {
                auto pass = std::make_unique<DevicePass>(
                    *context, static_cast<DeviceTexture &>(*op.texture),
                    op.depth ? static_cast<DeviceTexture *>(op.depth)
                             : nullptr);
                pass->Initialize();
                submission.passes.push_back(std::move(pass));
                submission.passes.back()->Begin(command, op.clear,
                                                op.clearDepth);
                break;
            }
            case Kind::EndPass:
                vkCmdEndRenderPass(command);
                break;
            case Kind::Pipeline: {
                const auto &pipeline =
                    static_cast<GraphicsPipeline &>(*op.pipeline);
                vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline.pipeline);
                boundLayout = pipeline.layout;
                break;
            }
            case Kind::BindingSet: {
                const auto set =
                    static_cast<BindingSet *>(op.bindingSet)->handle;
                vkCmdBindDescriptorSets(command,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        boundLayout, 0, 1, &set, 0, nullptr);
                break;
            }
            case Kind::Vertex: {
                const auto buffer = static_cast<Storage *>(op.src)->Buffer();
                const VkDeviceSize offset = op.srcOffset;
                vkCmdBindVertexBuffers(command, 0, 1, &buffer, &offset);
                break;
            }
            case Kind::Index:
                vkCmdBindIndexBuffer(
                    command, static_cast<Storage *>(op.src)->Buffer(),
                    op.srcOffset,
                    op.indexType == IndexType::UInt16 ? VK_INDEX_TYPE_UINT16
                                                      : VK_INDEX_TYPE_UINT32);
                break;
            case Kind::Viewport: {
                const auto &view = op.viewport;
                const VkViewport native{view.x,        view.y,
                                        view.width,    view.height,
                                        view.minDepth, view.maxDepth};
                vkCmdSetViewport(command, 0, 1, &native);
                break;
            }
            case Kind::Scissor: {
                const auto &rect = op.scissor;
                const VkRect2D native{{static_cast<std::int32_t>(rect.x),
                                       static_cast<std::int32_t>(rect.y)},
                                      {rect.width, rect.height}};
                vkCmdSetScissor(command, 0, 1, &native);
                break;
            }
            case Kind::Draw:
                vkCmdDraw(command, op.count, 1, op.first, 0);
                break;
            case Kind::DrawIndexed:
                vkCmdDrawIndexed(command, op.count, 1, op.first,
                                 op.vertexOffset, 0);
                break;
            case Kind::ReadTexture: {
                const auto &texture = static_cast<DeviceTexture &>(*op.texture);
                VkBufferImageCopy copy{};
                copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                copy.imageExtent = {texture.extent.width, texture.extent.height,
                                    1};
                vkCmdCopyImageToBuffer(command, texture.image,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       static_cast<Storage *>(op.dst)->Buffer(),
                                       1, &copy);
                break;
            }
            default:
                std::terminate();
        }
    }
    std::shared_ptr<Context> context;
};
} // namespace
} // namespace voryx::vulkan_backend

namespace voryx::vulkan_backend
{
Result<std::unique_ptr<IDevice>>
CreateVulkanRhiDevice(std::uint64_t identity, const DeviceDesc &desc,
                      std::shared_ptr<Context> context) noexcept
{
    return Invoke<std::unique_ptr<IDevice>>("CreateVulkanDevice", [&]() {
        DeviceInfo info{BackendType::Vulkan, context->properties.deviceName,
                        std::string(desc.debugName),
                        context->properties.deviceType
                            == VK_PHYSICAL_DEVICE_TYPE_CPU,
                        desc.enableValidation};
        const DeviceCapabilities capabilities{
            64 * 1024 * 1024,
            std::min(1024u,
                     context->properties.limits.maxMemoryAllocationCount / 2),
            4096,
            64,
            256,
            16,
            2048,
            64,
            4096,
            context->properties.limits.maxUniformBufferRange,
            context->properties.limits.minUniformBufferOffsetAlignment,
            std::min(256u,
                     context->properties.limits.maxSamplerAllocationCount),
            256,
            256,
            8,
            context->surface == VK_NULL_HANDLE ? 0u : windowFrameSlots};
        return rhi_detail::CreateBufferDevice(
            identity, std::move(info), capabilities,
            std::make_unique<Backend>(std::move(context)));
    });
}
} // namespace voryx::vulkan_backend

namespace voryx::detail
{
Result<std::unique_ptr<IDevice>>
CreateVulkanDevice(std::uint64_t identity, const DeviceDesc &desc) noexcept
{
    return vulkan_backend::Invoke<std::unique_ptr<IDevice>>(
        "CreateVulkanDevice", [&]() {
            auto context = std::make_shared<vulkan_backend::Context>(desc);
            context->Initialize(desc);
            return vulkan_backend::CreateVulkanRhiDevice(identity, desc,
                                                         std::move(context));
        });
}
} // namespace voryx::detail

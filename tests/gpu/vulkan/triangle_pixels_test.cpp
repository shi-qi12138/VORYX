#include "triangle_spirv.hpp"
#include "vulkan_depth_target.hpp"
#include "vulkan_presenter_resources.hpp"
#include "vulkan_triangle.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>

namespace voryx::vulkan_backend
{
namespace
{
constexpr VkExtent2D extent{64, 64};
using Pixel = std::array<std::uint8_t, 4>;
using Pixels = std::array<Pixel, extent.width * extent.height>;

class Target
{
public:
    explicit Target(Context &context)
        : context(context)
        , readback(context)
    {
    }

    ~Target()
    {
        vkDeviceWaitIdle(context.device);
        if (pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(context.device, pool, nullptr);
        }
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(context.device, framebuffer, nullptr);
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

    template<typename Draw>
    void Initialize(const Draw &draw)
    {
        VkImageCreateInfo create{};
        create.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        create.imageType = VK_IMAGE_TYPE_2D;
        create.format = draw.format;
        create.extent = {extent.width, extent.height, 1};
        create.mipLevels = 1;
        create.arrayLayers = 1;
        create.samples = VK_SAMPLE_COUNT_1_BIT;
        create.tiling = VK_IMAGE_TILING_OPTIMAL;
        create.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                       | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        Check(vkCreateImage(context.device, &create, nullptr, &image));
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(context.device, image, &requirements);
        std::uint32_t type = 0;
        for (; type < context.memoryProperties.memoryTypeCount; ++type) {
            if ((requirements.memoryTypeBits & (1u << type)) != 0) {
                break;
            }
        }
        if (type == context.memoryProperties.memoryTypeCount) {
            throw NativeFailure{VK_ERROR_FEATURE_NOT_PRESENT};
        }
        VkMemoryAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = type;
        Check(vkAllocateMemory(context.device, &allocate, nullptr, &memory));
        Check(vkBindImageMemory(context.device, image, memory, 0));
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = draw.format;
        viewInfo.subresourceRange = range;
        Check(vkCreateImageView(context.device, &viewInfo, nullptr, &view));
        if (draw.depthFormat != VK_FORMAT_UNDEFINED) {
            depth = std::make_unique<DepthTarget>(context);
            depth->Initialize(draw.depthFormat, extent);
        }
        const std::array attachments{view,
                                     depth ? depth->view : VK_NULL_HANDLE};
        VkFramebufferCreateInfo fb{};
        fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb.renderPass = draw.pass;
        fb.attachmentCount = depth ? 2u : 1u;
        fb.pAttachments = attachments.data();
        fb.width = extent.width;
        fb.height = extent.height;
        fb.layers = 1;
        Check(vkCreateFramebuffer(context.device, &fb, nullptr, &framebuffer));
        readback.Initialize(sizeof(Pixels), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = context.queueFamily;
        Check(vkCreateCommandPool(context.device, &poolInfo, nullptr, &pool));
    }

    template<typename Draw>
    Result<void> Capture(const Draw &draw, Pixels &pixels)
    {
        VkCommandBufferAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        VkCommandBuffer command = VK_NULL_HANDLE;
        Check(vkAllocateCommandBuffers(context.device, &allocate, &command));
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        Check(vkBeginCommandBuffer(command, &begin));
        draw.Record(command, framebuffer, extent, {0.0f, 0.0f, 1.0f, 1.0f});
        // 使用与窗口路径相同的渲染命令，将颜色附件转为测试读回源。
        VkImageMemoryBarrier transfer{};
        transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        transfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        transfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        transfer.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transfer.image = image;
        transfer.subresourceRange = range;
        vkCmdPipelineBarrier(command,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &transfer);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {extent.width, extent.height, 1};
        vkCmdCopyImageToBuffer(command, image,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               readback.buffer, 1, &copy);
        VkMemoryBarrier host{};
        host.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0,
                             nullptr, 0, nullptr);
        Check(vkEndCommandBuffer(command));
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        Check(vkQueueSubmit(context.queue, 1, &submit, VK_NULL_HANDLE));
        Check(vkQueueWaitIdle(context.queue));
        return readback.Read(0, std::as_writable_bytes(std::span(pixels)));
    }

private:
    static constexpr VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0,
                                                   1, 0, 1};
    Context &context;
    MappedBuffer readback;
    std::unique_ptr<DepthTarget> depth;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
};

struct PixelCheck
{
    std::size_t x;
    std::size_t y;
    Pixel expected;
};

template<typename Action>
void WithValidatedContext(Action &&action)
{
    std::atomic<int> errors{0};
    DeviceDesc desc{};
    desc.enableValidation = true;
    desc.allowSoftwareAdapter = VORYX_TEST_SOFTWARE != 0;
    desc.logUserData = &errors;
    desc.logCallback = [](LogLevel level, std::string_view message,
                          void *user) noexcept {
        if (level == LogLevel::Error) {
            static_cast<std::atomic<int> *>(user)->fetch_add(1);
            std::fwrite(message.data(), 1, message.size(), stderr);
            std::fputc('\n', stderr);
        }
    };
    try {
        Context context(desc);
        context.Initialize(desc);
        action(context);
    } catch (const NativeFailure &failure) {
        FAIL() << "Native Vulkan failure: " << failure.result;
    }
    EXPECT_EQ(errors.load(), 0);
}

template<typename Desc>
void ExpectPixels(const Desc &drawDesc, std::span<const PixelCheck> checks)
{
    WithValidatedContext([&](Context &context) {
        TriangleDraw draw(context, VK_FORMAT_R8G8B8A8_UNORM);
        ASSERT_TRUE(ValidTriangleList(drawDesc));
        ASSERT_TRUE(draw.Initialize(drawDesc));
        Target target(context);
        target.Initialize(draw);
        Pixels pixels{};
        ASSERT_TRUE(target.Capture(draw, pixels));
        for (const auto &check : checks) {
            EXPECT_EQ(pixels[check.y * extent.width + check.x], check.expected)
                << "Pixel at " << check.x << ", " << check.y;
        }
    });
}

TEST(VulkanTrianglePixels, DrawsTriangleOverClearColor)
{
    const std::array vertices{
        ColorVertex{0.0f, -0.75f, 1.0f, 0.0f, 0.0f},
        ColorVertex{0.75f, 0.75f, 1.0f, 0.0f, 0.0f},
        ColorVertex{-0.75f, 0.75f, 1.0f, 0.0f, 0.0f},
    };
    const std::array checks{
        PixelCheck{32, 32, {255, 0, 0, 255}},
        PixelCheck{2, 2, {0, 0, 255, 255}},
    };
    ExpectPixels(TriangleListDesc{vertices, sample_data::triangleVertexSpirv,
                                  sample_data::triangleFragmentSpirv},
                 checks);
}

TEST(VulkanTrianglePixels, SamplesPaddedRgbaRowsWithoutFlipping)
{
    const std::array vertices{
        TexturedVertex{-1.0f, -1.0f, 0.0f, 0.0f},
        TexturedVertex{1.0f, -1.0f, 1.0f, 0.0f},
        TexturedVertex{-1.0f, 1.0f, 0.0f, 1.0f},
        TexturedVertex{1.0f, -1.0f, 1.0f, 0.0f},
        TexturedVertex{1.0f, 1.0f, 1.0f, 1.0f},
        TexturedVertex{-1.0f, 1.0f, 0.0f, 1.0f},
    };
    // 第一行末尾有 3 字节填充，最后一行无需尾部填充。
    const std::array<std::uint8_t, 19> pixels{
        255, 16, 32, 192, 16,  255, 32,  160, 9,  10,
        11,  32, 16, 255, 128, 224, 192, 32,  96,
    };
    const std::array checks{
        PixelCheck{16, 16, {255, 16, 32, 192}},
        PixelCheck{48, 16, {16, 255, 32, 160}},
        PixelCheck{16, 48, {32, 16, 255, 128}},
        PixelCheck{48, 48, {224, 192, 32, 96}},
    };
    ExpectPixels(
        TexturedTriangleListDesc{vertices,
                                 {{2, 2}, 11, std::as_bytes(std::span(pixels))},
                                 sample_data::texturedVertexSpirv,
                                 sample_data::texturedFragmentSpirv},
        checks);
}
TEST(VulkanTrianglePixels, NearTriangleWinsInEitherVertexOrder)
{
    // 左侧为远处绿色三角形，右侧为近处红色三角形，中间区域重叠。
    std::array vertices{
        DepthVertex{-0.85f, -0.75f, 0.75f, 0.0f, 1.0f, 0.0f},
        DepthVertex{0.4f, -0.75f, 0.75f, 0.0f, 1.0f, 0.0f},
        DepthVertex{-0.2f, 0.8f, 0.75f, 0.0f, 1.0f, 0.0f},
        DepthVertex{-0.4f, -0.75f, 0.25f, 1.0f, 0.0f, 0.0f},
        DepthVertex{0.85f, -0.75f, 0.25f, 1.0f, 0.0f, 0.0f},
        DepthVertex{0.2f, 0.8f, 0.25f, 1.0f, 0.0f, 0.0f},
    };
    const std::array checks{
        PixelCheck{32, 24, {255, 0, 0, 255}},
        PixelCheck{14, 12, {0, 255, 0, 255}},
        PixelCheck{50, 12, {255, 0, 0, 255}},
        PixelCheck{2, 60, {0, 0, 255, 255}},
    };
    const DepthTriangleListDesc desc{vertices, sample_data::depthVertexSpirv,
                                     sample_data::triangleFragmentSpirv};
    {
        SCOPED_TRACE("far triangle first");
        ExpectPixels(desc, checks);
    }
    std::swap_ranges(vertices.begin(), vertices.begin() + 3,
                     vertices.begin() + 3);
    {
        SCOPED_TRACE("near triangle first");
        ExpectPixels(desc, checks);
    }
}
TEST(VulkanTrianglePixels, IndexedQuadSharesVerticesAndCoversTheSeam)
{
    const std::array vertices{
        ColorVertex{-0.75f, -0.75f, 0.0f, 1.0f, 0.0f},
        ColorVertex{0.75f, -0.75f, 0.0f, 1.0f, 0.0f},
        ColorVertex{-0.75f, 0.75f, 0.0f, 1.0f, 0.0f},
        ColorVertex{0.75f, 0.75f, 0.0f, 1.0f, 0.0f},
    };
    const std::array<std::uint32_t, 6> indices{0, 1, 2, 2, 1, 3};
    // 两个三角形各检查一个内部点，并覆盖共享边上的像素中心。
    const std::array checks{
        PixelCheck{16, 16, {0, 255, 0, 255}},
        PixelCheck{48, 48, {0, 255, 0, 255}},
        PixelCheck{31, 32, {0, 255, 0, 255}},
        PixelCheck{32, 31, {0, 255, 0, 255}},
        PixelCheck{2, 2, {0, 0, 255, 255}},
        PixelCheck{61, 61, {0, 0, 255, 255}},
    };
    ExpectPixels(IndexedTriangleListDesc{vertices, indices,
                                         sample_data::triangleVertexSpirv,
                                         sample_data::triangleFragmentSpirv},
                 checks);
}
TEST(VulkanTrianglePixels, ReusesTextureWithDifferentSamplersAndChecksLifetimes)
{
    WithValidatedContext([](Context &context) {
        PresenterResources resources(context);
        PresenterResources foreign(context);
        const std::array<std::uint8_t, 8> pixels{255, 0,   0, 255,
                                                 0,   255, 0, 255};
        auto texture = resources.CreateTexture(
            {{2, 1}, 0, std::as_bytes(std::span(pixels))});
        ASSERT_TRUE(texture);
        auto clamp = resources.CreateSampler(
            {TextureFilter::Nearest, TextureFilter::Nearest});
        auto repeat = resources.CreateSampler(
            {TextureFilter::Nearest, TextureFilter::Nearest,
             TextureAddressMode::Repeat, TextureAddressMode::ClampToEdge});
        ASSERT_TRUE(clamp);
        ASSERT_TRUE(repeat);
        const auto wrongOwner =
            foreign.CreateBinding({texture.Value(), clamp.Value()});
        ASSERT_FALSE(wrongOwner);
        EXPECT_EQ(wrongOwner.GetError().errorCode, ErrorCode::WrongDevice);
        auto clamped =
            resources.CreateBinding({texture.Value(), clamp.Value()});
        auto repeated =
            resources.CreateBinding({texture.Value(), repeat.Value()});
        ASSERT_TRUE(clamped);
        ASSERT_TRUE(repeated);
        const auto busyTexture = resources.Destroy(texture.Value());
        const auto busySampler = resources.Destroy(clamp.Value());
        ASSERT_FALSE(busyTexture);
        ASSERT_FALSE(busySampler);
        EXPECT_EQ(busyTexture.GetError().errorCode, ErrorCode::ResourceBusy);
        EXPECT_EQ(busySampler.GetError().errorCode, ErrorCode::ResourceBusy);
        // 常量 U=1.25，钳位读第二列，重复采样读第一列。
        const std::array vertices{
            TexturedVertex{-1.0f, -1.0f, 1.25f, 0.5f},
            TexturedVertex{1.0f, -1.0f, 1.25f, 0.5f},
            TexturedVertex{0.0f, 1.0f, 1.25f, 0.5f},
        };
        const auto render = [&](TextureBindingHandle handle, Pixel expected) {
            auto binding = resources.GetBinding(handle, "TestBinding");
            ASSERT_TRUE(binding);
            TriangleDraw draw(context, VK_FORMAT_R8G8B8A8_UNORM);
            const BoundTexturedTriangleListDesc desc{
                vertices, handle, sample_data::texturedVertexSpirv,
                sample_data::texturedFragmentSpirv};
            ASSERT_TRUE(draw.Initialize(desc, std::move(binding).Value()));
            const auto busyBinding = resources.Destroy(handle);
            ASSERT_FALSE(busyBinding);
            EXPECT_EQ(busyBinding.GetError().errorCode,
                      ErrorCode::ResourceBusy);
            Target target(context);
            target.Initialize(draw);
            Pixels rendered{};
            ASSERT_TRUE(target.Capture(draw, rendered));
            EXPECT_EQ(rendered[32 * extent.width + 32], expected);
        };
        render(clamped.Value(), {0, 255, 0, 255});
        render(repeated.Value(), {255, 0, 0, 255});
        ASSERT_TRUE(resources.Destroy(clamped.Value()));
        const auto stale = resources.GetBinding(clamped.Value(), "TestBinding");
        ASSERT_FALSE(stale);
        EXPECT_EQ(stale.GetError().errorCode, ErrorCode::InvalidHandle);
        ASSERT_TRUE(resources.Destroy(repeated.Value()));
        ASSERT_TRUE(resources.Destroy(texture.Value()));
        ASSERT_TRUE(resources.Destroy(clamp.Value()));
        ASSERT_TRUE(resources.Destroy(repeat.Value()));
    });
}

TEST(VulkanTrianglePixels, UsesSparseBindingsForTransformAndSampling)
{
    WithValidatedContext([](Context &context) {
        PresenterResources resources(context);
        PresenterResources foreign(context);
        const auto expectError = [](const auto &result, ErrorCode code) {
            ASSERT_FALSE(result);
            EXPECT_EQ(result.GetError().errorCode, code);
        };
        expectError(resources.CreateUniformBuffer({}),
                    ErrorCode::InvalidArgument);
        const std::array<std::uint8_t, 8> texels{255, 0,   0, 255,
                                                 0,   255, 0, 255};
        const auto texture = resources.CreateTexture(
            {{2, 1}, 0, std::as_bytes(std::span(texels))});
        const auto sampler = resources.CreateSampler(
            {TextureFilter::Nearest, TextureFilter::Nearest});
        ASSERT_TRUE(texture);
        ASSERT_TRUE(sampler);
        std::array transform{0.5f, 0.5f, 0.5f, 0.0f};
        const auto uniform =
            resources.CreateUniformBuffer(std::as_bytes(std::span(transform)));
        const auto foreignUniform =
            foreign.CreateUniformBuffer(std::as_bytes(std::span(transform)));
        ASSERT_TRUE(uniform);
        ASSERT_TRUE(foreignUniform);
        transform.fill(0.0f); // 上传返回后源内存可复用。

        constexpr std::array slots{
            BindingLayoutEntry{3, BindingType::UniformBuffer,
                               BindingVisibility::Vertex},
            BindingLayoutEntry{7, BindingType::SampledTexture,
                               BindingVisibility::Fragment},
            BindingLayoutEntry{11, BindingType::Sampler,
                               BindingVisibility::Fragment},
        };
        const std::array duplicate{slots[0], slots[0]};
        expectError(resources.CreateBindingLayout({duplicate}),
                    ErrorCode::InvalidArgument);
        const auto layout = resources.CreateBindingLayout({slots});
        ASSERT_TRUE(layout);
        std::array entries{
            BindingSetEntry{7, texture.Value()},
            BindingSetEntry{11, sampler.Value()},
            BindingSetEntry{3, foreignUniform.Value()},
        };
        expectError(resources.CreateBindingSet({layout.Value(), entries}),
                    ErrorCode::WrongDevice);
        entries[2].resource = sampler.Value();
        expectError(resources.CreateBindingSet({layout.Value(), entries}),
                    ErrorCode::InvalidArgument);
        entries[2] = {11, uniform.Value()};
        expectError(resources.CreateBindingSet({layout.Value(), entries}),
                    ErrorCode::InvalidArgument);
        entries[2].slot = 3;
        const auto set = resources.CreateBindingSet({layout.Value(), entries});
        ASSERT_TRUE(set);
        expectError(resources.Destroy(layout.Value()), ErrorCode::ResourceBusy);
        expectError(resources.Destroy(uniform.Value()),
                    ErrorCode::ResourceBusy);
        expectError(resources.Destroy(texture.Value()),
                    ErrorCode::ResourceBusy);
        expectError(resources.Destroy(sampler.Value()),
                    ErrorCode::ResourceBusy);

        constexpr std::array vertices{
            TexturedVertex{-1.0f, -1.0f, 0.0f, 0.0f},
            TexturedVertex{1.0f, -1.0f, 1.0f, 0.0f},
            TexturedVertex{-1.0f, 1.0f, 0.0f, 1.0f},
            TexturedVertex{1.0f, -1.0f, 1.0f, 0.0f},
            TexturedVertex{1.0f, 1.0f, 1.0f, 1.0f},
            TexturedVertex{-1.0f, 1.0f, 0.0f, 1.0f},
        };
        {
            auto bindings =
                resources.GetBindingSet(set.Value(), "TestBindingSet");
            ASSERT_TRUE(bindings);
            const ResourceTriangleListDesc desc{
                vertices, set.Value(), sample_data::resourceVertexSpirv,
                sample_data::resourceFragmentSpirv};
            ASSERT_TRUE(ValidTriangleList(desc));
            TriangleDraw draw(context, VK_FORMAT_R8G8B8A8_UNORM);
            ASSERT_TRUE(draw.Initialize(desc, std::move(bindings).Value()));
            expectError(resources.Destroy(set.Value()),
                        ErrorCode::ResourceBusy);

            Target target(context);
            target.Initialize(draw);
            Pixels rendered{};
            ASSERT_TRUE(target.Capture(draw, rendered));
            const Pixel background{0, 0, 255, 255};
            const Pixel red{255, 0, 0, 255};
            const Pixel green{0, 255, 0, 255};
            EXPECT_EQ(rendered[32 * extent.width + 16], background);
            EXPECT_EQ(rendered[10 * extent.width + 48], background);
            EXPECT_EQ(rendered[32 * extent.width + 40], red);
            EXPECT_EQ(rendered[32 * extent.width + 56], green);
        }
        ASSERT_TRUE(resources.Destroy(set.Value()));
        expectError(resources.GetBindingSet(set.Value(), "TestBindingSet"),
                    ErrorCode::InvalidHandle);
        ASSERT_TRUE(resources.Destroy(layout.Value()));
        ASSERT_TRUE(resources.Destroy(uniform.Value()));
        ASSERT_TRUE(resources.Destroy(texture.Value()));
        ASSERT_TRUE(resources.Destroy(sampler.Value()));
    });
}

// 将独立管线用于测试附件，几何上传与管线创建各自拥有寿命。
class PipelineGeometry
{
public:
    PipelineGeometry(Context &context, const GraphicsPipeline &graphics)
        : pass(graphics.pass)
        , graphics(graphics)
        , vertices(context)
    {
    }
    void Upload(ByteSpan bytes)
    {
        vertices.Initialize(bytes.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        const auto written = vertices.Write(0, bytes);
        ASSERT_TRUE(written);
    }
    void Record(VkCommandBuffer command, VkFramebuffer framebuffer,
                VkExtent2D targetExtent, ClearColor background) const
    {
        VkClearValue clear{};
        clear.color = {{background.red, background.green, background.blue,
                        background.alpha}};
        VkRenderPassBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        begin.renderPass = pass;
        begin.framebuffer = framebuffer;
        begin.renderArea = {{0, 0}, targetExtent};
        begin.clearValueCount = 1;
        begin.pClearValues = &clear;
        vkCmdBeginRenderPass(command, &begin, VK_SUBPASS_CONTENTS_INLINE);
        const VkViewport viewport{0,
                                  0,
                                  static_cast<float>(targetExtent.width),
                                  static_cast<float>(targetExtent.height),
                                  0,
                                  1};
        const VkRect2D scissor{{0, 0}, targetExtent};
        vkCmdSetViewport(command, 0, 1, &viewport);
        vkCmdSetScissor(command, 0, 1, &scissor);
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          graphics.pipeline);
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(command, 0, 1, &vertices.buffer, &offset);
        vkCmdDraw(command, 3, 1, 0, 0);
        vkCmdEndRenderPass(command);
    }
    const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    const VkRenderPass pass;
    const VkFormat depthFormat = VK_FORMAT_UNDEFINED;

private:
    const GraphicsPipeline &graphics;
    MappedBuffer vertices;
};

TEST(VulkanTrianglePixels,
     IndependentPipelineReusesPaddedVerticesAndAppliesRasterState)
{
    WithValidatedContext([](Context &context) {
        // 颜色在位置之前，额外填充位于两属性之间。
        constexpr std::array attributes{
            VertexAttribute{1, VertexFormat::Float3, 0},
            VertexAttribute{0, VertexFormat::Float2, 16},
        };
        GraphicsPipelineDesc desc{};
        desc.vertexSpirv = sample_data::triangleVertexSpirv;
        desc.fragmentSpirv = sample_data::triangleFragmentSpirv;
        desc.vertexStrideBytes = 24;
        desc.vertexAttributes = attributes;
        const auto capture = [&](const GraphicsPipeline &pipeline, float red,
                                 float green) {
            const std::array<std::array<float, 6>, 3> data{{
                {red, green, 0, 99, -0.8f, -0.8f},
                {red, green, 0, 99, 0.8f, -0.8f},
                {red, green, 0, 99, 0, 0.8f},
            }};
            PipelineGeometry draw(context, pipeline);
            draw.Upload(std::as_bytes(std::span(data)));
            Target target(context);
            target.Initialize(draw);
            Pixels pixels{};
            const auto captured = target.Capture(draw, pixels);
            EXPECT_TRUE(captured);
            const Pixel background{0, 0, 255, 255};
            EXPECT_EQ(pixels[2 * extent.width + 2], background);
            return pixels[32 * extent.width + 32];
        };
        GraphicsPipeline reusable(context, VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_FORMAT_UNDEFINED);
        reusable.Initialize(desc);
        const Pixel red{255, 0, 0, 255};
        const Pixel green{0, 255, 0, 255};
        EXPECT_EQ(capture(reusable, 1, 0), red);
        EXPECT_EQ(capture(reusable, 0, 1), green);

        desc.cullMode = CullMode::Back;
        GraphicsPipeline firstWinding(context, VK_FORMAT_R8G8B8A8_UNORM,
                                      VK_FORMAT_UNDEFINED);
        firstWinding.Initialize(desc);
        const auto first = capture(firstWinding, 1, 0);
        desc.frontFace = FrontFace::Clockwise;
        GraphicsPipeline otherWinding(context, VK_FORMAT_R8G8B8A8_UNORM,
                                      VK_FORMAT_UNDEFINED);
        otherWinding.Initialize(desc);
        const auto other = capture(otherWinding, 1, 0);
        const Pixel background{0, 0, 255, 255};
        EXPECT_TRUE((first == red && other == background)
                    || (first == background && other == red));

        desc.cullMode = CullMode::None;
        desc.fragmentSpirv = sample_data::triangleAlphaFragmentSpirv;
        desc.blendMode = BlendMode::Alpha;
        GraphicsPipeline alpha(context, VK_FORMAT_R8G8B8A8_UNORM,
                               VK_FORMAT_UNDEFINED);
        alpha.Initialize(desc);
        const auto blended = capture(alpha, 1, 0);
        EXPECT_NEAR(blended[0], 128, 1);
        EXPECT_EQ(blended[1], 0);
        EXPECT_NEAR(blended[2], 128, 1);
        EXPECT_EQ(blended[3], 255);

        desc.blendMode = BlendMode::PremultipliedAlpha;
        GraphicsPipeline premultiplied(context, VK_FORMAT_R8G8B8A8_UNORM,
                                       VK_FORMAT_UNDEFINED);
        premultiplied.Initialize(desc);
        const auto premultipliedPixel = capture(premultiplied, 0.5f, 0);
        EXPECT_NEAR(premultipliedPixel[0], 128, 1);
        EXPECT_NEAR(premultipliedPixel[2], 128, 1);
        EXPECT_EQ(premultipliedPixel[3], 255);
    });
}
} // namespace
} // namespace voryx::vulkan_backend

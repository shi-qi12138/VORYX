#pragma once

#include "vulkan_binding_set.hpp"
#include "vulkan_graphics_pipeline.hpp"

#include <voryx/rhi/presenter.hpp>

namespace voryx::vulkan_backend
{
class TriangleDraw
{
public:
    TriangleDraw(Context &context, VkFormat format) noexcept;
    ~TriangleDraw();
    TriangleDraw(const TriangleDraw &) = delete;
    TriangleDraw &operator=(const TriangleDraw &) = delete;

    Result<void> Initialize(const TriangleListDesc &desc);
    Result<void> Initialize(const TexturedTriangleListDesc &desc);
    Result<void> Initialize(const BoundTexturedTriangleListDesc &desc,
                            std::shared_ptr<TextureBinding> existing);
    Result<void> Initialize(const ResourceTriangleListDesc &desc,
                            std::shared_ptr<BindingSet> resources);
    Result<void> Initialize(const DepthTriangleListDesc &desc);
    Result<void> Initialize(const IndexedTriangleListDesc &desc);
    void Record(VkCommandBuffer command, VkFramebuffer framebuffer,
                VkExtent2D extent, ClearColor background) const noexcept;

    const VkFormat format;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkRenderPass pass = VK_NULL_HANDLE;

private:
    Result<void>
    InitializeGeometry(ByteSpan vertexData, std::size_t vertexCount,
                       std::span<const std::uint32_t> vertexSpirv,
                       std::span<const std::uint32_t> fragmentSpirv);
    Context &context;
    MappedBuffer vertices;
    MappedBuffer indices;
    std::shared_ptr<TextureBinding> binding;
    std::shared_ptr<BindingSet> bindings;
    std::unique_ptr<GraphicsPipeline> graphics;
    std::uint32_t count = 0;
    std::uint32_t indexCount = 0;
};

bool ValidTriangleList(const TriangleListDesc &desc) noexcept;
bool ValidTriangleList(const TexturedTriangleListDesc &desc) noexcept;
bool ValidTriangleList(const BoundTexturedTriangleListDesc &desc) noexcept;
bool ValidTriangleList(const ResourceTriangleListDesc &desc) noexcept;
bool ValidTriangleList(const DepthTriangleListDesc &desc) noexcept;
bool ValidTriangleList(const IndexedTriangleListDesc &desc) noexcept;
} // namespace voryx::vulkan_backend

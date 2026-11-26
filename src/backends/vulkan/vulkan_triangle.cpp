#include "vulkan_triangle.hpp"
#include "vulkan_depth_target.hpp"

#include <array>
#include <cmath>
#include <cstddef>

namespace voryx::vulkan_backend
{
namespace
{
bool IsUnit(float value) noexcept
{
    return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

constexpr std::size_t maxElements = 3'000'000;

bool ValidDrawInputs(std::size_t count,
                     std::span<const std::uint32_t> vertexSpirv,
                     std::span<const std::uint32_t> fragmentSpirv) noexcept
{
    const auto shaderHeader = [](std::span<const std::uint32_t> words) {
        // 检查基本 SPIR-V 头。模块语义由离线编译器与 Vulkan 验证层验证。
        return words.size() > 5 && words.size() <= 262'144
               && words[0] == 0x07230203u && words[3] != 0 && words[4] == 0;
    };
    return count != 0 && count % 3 == 0 && count <= maxElements
           && shaderHeader(vertexSpirv) && shaderHeader(fragmentSpirv);
}
bool ValidColorVertices(std::span<const ColorVertex> vertices) noexcept
{
    for (const auto &vertex : vertices) {
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y)) {
            return false;
        }
        for (float channel : {vertex.red, vertex.green, vertex.blue}) {
            if (!IsUnit(channel)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

bool ValidTriangleList(const TriangleListDesc &desc) noexcept
{
    return ValidDrawInputs(desc.vertices.size(), desc.vertexSpirv,
                           desc.fragmentSpirv)
           && ValidColorVertices(desc.vertices);
}

bool ValidTriangleList(const IndexedTriangleListDesc &desc) noexcept
{
    if (desc.vertices.empty() || desc.vertices.size() > maxElements
        || !ValidDrawInputs(desc.indices.size(), desc.vertexSpirv,
                            desc.fragmentSpirv)
        || !ValidColorVertices(desc.vertices)) {
        return false;
    }
    for (const auto index : desc.indices) {
        if (index >= desc.vertices.size()) {
            return false;
        }
    }
    return true;
}

namespace
{
bool ValidTexturedVertices(std::span<const TexturedVertex> vertices) noexcept
{
    for (const auto &vertex : vertices) {
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y)
            || !std::isfinite(vertex.u) || !std::isfinite(vertex.v)) {
            return false;
        }
    }
    return true;
}

} // namespace

bool ValidTriangleList(const TexturedTriangleListDesc &desc) noexcept
{
    return ValidTexture(desc.texture)
           && ValidDrawInputs(desc.vertices.size(), desc.vertexSpirv,
                              desc.fragmentSpirv)
           && ValidTexturedVertices(desc.vertices);
}

bool ValidTriangleList(const BoundTexturedTriangleListDesc &desc) noexcept
{
    return ValidDrawInputs(desc.vertices.size(), desc.vertexSpirv,
                           desc.fragmentSpirv)
           && ValidTexturedVertices(desc.vertices);
}

bool ValidTriangleList(const ResourceTriangleListDesc &desc) noexcept
{
    return ValidDrawInputs(desc.vertices.size(), desc.vertexSpirv,
                           desc.fragmentSpirv)
           && ValidTexturedVertices(desc.vertices);
}

bool ValidTriangleList(const DepthTriangleListDesc &desc) noexcept
{
    if (!ValidDrawInputs(desc.vertices.size(), desc.vertexSpirv,
                         desc.fragmentSpirv)) {
        return false;
    }
    for (const auto &vertex : desc.vertices) {
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y)
            || !IsUnit(vertex.z) || !IsUnit(vertex.red) || !IsUnit(vertex.green)
            || !IsUnit(vertex.blue)) {
            return false;
        }
    }
    return true;
}

TriangleDraw::TriangleDraw(Context &context, VkFormat format) noexcept
    : format(format)
    , context(context)
    , vertices(context)
    , indices(context)
{
}

TriangleDraw::~TriangleDraw() = default;

Result<void> TriangleDraw::Initialize(const TriangleListDesc &desc)
{
    return InitializeGeometry(std::as_bytes(desc.vertices),
                              desc.vertices.size(), desc.vertexSpirv,
                              desc.fragmentSpirv);
}

Result<void> TriangleDraw::Initialize(const TexturedTriangleListDesc &desc)
{
    auto texture = std::make_shared<SampledTexture>(context);
    auto uploaded = texture->Initialize(desc.texture);
    if (!uploaded) {
        return uploaded;
    }
    auto sampler = std::make_shared<TextureSampler>(context);
    sampler->Initialize({TextureFilter::Nearest, TextureFilter::Nearest});
    binding = std::make_shared<TextureBinding>(context, std::move(texture),
                                               std::move(sampler));
    binding->Initialize();
    return InitializeGeometry(std::as_bytes(desc.vertices),
                              desc.vertices.size(), desc.vertexSpirv,
                              desc.fragmentSpirv);
}

Result<void> TriangleDraw::Initialize(const BoundTexturedTriangleListDesc &desc,
                                      std::shared_ptr<TextureBinding> existing)
{
    binding = std::move(existing);
    return InitializeGeometry(std::as_bytes(desc.vertices),
                              desc.vertices.size(), desc.vertexSpirv,
                              desc.fragmentSpirv);
}

Result<void> TriangleDraw::Initialize(const ResourceTriangleListDesc &desc,
                                      std::shared_ptr<BindingSet> resources)
{
    bindings = std::move(resources);
    return InitializeGeometry(std::as_bytes(desc.vertices),
                              desc.vertices.size(), desc.vertexSpirv,
                              desc.fragmentSpirv);
}

Result<void> TriangleDraw::Initialize(const DepthTriangleListDesc &desc)
{
    depthFormat = ChooseDepthFormat(context.physical);
    return InitializeGeometry(std::as_bytes(desc.vertices),
                              desc.vertices.size(), desc.vertexSpirv,
                              desc.fragmentSpirv);
}

Result<void> TriangleDraw::Initialize(const IndexedTriangleListDesc &desc)
{
    const auto indexData = std::as_bytes(desc.indices);
    indices.Initialize(indexData.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    auto uploaded = indices.Write(0, indexData);
    if (!uploaded) {
        return uploaded;
    }
    indexCount = static_cast<std::uint32_t>(desc.indices.size());
    return InitializeGeometry(std::as_bytes(desc.vertices),
                              desc.vertices.size(), desc.vertexSpirv,
                              desc.fragmentSpirv);
}

Result<void>
TriangleDraw::InitializeGeometry(ByteSpan vertexData, std::size_t vertexCount,
                                 std::span<const std::uint32_t> vertexSpirv,
                                 std::span<const std::uint32_t> fragmentSpirv)
{
    vertices.Initialize(vertexData.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    auto uploaded = vertices.Write(0, vertexData);
    if (!uploaded) {
        return uploaded;
    }
    count = static_cast<std::uint32_t>(vertexCount);
    const bool textured = binding || bindings;
    const bool hasDepth = depthFormat != VK_FORMAT_UNDEFINED;
    const auto stride =
        hasDepth ? sizeof(DepthVertex)
                 : (textured ? sizeof(TexturedVertex) : sizeof(ColorVertex));
    const auto offset = hasDepth ? offsetof(DepthVertex, red)
                                 : (textured ? offsetof(TexturedVertex, u)
                                             : offsetof(ColorVertex, red));
    const std::array attributes{
        VertexAttribute{
            0, hasDepth ? VertexFormat::Float3 : VertexFormat::Float2, 0},
        VertexAttribute{1,
                        textured ? VertexFormat::Float2 : VertexFormat::Float3,
                        static_cast<std::uint32_t>(offset)},
    };
    GraphicsPipelineDesc desc{};
    desc.vertexSpirv = vertexSpirv;
    desc.fragmentSpirv = fragmentSpirv;
    desc.vertexStrideBytes = static_cast<std::uint32_t>(stride);
    desc.vertexAttributes = attributes;
    desc.depthTest = hasDepth;
    desc.depthWrite = hasDepth;
    desc.depthCompare = DepthCompare::Less;
    const VkDescriptorSetLayout setLayout =
        bindings ? bindings->Layout()
                 : (binding ? binding->layout : VK_NULL_HANDLE);
    graphics = std::make_unique<GraphicsPipeline>(context, format, depthFormat);
    graphics->Initialize(desc, setLayout);
    pass = graphics->pass;
    return Result<void>::Success();
}

void TriangleDraw::Record(VkCommandBuffer command, VkFramebuffer framebuffer,
                          VkExtent2D extent,
                          ClearColor background) const noexcept
{
    std::array<VkClearValue, 2> clear{};
    clear[0].color.float32[0] = background.red;
    clear[0].color.float32[1] = background.green;
    clear[0].color.float32[2] = background.blue;
    clear[0].color.float32[3] = background.alpha;
    clear[1].depthStencil = {1.0f, 0};
    const VkRect2D area{{0, 0}, extent};
    VkRenderPassBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.renderPass = pass;
    begin.framebuffer = framebuffer;
    begin.renderArea = area;
    begin.clearValueCount = depthFormat == VK_FORMAT_UNDEFINED ? 1u : 2u;
    begin.pClearValues = clear.data();
    vkCmdBeginRenderPass(command, &begin, VK_SUBPASS_CONTENTS_INLINE);
    const VkViewport viewport{0.0f,
                              0.0f,
                              static_cast<float>(extent.width),
                              static_cast<float>(extent.height),
                              0.0f,
                              1.0f};
    vkCmdSetViewport(command, 0, 1, &viewport);
    vkCmdSetScissor(command, 0, 1, &area);
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      graphics->pipeline);
    const VkDescriptorSet set =
        bindings ? bindings->handle : (binding ? binding->set : VK_NULL_HANDLE);
    if (set != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                graphics->layout, 0, 1, &set, 0, nullptr);
    }
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(command, 0, 1, &vertices.buffer, &offset);
    if (indexCount != 0) {
        vkCmdBindIndexBuffer(command, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(command, indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(command, count, 1, 0, 0);
    }
    vkCmdEndRenderPass(command);
}
} // namespace voryx::vulkan_backend

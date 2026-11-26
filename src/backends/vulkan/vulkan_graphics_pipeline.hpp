#pragma once
#include "vulkan_context.hpp"
#include <buffer_backend.hpp>

namespace voryx::vulkan_backend
{
VkRenderPass CreateRenderPass(Context &context, VkFormat format,
                              VkFormat depthFormat);

/// 独立于窗口与几何的原生不可变管线。Context 必须覆盖其完整寿命。
class GraphicsPipeline final : public rhi_detail::PipelineStorage
{
public:
    GraphicsPipeline(Context &context, VkFormat format,
                     VkFormat depthFormat) noexcept;
    ~GraphicsPipeline() override;
    GraphicsPipeline(const GraphicsPipeline &) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;
    void Initialize(const GraphicsPipelineDesc &desc,
                    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE);
    VkRenderPass pass = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

private:
    Context &context;
    const VkFormat format;
    const VkFormat depthFormat;
};
} // namespace voryx::vulkan_backend

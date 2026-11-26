#pragma once
#include <voryx/rhi/pipeline.hpp>
namespace voryx::rhi_detail
{
/// 验证结构和阶段入口，完整 Shader 语义由离线工具验证。
bool ValidGraphicsPipeline(const GraphicsPipelineDesc &desc,
                           std::span<const BindingLayoutEntry> layout) noexcept;
} // namespace voryx::rhi_detail

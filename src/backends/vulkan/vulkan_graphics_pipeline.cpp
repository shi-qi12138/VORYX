#include "vulkan_graphics_pipeline.hpp"
#include <array>

namespace voryx::vulkan_backend
{
namespace
{
struct ShaderModule
{
    explicit ShaderModule(VkDevice device, std::span<const std::uint32_t> words)
        : device(device)
    {
        VkShaderModuleCreateInfo create{};
        create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create.codeSize = words.size_bytes();
        create.pCode = words.data();
        Check(vkCreateShaderModule(device, &create, nullptr, &module));
    }
    ~ShaderModule()
    {
        vkDestroyShaderModule(device, module, nullptr);
    }
    ShaderModule(const ShaderModule &) = delete;
    ShaderModule &operator=(const ShaderModule &) = delete;
    VkDevice device;
    VkShaderModule module = VK_NULL_HANDLE;
};
} // namespace
GraphicsPipeline::GraphicsPipeline(Context &context, VkFormat format,
                                   VkFormat depthFormat) noexcept
    : context(context)
    , format(format)
    , depthFormat(depthFormat)
{
}
GraphicsPipeline::~GraphicsPipeline()
{
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.device, pipeline, nullptr);
    }
    if (layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context.device, layout, nullptr);
    }
    if (pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context.device, pass, nullptr);
    }
}

VkRenderPass CreateRenderPass(Context &context, VkFormat format,
                              VkFormat depthFormat)
{
    const bool hasDepth = depthFormat != VK_FORMAT_UNDEFINED;
    std::array<VkAttachmentDescription, 2> attachments{};
    auto &color = attachments[0];
    color.format = format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    const VkAttachmentReference reference{
        0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkAttachmentReference depthReference{
        1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &reference;
    VkSubpassDependency dependency{};
    VkRenderPassCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create.attachmentCount = hasDepth ? 2u : 1u;
    create.pAttachments = attachments.data();
    create.subpassCount = 1;
    create.pSubpasses = &subpass;
    if (hasDepth) {
        auto &depth = attachments[1];
        depth.format = depthFormat;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        subpass.pDepthStencilAttachment = &depthReference;
    }
    // 同步前一 Pass 的附件写入与本次布局转换、清除和深度测试。
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                              | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                              | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = dependency.srcStageMask;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                               | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask =
        dependency.srcAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    create.dependencyCount = 1;
    create.pDependencies = &dependency;
    VkRenderPass created = VK_NULL_HANDLE;
    Check(vkCreateRenderPass(context.device, &create, nullptr, &created));
    return created;
}

void GraphicsPipeline::Initialize(const GraphicsPipelineDesc &desc,
                                  VkDescriptorSetLayout setLayout)
{
    pass = CreateRenderPass(context, format, depthFormat);
    const ShaderModule vertex(context.device, desc.vertexSpirv);
    const ShaderModule fragment(context.device, desc.fragmentSpirv);
    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertex.module;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragment.module;
    stages[1].pName = "main";

    const bool hasDepth = depthFormat != VK_FORMAT_UNDEFINED;
    const VkVertexInputBindingDescription vertexBinding{
        0, desc.vertexStrideBytes, VK_VERTEX_INPUT_RATE_VERTEX};
    std::array<VkVertexInputAttributeDescription, 16> attributes{};
    constexpr std::array formats{VK_FORMAT_R32G32_SFLOAT,
                                 VK_FORMAT_R32G32B32_SFLOAT,
                                 VK_FORMAT_R32G32B32A32_SFLOAT};
    for (std::size_t index = 0; index < desc.vertexAttributes.size(); ++index) {
        const auto &input = desc.vertexAttributes[index];
        attributes[index] = {input.location, 0,
                             formats[static_cast<std::size_t>(input.format)],
                             input.offsetBytes};
    }
    VkPipelineVertexInputStateCreateInfo inputs{};
    inputs.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    inputs.vertexBindingDescriptionCount =
        desc.vertexAttributes.empty() ? 0u : 1u;
    inputs.pVertexBindingDescriptions = &vertexBinding;
    inputs.vertexAttributeDescriptionCount =
        static_cast<std::uint32_t>(desc.vertexAttributes.size());
    inputs.pVertexAttributeDescriptions = attributes.data();
    VkPipelineInputAssemblyStateCreateInfo topology{};
    topology.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    topology.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    view.viewportCount = 1;
    view.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    constexpr std::array cullModes{VK_CULL_MODE_NONE, VK_CULL_MODE_FRONT_BIT,
                                   VK_CULL_MODE_BACK_BIT};
    raster.cullMode = cullModes[static_cast<std::size_t>(desc.cullMode)];
    raster.frontFace = desc.frontFace == FrontFace::CounterClockwise
                           ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                           : VK_FRONT_FACE_CLOCKWISE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo samples{};
    samples.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depthState{};
    depthState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthState.depthTestEnable = desc.depthTest;
    depthState.depthWriteEnable = desc.depthWrite;
    constexpr std::array comparisons{
        VK_COMPARE_OP_LESS, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_ALWAYS};
    depthState.depthCompareOp =
        comparisons[static_cast<std::size_t>(desc.depthCompare)];
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    attachment.blendEnable = desc.blendMode != BlendMode::Opaque;
    attachment.srcColorBlendFactor = desc.blendMode == BlendMode::Alpha
                                         ? VK_BLEND_FACTOR_SRC_ALPHA
                                         : VK_BLEND_FACTOR_ONE;
    attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    attachment.colorBlendOp = VK_BLEND_OP_ADD;
    attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &attachment;
    const std::array states{VK_DYNAMIC_STATE_VIEWPORT,
                            VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = states.data();
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (setLayout != VK_NULL_HANDLE) {
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &setLayout;
    }
    VkPipelineLayout createdLayout = VK_NULL_HANDLE;
    Check(vkCreatePipelineLayout(context.device, &layoutInfo, nullptr,
                                 &createdLayout));
    layout = createdLayout;
    VkGraphicsPipelineCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create.stageCount = 2;
    create.pStages = stages.data();
    create.pVertexInputState = &inputs;
    create.pInputAssemblyState = &topology;
    create.pViewportState = &view;
    create.pRasterizationState = &raster;
    create.pMultisampleState = &samples;
    create.pColorBlendState = &blend;
    create.pDepthStencilState = hasDepth ? &depthState : nullptr;
    create.pDynamicState = &dynamic;
    create.layout = layout;
    create.renderPass = pass;
    // Vulkan 可在错误时返回已创建的管线，直接交给 RAII 回收。
    Check(vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &create,
                                    nullptr, &pipeline));
}

} // namespace voryx::vulkan_backend

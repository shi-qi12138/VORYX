#include "vulkan_texture_binding.hpp"

#include <binding_validation.hpp>
#include <utility>

namespace voryx::vulkan_backend
{
bool ValidSampler(const SamplerDesc &desc) noexcept
{
    return rhi_detail::ValidSampler(desc);
}

TextureSampler::TextureSampler(Context &context) noexcept
    : context(context)
{
}

TextureSampler::~TextureSampler()
{
    if (handle != VK_NULL_HANDLE) {
        vkDestroySampler(context.device, handle, nullptr);
    }
}

void TextureSampler::Initialize(const SamplerDesc &desc)
{
    if (desc.minFilter == TextureFilter::Linear
        || desc.magFilter == TextureFilter::Linear) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(
            context.physical, VK_FORMAT_R8G8B8A8_UNORM, &properties);
        if ((properties.optimalTilingFeatures
             & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
            == 0) {
            throw NativeFailure{VK_ERROR_FORMAT_NOT_SUPPORTED};
        }
    }
    const auto filter = [](TextureFilter value) {
        return value == TextureFilter::Linear ? VK_FILTER_LINEAR
                                              : VK_FILTER_NEAREST;
    };
    const auto address = [](TextureAddressMode value) {
        return value == TextureAddressMode::Repeat
                   ? VK_SAMPLER_ADDRESS_MODE_REPEAT
                   : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    };
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = filter(desc.magFilter);
    samplerInfo.minFilter = filter(desc.minFilter);
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = address(desc.addressU);
    samplerInfo.addressModeV = address(desc.addressV);
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSampler createdSampler = VK_NULL_HANDLE;
    Check(vkCreateSampler(context.device, &samplerInfo, nullptr,
                          &createdSampler));
    handle = createdSampler;
}

TextureBinding::TextureBinding(Context &context,
                               std::shared_ptr<SampledTexture> texture,
                               std::shared_ptr<TextureSampler> sampler) noexcept
    : context(context)
    , texture(std::move(texture))
    , sampler(std::move(sampler))
{
}

TextureBinding::~TextureBinding()
{
    if (pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context.device, pool, nullptr);
    }
    if (layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context.device, layout, nullptr);
    }
}

void TextureBinding::Initialize()
{
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    VkDescriptorSetLayout createdLayout = VK_NULL_HANDLE;
    Check(vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr,
                                      &createdLayout));
    layout = createdLayout;
    const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    1};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &size;
    VkDescriptorPool createdPool = VK_NULL_HANDLE;
    Check(vkCreateDescriptorPool(context.device, &poolInfo, nullptr,
                                 &createdPool));
    pool = createdPool;
    VkDescriptorSetAllocateInfo allocate{};
    allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate.descriptorPool = pool;
    allocate.descriptorSetCount = 1;
    allocate.pSetLayouts = &layout;
    Check(vkAllocateDescriptorSets(context.device, &allocate, &set));
    const VkDescriptorImageInfo sampled{
        sampler->handle, texture->view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &sampled;
    vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
}
} // namespace voryx::vulkan_backend

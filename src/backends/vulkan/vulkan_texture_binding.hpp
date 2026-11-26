#pragma once

#include "vulkan_sampled_texture.hpp"

namespace voryx::vulkan_backend
{
bool ValidSampler(const SamplerDesc &desc) noexcept;

class TextureSampler final : public rhi_detail::SamplerStorage
{
public:
    explicit TextureSampler(Context &context) noexcept;
    ~TextureSampler() override;
    TextureSampler(const TextureSampler &) = delete;
    TextureSampler &operator=(const TextureSampler &) = delete;
    void Initialize(const SamplerDesc &desc);
    VkSampler handle = VK_NULL_HANDLE;

private:
    Context &context;
};

class TextureBinding
{
public:
    TextureBinding(Context &context, std::shared_ptr<SampledTexture> texture,
                   std::shared_ptr<TextureSampler> sampler) noexcept;
    ~TextureBinding();
    TextureBinding(const TextureBinding &) = delete;
    TextureBinding &operator=(const TextureBinding &) = delete;
    void Initialize();
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;

private:
    Context &context;
    std::shared_ptr<SampledTexture> texture;
    std::shared_ptr<TextureSampler> sampler;
    VkDescriptorPool pool = VK_NULL_HANDLE;
};
} // namespace voryx::vulkan_backend

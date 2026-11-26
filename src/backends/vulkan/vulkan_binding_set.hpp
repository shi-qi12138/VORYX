#pragma once

#include "vulkan_texture_binding.hpp"
#include <binding_validation.hpp>

#include <variant>
#include <vector>

namespace voryx::vulkan_backend
{
using rhi_detail::maxBindingSlots;
bool ValidBindingLayout(const BindingLayoutDesc &desc) noexcept;

class UniformBuffer
{
public:
    explicit UniformBuffer(Context &context) noexcept;
    Result<void> Initialize(ByteSpan bytes);
    MappedBuffer buffer;
    VkDeviceSize size = 0;
};

class BindingLayout final : public rhi_detail::BindingLayoutStorage
{
public:
    BindingLayout(Context &context, const BindingLayoutDesc &desc);
    ~BindingLayout() override;
    BindingLayout(const BindingLayout &) = delete;
    BindingLayout &operator=(const BindingLayout &) = delete;
    void Initialize();
    const std::vector<BindingLayoutEntry> entries;
    VkDescriptorSetLayout handle = VK_NULL_HANDLE;

private:
    Context &context;
};

using BoundResource = std::variant<std::shared_ptr<UniformBuffer>,
                                   std::shared_ptr<SampledTexture>,
                                   std::shared_ptr<TextureSampler>>;

using NativeBinding =
    std::variant<VkDescriptorBufferInfo, VkDescriptorImageInfo>;

class BindingSet final : public rhi_detail::BindingSetStorage
{
public:
    BindingSet(Context &context, std::shared_ptr<BindingLayout> layout,
               std::span<const BoundResource> resources);
    BindingSet(Context &context, const BindingLayout &layout) noexcept;
    ~BindingSet() override;
    BindingSet(const BindingSet &) = delete;
    BindingSet &operator=(const BindingSet &) = delete;
    void Initialize();
    void Initialize(std::span<const NativeBinding> descriptors);
    [[nodiscard]] VkDescriptorSetLayout Layout() const noexcept;
    VkDescriptorSet handle = VK_NULL_HANDLE;

private:
    Context &context;
    const BindingLayout &layout;
    std::shared_ptr<BindingLayout> retainedLayout;
    std::vector<BoundResource> resources;
    VkDescriptorPool pool = VK_NULL_HANDLE;
};
} // namespace voryx::vulkan_backend

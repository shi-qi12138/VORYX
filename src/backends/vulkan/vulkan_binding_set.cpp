#include "vulkan_binding_set.hpp"

#include <array>
#include <exception>
#include <utility>

namespace voryx::vulkan_backend
{
namespace
{
VkDescriptorType NativeType(BindingType type) noexcept
{
    switch (type) {
        case BindingType::UniformBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case BindingType::SampledTexture:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case BindingType::Sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
    }
    std::terminate();
}

VkShaderStageFlags NativeVisibility(BindingVisibility visibility) noexcept
{
    const auto bits = static_cast<std::uint8_t>(visibility);
    VkShaderStageFlags flags = 0;
    if ((bits & static_cast<std::uint8_t>(BindingVisibility::Vertex)) != 0) {
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
    }
    if ((bits & static_cast<std::uint8_t>(BindingVisibility::Fragment)) != 0) {
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    return flags;
}
} // namespace

bool ValidBindingLayout(const BindingLayoutDesc &desc) noexcept
{
    return rhi_detail::ValidBindingLayout(desc);
}

UniformBuffer::UniformBuffer(Context &context) noexcept
    : buffer(context)
{
}

Result<void> UniformBuffer::Initialize(ByteSpan bytes)
{
    size = bytes.size();
    buffer.Initialize(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    return buffer.Write(0, bytes);
}

BindingLayout::BindingLayout(Context &context, const BindingLayoutDesc &desc)
    : entries(desc.entries.begin(), desc.entries.end())
    , context(context)
{
}

BindingLayout::~BindingLayout()
{
    if (handle != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context.device, handle, nullptr);
    }
}

void BindingLayout::Initialize()
{
    std::array<VkDescriptorSetLayoutBinding, maxBindingSlots> slots{};
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto &entry = entries[index];
        slots[index] = {entry.slot, NativeType(entry.type), 1,
                        NativeVisibility(entry.visibility), nullptr};
    }
    VkDescriptorSetLayoutCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    create.bindingCount = static_cast<std::uint32_t>(entries.size());
    create.pBindings = slots.data();
    VkDescriptorSetLayout created = VK_NULL_HANDLE;
    Check(vkCreateDescriptorSetLayout(context.device, &create, nullptr,
                                      &created));
    handle = created;
}

BindingSet::BindingSet(Context &context, std::shared_ptr<BindingLayout> layout,
                       std::span<const BoundResource> resources)
    : context(context)
    , layout(*layout)
    , retainedLayout(std::move(layout))
    , resources(resources.begin(), resources.end())
{
}

BindingSet::BindingSet(Context &context, const BindingLayout &layout) noexcept
    : context(context)
    , layout(layout)
{
}

BindingSet::~BindingSet()
{
    if (pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context.device, pool, nullptr);
    }
}

VkDescriptorSetLayout BindingSet::Layout() const noexcept
{
    return layout.handle;
}

void BindingSet::Initialize()
{
    std::array<NativeBinding, maxBindingSlots> native{};
    for (std::size_t index = 0; index < resources.size(); ++index) {
        const auto &resource = resources[index];
        if (const auto *buffer =
                std::get_if<std::shared_ptr<UniformBuffer>>(&resource)) {
            native[index] = VkDescriptorBufferInfo{(*buffer)->buffer.buffer, 0,
                                                   (*buffer)->size};
        } else if (const auto *texture =
                       std::get_if<std::shared_ptr<SampledTexture>>(
                           &resource)) {
            native[index] =
                VkDescriptorImageInfo{VK_NULL_HANDLE, (*texture)->view,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        } else {
            const auto &sampler =
                std::get<std::shared_ptr<TextureSampler>>(resource);
            native[index] = VkDescriptorImageInfo{
                sampler->handle, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
        }
    }
    Initialize(std::span(native).first(resources.size()));
}

void BindingSet::Initialize(std::span<const NativeBinding> descriptors)
{
    constexpr std::array types{BindingType::UniformBuffer,
                               BindingType::SampledTexture,
                               BindingType::Sampler};
    std::array<VkDescriptorPoolSize, types.size()> sizes{};
    std::uint32_t sizeCount = 0;
    for (const auto type : types) {
        std::uint32_t count = 0;
        for (const auto &entry : layout.entries) {
            if (entry.type == type) {
                ++count;
            }
        }
        if (count != 0) {
            sizes[sizeCount++] = {NativeType(type), count};
        }
    }
    VkDescriptorPoolCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    create.maxSets = 1;
    create.poolSizeCount = sizeCount;
    create.pPoolSizes = sizes.data();
    VkDescriptorPool created = VK_NULL_HANDLE;
    Check(vkCreateDescriptorPool(context.device, &create, nullptr, &created));
    pool = created;
    VkDescriptorSetAllocateInfo allocate{};
    allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate.descriptorPool = pool;
    allocate.descriptorSetCount = 1;
    allocate.pSetLayouts = &layout.handle;
    Check(vkAllocateDescriptorSets(context.device, &allocate, &handle));

    // 资源描述在 Vulkan 写入期间有效，集合在发布后保持不变。
    std::array<VkWriteDescriptorSet, maxBindingSlots> writes{};
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        const auto &entry = layout.entries[index];
        auto &write = writes[index];
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = handle;
        write.dstBinding = entry.slot;
        write.descriptorType = NativeType(entry.type);
        write.descriptorCount = 1;
        write.pBufferInfo =
            std::get_if<VkDescriptorBufferInfo>(&descriptors[index]);
        write.pImageInfo =
            std::get_if<VkDescriptorImageInfo>(&descriptors[index]);
    }
    vkUpdateDescriptorSets(context.device,
                           static_cast<std::uint32_t>(descriptors.size()),
                           writes.data(), 0, nullptr);
}
} // namespace voryx::vulkan_backend

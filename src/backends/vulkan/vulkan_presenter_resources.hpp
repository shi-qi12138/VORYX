#pragma once

#include "vulkan_binding_set.hpp"

#include <unordered_map>

namespace voryx::vulkan_backend
{
/// 只管理 Presenter 的着色器资源与不可变绑定。由 Presenter 线程调用。
class PresenterResources
{
public:
    explicit PresenterResources(Context &context);
    Result<Texture2DHandle> CreateTexture(const Rgba8TextureDesc &desc);
    Result<SamplerHandle> CreateSampler(const SamplerDesc &desc);
    Result<TextureBindingHandle> CreateBinding(const TextureBindingDesc &desc);
    Result<std::shared_ptr<TextureBinding>>
    GetBinding(TextureBindingHandle handle,
               std::string_view operation) const noexcept;
    Result<UniformBufferHandle> CreateUniformBuffer(ByteSpan bytes);
    Result<BindingLayoutHandle>
    CreateBindingLayout(const BindingLayoutDesc &desc);
    Result<BindingSetHandle> CreateBindingSet(const BindingSetDesc &desc);
    Result<std::shared_ptr<BindingSet>>
    GetBindingSet(BindingSetHandle handle,
                  std::string_view operation) const noexcept;
    Result<void> Destroy(UniformBufferHandle handle) noexcept;
    Result<void> Destroy(BindingLayoutHandle handle) noexcept;
    Result<void> Destroy(BindingSetHandle handle) noexcept;
    Result<void> Destroy(Texture2DHandle handle) noexcept;
    Result<void> Destroy(SamplerHandle handle) noexcept;
    Result<void> Destroy(TextureBindingHandle handle) noexcept;

private:
    template<typename T>
    using ResourceMap = std::unordered_map<std::uint64_t, std::shared_ptr<T>>;
    template<typename Handle, typename T>
    Result<Handle> Insert(ResourceMap<T> &map, std::shared_ptr<T> value,
                          std::string_view operation);
    template<typename Handle, typename T>
    Result<const std::shared_ptr<T> *>
    Find(const ResourceMap<T> &map, Handle handle,
         std::string_view operation) const noexcept;
    template<typename Handle, typename T>
    Result<void> Remove(ResourceMap<T> &map, Handle handle,
                        std::string_view operation) noexcept;

    template<typename Handle, typename T>
    Result<BoundResource>
    ResolveBindingResource(const ResourceMap<T> &map,
                           const BindingResource &resource,
                           std::string_view operation) const noexcept;

    Context &context;
    const std::uint64_t owner;
    std::uint64_t nextId = 1;
    ResourceMap<SampledTexture> textures;
    ResourceMap<TextureSampler> samplers;
    ResourceMap<UniformBuffer> uniforms;
    ResourceMap<BindingLayout> layouts;
    ResourceMap<TextureBinding> bindings;
    ResourceMap<BindingSet> bindingSets;
};
} // namespace voryx::vulkan_backend

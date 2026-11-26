#include "vulkan_presenter_resources.hpp"

#include <error_context.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <exception>
#include <limits>
#include <utility>

namespace voryx::detail
{
struct PresenterHandleAccess
{
    template<typename Handle>
    static Handle Make(std::uint64_t owner, std::uint64_t id) noexcept
    {
        Handle handle;
        handle.owner = owner;
        handle.id = id;
        return handle;
    }
    template<typename Handle>
    static std::uint64_t Owner(Handle handle) noexcept
    {
        return handle.owner;
    }
    template<typename Handle>
    static std::uint64_t Id(Handle handle) noexcept
    {
        return handle.id;
    }
};
} // namespace voryx::detail

namespace voryx::vulkan_backend
{
namespace
{
std::uint64_t AllocateOwner()
{
    // 只分配标识，不持有设备。已销毁 Presenter 的标识也不复用。
    static std::atomic<std::uint64_t> nextOwner{1};
    auto value = nextOwner.load(std::memory_order_relaxed);
    while (value != std::numeric_limits<std::uint64_t>::max()) {
        if (nextOwner.compare_exchange_weak(value, value + 1,
                                            std::memory_order_relaxed)) {
            return value;
        }
    }
    throw NativeFailure{VK_ERROR_TOO_MANY_OBJECTS};
}

template<typename T>
Result<T> Failure(ErrorCode code, std::string_view operation) noexcept
{
    return Result<T>::Failure(
        detail::WithOperation({code, {}, {}, {}}, operation));
}
} // namespace

PresenterResources::PresenterResources(Context &context)
    : context(context)
    , owner(AllocateOwner())
{
}

template<typename Handle, typename T>
Result<Handle> PresenterResources::Insert(ResourceMap<T> &map,
                                          std::shared_ptr<T> value,
                                          std::string_view operation)
{
    if (nextId == std::numeric_limits<std::uint64_t>::max()) {
        return Failure<Handle>(ErrorCode::OutOfMemory, operation);
    }
    map.emplace(nextId, std::move(value));
    return Result<Handle>::Success(
        detail::PresenterHandleAccess::Make<Handle>(owner, nextId++));
}

template<typename Handle, typename T>
Result<const std::shared_ptr<T> *>
PresenterResources::Find(const ResourceMap<T> &map, Handle handle,
                         std::string_view operation) const noexcept
{
    if (!handle.IsValid()) {
        return Failure<const std::shared_ptr<T> *>(ErrorCode::InvalidHandle,
                                                   operation);
    }
    if (detail::PresenterHandleAccess::Owner(handle) != owner) {
        return Failure<const std::shared_ptr<T> *>(ErrorCode::WrongDevice,
                                                   operation);
    }
    const auto entry = map.find(detail::PresenterHandleAccess::Id(handle));
    if (entry == map.end()) {
        return Failure<const std::shared_ptr<T> *>(ErrorCode::InvalidHandle,
                                                   operation);
    }
    return Result<const std::shared_ptr<T> *>::Success(&entry->second);
}

template<typename Handle, typename T>
Result<void> PresenterResources::Remove(ResourceMap<T> &map, Handle handle,
                                        std::string_view operation) noexcept
{
    auto found = Find(map, handle, operation);
    if (!found) {
        return Result<void>::Failure(std::move(found).GetError());
    }
    if (found.Value()->use_count() != 1) {
        return Failure<void>(ErrorCode::ResourceBusy, operation);
    }
    map.erase(detail::PresenterHandleAccess::Id(handle));
    return Result<void>::Success();
}

Result<Texture2DHandle>
PresenterResources::CreateTexture(const Rgba8TextureDesc &desc)
{
    constexpr std::string_view operation = "CreateTexture2D";
    if (!ValidTexture(desc)) {
        return Failure<Texture2DHandle>(ErrorCode::InvalidArgument, operation);
    }
    auto texture = std::make_shared<SampledTexture>(context);
    auto uploaded = texture->Initialize(desc);
    if (!uploaded) {
        return Result<Texture2DHandle>::Failure(std::move(uploaded).GetError());
    }
    return Insert<Texture2DHandle>(textures, std::move(texture), operation);
}

Result<SamplerHandle> PresenterResources::CreateSampler(const SamplerDesc &desc)
{
    constexpr std::string_view operation = "CreateSampler";
    if (!ValidSampler(desc)) {
        return Failure<SamplerHandle>(ErrorCode::InvalidArgument, operation);
    }
    auto sampler = std::make_shared<TextureSampler>(context);
    sampler->Initialize(desc);
    return Insert<SamplerHandle>(samplers, std::move(sampler), operation);
}

Result<TextureBindingHandle>
PresenterResources::CreateBinding(const TextureBindingDesc &desc)
{
    constexpr std::string_view operation = "CreateTextureBinding";
    auto texture = Find(textures, desc.texture, operation);
    if (!texture) {
        return Result<TextureBindingHandle>::Failure(
            std::move(texture).GetError());
    }
    auto sampler = Find(samplers, desc.sampler, operation);
    if (!sampler) {
        return Result<TextureBindingHandle>::Failure(
            std::move(sampler).GetError());
    }
    auto binding = std::make_shared<TextureBinding>(context, *texture.Value(),
                                                    *sampler.Value());
    binding->Initialize();
    return Insert<TextureBindingHandle>(bindings, std::move(binding),
                                        operation);
}

Result<std::shared_ptr<TextureBinding>>
PresenterResources::GetBinding(TextureBindingHandle handle,
                               std::string_view operation) const noexcept
{
    auto found = Find(bindings, handle, operation);
    if (!found) {
        return Result<std::shared_ptr<TextureBinding>>::Failure(
            std::move(found).GetError());
    }
    return Result<std::shared_ptr<TextureBinding>>::Success(*found.Value());
}

Result<void> PresenterResources::Destroy(Texture2DHandle handle) noexcept
{
    return Remove(textures, handle, "DestroyTexture2D");
}

Result<void> PresenterResources::Destroy(SamplerHandle handle) noexcept
{
    return Remove(samplers, handle, "DestroySampler");
}

Result<void> PresenterResources::Destroy(TextureBindingHandle handle) noexcept
{
    return Remove(bindings, handle, "DestroyTextureBinding");
}
Result<UniformBufferHandle>
PresenterResources::CreateUniformBuffer(ByteSpan bytes)
{
    constexpr std::string_view operation = "CreateUniformBuffer";
    if (bytes.empty()) {
        return Failure<UniformBufferHandle>(ErrorCode::InvalidArgument,
                                            operation);
    }
    if (bytes.size() > context.properties.limits.maxUniformBufferRange) {
        return Failure<UniformBufferHandle>(ErrorCode::Unsupported, operation);
    }
    auto uniform = std::make_shared<UniformBuffer>(context);
    auto uploaded = uniform->Initialize(bytes);
    if (!uploaded) {
        return Result<UniformBufferHandle>::Failure(
            std::move(uploaded).GetError());
    }
    return Insert<UniformBufferHandle>(uniforms, std::move(uniform), operation);
}

Result<BindingLayoutHandle>
PresenterResources::CreateBindingLayout(const BindingLayoutDesc &desc)
{
    constexpr std::string_view operation = "CreateBindingLayout";
    if (!ValidBindingLayout(desc)) {
        return Failure<BindingLayoutHandle>(ErrorCode::InvalidArgument,
                                            operation);
    }
    auto layout = std::make_shared<BindingLayout>(context, desc);
    layout->Initialize();
    return Insert<BindingLayoutHandle>(layouts, std::move(layout), operation);
}

template<typename Handle, typename T>
Result<BoundResource> PresenterResources::ResolveBindingResource(
    const ResourceMap<T> &map, const BindingResource &resource,
    std::string_view operation) const noexcept
{
    const auto *handle = std::get_if<Handle>(&resource);
    if (handle == nullptr) {
        return Failure<BoundResource>(ErrorCode::InvalidArgument, operation);
    }
    auto found = Find(map, *handle, operation);
    if (!found) {
        return Result<BoundResource>::Failure(std::move(found).GetError());
    }
    return Result<BoundResource>::Success(BoundResource{*found.Value()});
}

Result<BindingSetHandle>
PresenterResources::CreateBindingSet(const BindingSetDesc &desc)
{
    constexpr std::string_view operation = "CreateBindingSet";
    auto layout = Find(layouts, desc.layout, operation);
    if (!layout) {
        return Result<BindingSetHandle>::Failure(std::move(layout).GetError());
    }
    const auto &entries = (*layout.Value())->entries;
    if (desc.entries.size() != entries.size()) {
        return Failure<BindingSetHandle>(ErrorCode::InvalidArgument, operation);
    }
    std::array<BoundResource, maxBindingSlots> values{};
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto &expected = entries[index];
        const auto supplied = std::ranges::find(desc.entries, expected.slot,
                                                &BindingSetEntry::slot);
        if (supplied == desc.entries.end()) {
            return Failure<BindingSetHandle>(ErrorCode::InvalidArgument,
                                             operation);
        }
        auto resolved = [&]() {
            switch (expected.type) {
                case BindingType::UniformBuffer:
                    return ResolveBindingResource<UniformBufferHandle>(
                        uniforms, supplied->resource, operation);
                case BindingType::SampledTexture:
                    return ResolveBindingResource<Texture2DHandle>(
                        textures, supplied->resource, operation);
                case BindingType::Sampler:
                    return ResolveBindingResource<SamplerHandle>(
                        samplers, supplied->resource, operation);
            }
            std::terminate();
        }();
        if (!resolved) {
            return Result<BindingSetHandle>::Failure(
                std::move(resolved).GetError());
        }
        values[index] = std::move(resolved).Value();
    }
    auto set = std::make_shared<BindingSet>(
        context, *layout.Value(), std::span(values).first(entries.size()));
    set->Initialize();
    return Insert<BindingSetHandle>(bindingSets, std::move(set), operation);
}

Result<std::shared_ptr<BindingSet>>
PresenterResources::GetBindingSet(BindingSetHandle handle,
                                  std::string_view operation) const noexcept
{
    auto found = Find(bindingSets, handle, operation);
    if (!found) {
        return Result<std::shared_ptr<BindingSet>>::Failure(
            std::move(found).GetError());
    }
    return Result<std::shared_ptr<BindingSet>>::Success(*found.Value());
}

Result<void> PresenterResources::Destroy(UniformBufferHandle handle) noexcept
{
    return Remove(uniforms, handle, "DestroyUniformBuffer");
}

Result<void> PresenterResources::Destroy(BindingLayoutHandle handle) noexcept
{
    return Remove(layouts, handle, "DestroyBindingLayout");
}

Result<void> PresenterResources::Destroy(BindingSetHandle handle) noexcept
{
    return Remove(bindingSets, handle, "DestroyBindingSet");
}
} // namespace voryx::vulkan_backend

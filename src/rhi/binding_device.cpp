#include "binding_validation.hpp"
#include "buffer_device.hpp"
#include "handle_access.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <type_traits>

namespace voryx::rhi_detail
{
namespace
{
using Access = detail::RhiHandleAccess;

template<typename Handle, typename T>
Result<std::shared_ptr<T>> Find(const State &state, const BindingMap<T> &map,
                                Handle handle) noexcept
{
    using Value = std::shared_ptr<T>;
    if (!handle.IsValid()) {
        return Fail<Value>(ErrorCode::InvalidHandle);
    }
    if (Access::Owner(handle) != state.identity) {
        return Fail<Value>(ErrorCode::WrongDevice);
    }
    const auto found = map.find(Access::Id(handle));
    if (found == map.end()) {
        return Fail<Value>(ErrorCode::InvalidHandle);
    }
    return Result<Value>::Success(found->second);
}

template<typename Handle, typename T>
Result<void> Remove(State &state, BindingMap<T> &map, Handle handle,
                    std::string_view operation) noexcept
{
    return Invoke<void>(operation, [&]() {
        if (const auto error = state.CheckAccess(DeviceAccess::Cleanup)) {
            return Fail<void>(*error);
        }
        const auto found = Find(state, map, handle);
        if (!found) {
            return Fail<void>(found.GetError().errorCode);
        }
        // 设备表和本次查找各持有一份。GPU 对集合的引用可以延迟释放。
        std::size_t retained = 2;
        if constexpr (std::is_same_v<T, BindingSet>) {
            retained += found.Value()->inFlightRefs;
        }
        if (static_cast<std::size_t>(found.Value().use_count()) > retained) {
            return Fail<void>(ErrorCode::ResourceBusy);
        }
        map.erase(Access::Id(handle));
        return Result<void>::Success();
    });
}

bool Full(const State &state, std::size_t size, std::uint32_t limit) noexcept
{
    return size >= limit
           || state.nextBindingId == std::numeric_limits<std::uint64_t>::max();
}

template<typename Handle, typename T>
Result<Handle> Insert(State &state, BindingMap<T> &map,
                      std::shared_ptr<T> value)
{
    map.emplace(state.nextBindingId, std::move(value));
    return Result<Handle>::Success(
        Access::Binding<Handle>(state.identity, state.nextBindingId++));
}

template<typename Handle, typename T>
Result<Handle> NativeFailure(State &state, Result<T> result)
{
    if (result.GetError().errorCode == ErrorCode::DeviceLost) {
        state.lost = true;
    }
    return Result<Handle>::Failure(std::move(result).GetError());
}

Result<BackendBinding> Resolve(State &state, BindingSet &set, BindingType type,
                               const DeviceBindingResource &input)
{
    if (type == BindingType::UniformBuffer) {
        const auto *range = std::get_if<UniformBufferBinding>(&input);
        if (!range) {
            return Fail<BackendBinding>(ErrorCode::InvalidArgument);
        }
        auto found = state.Lookup(range->buffer);
        if (!found) {
            return Fail<BackendBinding>(found.GetError().errorCode);
        }
        const auto &caps = state.capabilities;
        if (!HasUsage(found.Value()->usage, BufferUsage::Constant)
            || range->sizeBytes == 0
            || range->offsetBytes % caps.uniformBufferOffsetAlignmentBytes != 0
            || !InRange(range->offsetBytes, range->sizeBytes,
                        found.Value()->sizeBytes)) {
            return Fail<BackendBinding>(ErrorCode::InvalidArgument);
        }
        if (range->sizeBytes > caps.maxUniformBufferRangeBytes) {
            return Fail<BackendBinding>(ErrorCode::Unsupported);
        }
        auto *storage = found.Value()->storage.get();
        set.buffers.push_back(std::move(found).Value());
        return Result<BackendBinding>::Success(BackendUniformBinding{
            storage, range->offsetBytes, range->sizeBytes});
    }
    if (type == BindingType::SampledTexture) {
        const auto *handle = std::get_if<TextureHandle>(&input);
        if (!handle) {
            return Fail<BackendBinding>(ErrorCode::InvalidArgument);
        }
        auto found = state.Lookup(*handle);
        if (!found) {
            return Fail<BackendBinding>(found.GetError().errorCode);
        }
        if (!HasUsage(found.Value()->usage, TextureUsage::Sampled)) {
            return Fail<BackendBinding>(ErrorCode::InvalidArgument);
        }
        auto *storage = found.Value()->storage.get();
        set.textures.push_back(std::move(found).Value());
        return Result<BackendBinding>::Success(storage);
    }
    const auto *handle = std::get_if<DeviceSamplerHandle>(&input);
    if (!handle) {
        return Fail<BackendBinding>(ErrorCode::InvalidArgument);
    }
    auto found = Find(state, state.samplers, *handle);
    if (!found) {
        return Fail<BackendBinding>(found.GetError().errorCode);
    }
    auto *storage = found.Value().get();
    set.samplers.push_back(std::move(found).Value());
    return Result<BackendBinding>::Success(storage);
}
} // namespace

Result<std::shared_ptr<BindingLayout>>
State::Lookup(DeviceBindingLayoutHandle handle) const
{
    return Find(*this, bindingLayouts, handle);
}

Result<std::shared_ptr<BindingSet>>
State::Lookup(DeviceBindingSetHandle handle) const
{
    return Find(*this, bindingSets, handle);
}

Result<DeviceSamplerHandle>
BufferDevice::CreateSampler(const SamplerDesc &desc) noexcept
{
    return Invoke<DeviceSamplerHandle>("CreateSampler", [&]() {
        if (const auto error = state->CheckAccess()) {
            return Fail<DeviceSamplerHandle>(*error);
        }
        if (!ValidSampler(desc)) {
            return Fail<DeviceSamplerHandle>(ErrorCode::InvalidArgument);
        }
        const auto limit = state->capabilities.maxSamplers;
        if (limit == 0) {
            return Fail<DeviceSamplerHandle>(ErrorCode::Unsupported);
        }
        if (Full(*state, state->samplers.size(), limit)) {
            return Fail<DeviceSamplerHandle>(ErrorCode::OutOfMemory);
        }
        auto created = state->backend->CreateSampler(desc);
        if (!created) {
            return NativeFailure<DeviceSamplerHandle>(*state,
                                                      std::move(created));
        }
        std::shared_ptr<SamplerStorage> sampler = std::move(created).Value();
        return Insert<DeviceSamplerHandle>(*state, state->samplers,
                                           std::move(sampler));
    });
}

Result<DeviceBindingLayoutHandle>
BufferDevice::CreateBindingLayout(const BindingLayoutDesc &desc) noexcept
{
    return Invoke<DeviceBindingLayoutHandle>("CreateBindingLayout", [&]() {
        if (const auto error = state->CheckAccess()) {
            return Fail<DeviceBindingLayoutHandle>(*error);
        }
        if (!ValidBindingLayout(desc)) {
            return Fail<DeviceBindingLayoutHandle>(ErrorCode::InvalidArgument);
        }
        const auto limit = state->capabilities.maxBindingLayouts;
        if (limit == 0) {
            return Fail<DeviceBindingLayoutHandle>(ErrorCode::Unsupported);
        }
        if (Full(*state, state->bindingLayouts.size(), limit)) {
            return Fail<DeviceBindingLayoutHandle>(ErrorCode::OutOfMemory);
        }
        auto layout = std::make_shared<BindingLayout>();
        layout->entries.assign(desc.entries.begin(), desc.entries.end());
        auto created = state->backend->CreateBindingLayout({layout->entries});
        if (!created) {
            return NativeFailure<DeviceBindingLayoutHandle>(*state,
                                                            std::move(created));
        }
        layout->storage = std::move(created).Value();
        return Insert<DeviceBindingLayoutHandle>(*state, state->bindingLayouts,
                                                 std::move(layout));
    });
}

Result<DeviceBindingSetHandle>
BufferDevice::CreateBindingSet(const DeviceBindingSetDesc &desc) noexcept
{
    return Invoke<DeviceBindingSetHandle>("CreateBindingSet", [&]() {
        if (const auto error = state->CheckAccess()) {
            return Fail<DeviceBindingSetHandle>(*error);
        }
        auto layout = state->Lookup(desc.layout);
        if (!layout) {
            return Fail<DeviceBindingSetHandle>(layout.GetError().errorCode);
        }
        if (desc.entries.size() != layout.Value()->entries.size()) {
            return Fail<DeviceBindingSetHandle>(ErrorCode::InvalidArgument);
        }
        if (Full(*state, state->bindingSets.size(),
                 state->capabilities.maxBindingSets)) {
            return Fail<DeviceBindingSetHandle>(ErrorCode::OutOfMemory);
        }
        auto set = std::make_shared<BindingSet>();
        set->layout = std::move(layout).Value();
        std::array<BackendBinding, maxBindingSlots> resources{};
        std::size_t index = 0;
        for (const auto &entry : set->layout->entries) {
            const auto input = std::ranges::find(desc.entries, entry.slot,
                                                 &DeviceBindingSetEntry::slot);
            if (input == desc.entries.end()) {
                return Fail<DeviceBindingSetHandle>(ErrorCode::InvalidArgument);
            }
            auto resource = Resolve(*state, *set, entry.type, input->resource);
            if (!resource) {
                return Fail<DeviceBindingSetHandle>(
                    resource.GetError().errorCode);
            }
            resources[index++] = resource.Value();
        }
        auto created = state->backend->CreateBindingSet(
            *set->layout->storage, std::span(resources).first(index));
        if (!created) {
            return NativeFailure<DeviceBindingSetHandle>(*state,
                                                         std::move(created));
        }
        set->storage = std::move(created).Value();
        return Insert<DeviceBindingSetHandle>(*state, state->bindingSets,
                                              std::move(set));
    });
}

Result<void> BufferDevice::Destroy(DeviceSamplerHandle handle) noexcept
{
    return Remove(*state, state->samplers, handle, "DestroySampler");
}
Result<void> BufferDevice::Destroy(DeviceBindingLayoutHandle handle) noexcept
{
    return Remove(*state, state->bindingLayouts, handle,
                  "DestroyBindingLayout");
}
Result<void> BufferDevice::Destroy(DeviceBindingSetHandle handle) noexcept
{
    return Remove(*state, state->bindingSets, handle, "DestroyBindingSet");
}
} // namespace voryx::rhi_detail

#pragma once

#include <voryx/rhi/texture.hpp>

#include <cstdint>
#include <span>
#include <variant>

namespace voryx
{
namespace detail
{
struct PresenterHandleAccess;
struct Texture2DTag;
struct SamplerTag;
struct TextureBindingTag;
struct UniformBufferTag;
struct BindingLayoutTag;
struct BindingSetTag;
} // namespace detail

template<typename Tag>
class PresenterResourceHandle
{
public:
    /// 仅检查结构，实际生命周期和所属 Presenter 由接口校验。
    [[nodiscard]] bool IsValid() const noexcept
    {
        return owner != 0 && id != 0;
    }
    bool operator==(const PresenterResourceHandle &) const = default;

private:
    friend struct detail::PresenterHandleAccess;
    std::uint64_t owner = 0;
    std::uint64_t id = 0;
};

using Texture2DHandle = PresenterResourceHandle<detail::Texture2DTag>;
using SamplerHandle = PresenterResourceHandle<detail::SamplerTag>;
using TextureBindingHandle = PresenterResourceHandle<detail::TextureBindingTag>;

using UniformBufferHandle = PresenterResourceHandle<detail::UniformBufferTag>;
using BindingLayoutHandle = PresenterResourceHandle<detail::BindingLayoutTag>;
using BindingSetHandle = PresenterResourceHandle<detail::BindingSetTag>;

enum class TextureFilter : std::uint8_t
{
    Nearest,
    Linear,
};

enum class TextureAddressMode : std::uint8_t
{
    ClampToEdge,
    Repeat,
};

struct SamplerDesc
{
    TextureFilter minFilter = TextureFilter::Linear;
    TextureFilter magFilter = TextureFilter::Linear;
    TextureAddressMode addressU = TextureAddressMode::ClampToEdge;
    TextureAddressMode addressV = TextureAddressMode::ClampToEdge;
};

struct TextureBindingDesc
{
    Texture2DHandle texture;
    SamplerHandle sampler;
};

enum class BindingType : std::uint8_t
{
    UniformBuffer,
    SampledTexture,
    Sampler,
};

enum class BindingVisibility : std::uint8_t
{
    Vertex = 1,
    Fragment = 2,
};

[[nodiscard]] constexpr BindingVisibility
operator|(BindingVisibility left, BindingVisibility right) noexcept
{
    return static_cast<BindingVisibility>(static_cast<std::uint8_t>(left)
                                          | static_cast<std::uint8_t>(right));
}

/// 描述第 0 个集合中的单个槽位，槽号范围为 0 至 31。
struct BindingLayoutEntry
{
    std::uint32_t slot = 0;
    BindingType type = BindingType::UniformBuffer;
    BindingVisibility visibility = BindingVisibility::Vertex;
};

/// 布局包含 1 至 8 个互不重复的槽位，创建时复制条目。
struct BindingLayoutDesc
{
    std::span<const BindingLayoutEntry> entries;
};

using BindingResource =
    std::variant<UniformBufferHandle, Texture2DHandle, SamplerHandle>;

struct BindingSetEntry
{
    std::uint32_t slot = 0;
    BindingResource resource;
};

/// 条目应与布局逐槽对应，顺序任意，创建后集合不可更新。
struct BindingSetDesc
{
    BindingLayoutHandle layout;
    std::span<const BindingSetEntry> entries;
};

namespace detail
{
struct RhiHandleAccess;
struct DeviceSamplerTag;
struct DeviceBindingLayoutTag;
struct DeviceBindingSetTag;
} // namespace detail

/// 句柄归属创建设备，IsValid 只检查结构。
template<typename Tag>
class DeviceBindingHandle
{
public:
    [[nodiscard]] bool IsValid() const noexcept
    {
        return owner != 0 && id != 0;
    }
    bool operator==(const DeviceBindingHandle &) const = default;

private:
    friend struct detail::RhiHandleAccess;
    std::uint64_t owner = 0;
    std::uint64_t id = 0;
};

using DeviceSamplerHandle = DeviceBindingHandle<detail::DeviceSamplerTag>;
using DeviceBindingLayoutHandle =
    DeviceBindingHandle<detail::DeviceBindingLayoutTag>;
using DeviceBindingSetHandle = DeviceBindingHandle<detail::DeviceBindingSetTag>;

struct UniformBufferBinding
{
    BufferHandle buffer;
    std::uint64_t offsetBytes = 0;
    std::uint64_t sizeBytes = 0;
};

using DeviceBindingResource =
    std::variant<UniformBufferBinding, TextureHandle, DeviceSamplerHandle>;

struct DeviceBindingSetEntry
{
    std::uint32_t slot = 0;
    DeviceBindingResource resource;
};

struct DeviceBindingSetDesc
{
    DeviceBindingLayoutHandle layout;
    std::span<const DeviceBindingSetEntry> entries;
};
} // namespace voryx

#pragma once

#include <voryx/core/result.hpp>
#include <voryx/rhi/binding.hpp>
#include <voryx/rhi/frame_presenter.hpp>
#include <voryx/rhi/types.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace voryx
{
struct ColorVertex
{
    float x;
    float y;
    float red;
    float green;
    float blue;
};

struct TriangleListDesc
{
    std::span<const ColorVertex> vertices;
    std::span<const std::uint32_t> vertexSpirv;
    std::span<const std::uint32_t> fragmentSpirv;
};

struct TexturedVertex
{
    float x;
    float y;
    float u;
    float v;
};

struct TexturedTriangleListDesc
{
    std::span<const TexturedVertex> vertices;
    Rgba8TextureDesc texture;
    std::span<const std::uint32_t> vertexSpirv;
    std::span<const std::uint32_t> fragmentSpirv;
};

struct BoundTexturedTriangleListDesc
{
    std::span<const TexturedVertex> vertices;
    TextureBindingHandle binding;
    std::span<const std::uint32_t> vertexSpirv;
    std::span<const std::uint32_t> fragmentSpirv;
};

struct ResourceTriangleListDesc
{
    std::span<const TexturedVertex> vertices;
    BindingSetHandle bindings;
    std::span<const std::uint32_t> vertexSpirv;
    std::span<const std::uint32_t> fragmentSpirv;
};

struct DepthVertex
{
    float x;
    float y;
    float z;
    float red;
    float green;
    float blue;
};

struct DepthTriangleListDesc
{
    std::span<const DepthVertex> vertices;
    std::span<const std::uint32_t> vertexSpirv;
    std::span<const std::uint32_t> fragmentSpirv;
};

struct IndexedTriangleListDesc
{
    std::span<const ColorVertex> vertices;
    std::span<const std::uint32_t> indices;
    std::span<const std::uint32_t> vertexSpirv;
    std::span<const std::uint32_t> fragmentSpirv;
};

/// Vulkan 固定布局示例入口。通用应用可直接使用 IFramePresenter。
class IPresenter : public IFramePresenter
{
public:
    ~IPresenter() override = default;
    IPresenter(const IPresenter &) = delete;
    IPresenter &operator=(const IPresenter &) = delete;

    /// 在创建线程调用。窗口暂停时返回 SurfaceUnavailable，可在恢复后重试。
    virtual Result<void> ClearAndPresent(ClearColor color) noexcept = 0;

    /// 复制顶点并创建固定布局的三角形管线。失败时保留上一组有效数据。
    virtual Result<void>
    SetTriangleList(const TriangleListDesc &desc) noexcept = 0;

    /// 清屏、执行当前绘制配置，再呈现。
    virtual Result<void> DrawAndPresent(ClearColor background) noexcept = 0;

    /// 同步上传一张 RGBA8 纹理并绑定到片元 Shader 的 set 0 / binding 0。
    /// 使用最近邻和边缘钳位。成功后 DrawAndPresent 绘制这一组顶点。
    virtual Result<void>
    SetTexturedTriangleList(const TexturedTriangleListDesc &desc) noexcept = 0;

    /// 配置带深度的彩色三角形，z 在 [0, 1] 内。每帧深度清为 1，较小值胜出。
    virtual Result<void>
    SetDepthTriangleList(const DepthTriangleListDesc &desc) noexcept = 0;

    /// 复制顶点与索引。索引数须为 3 的倍数，且每项都须引用有效顶点。
    virtual Result<void>
    SetIndexedTriangleList(const IndexedTriangleListDesc &desc) noexcept = 0;

    /// 创建不可变的 RGBA8 纹理。返回后不再借用像素。
    virtual Result<Texture2DHandle>
    CreateTexture2D(const Rgba8TextureDesc &desc) noexcept = 0;
    virtual Result<SamplerHandle>
    CreateSampler(const SamplerDesc &desc) noexcept = 0;
    /// 创建不可变组合绑定，持有纹理和采样器，供片元 Shader 的 set 0 / binding 0
    /// 使用。
    virtual Result<TextureBindingHandle>
    CreateTextureBinding(const TextureBindingDesc &desc) noexcept = 0;
    /// 复用已有纹理绑定，只创建本次配置的几何和管线。
    virtual Result<void> SetBoundTexturedTriangleList(
        const BoundTexturedTriangleListDesc &desc) noexcept = 0;

    /// 停用当前绘制配置并释放它所持有的绑定。
    virtual Result<void> ResetDraw() noexcept = 0;
    /// 有依赖时返回 ResourceBusy。成功后句柄立即失效。
    virtual Result<void> Destroy(TextureBindingHandle handle) noexcept = 0;
    virtual Result<void> Destroy(Texture2DHandle handle) noexcept = 0;
    virtual Result<void> Destroy(SamplerHandle handle) noexcept = 0;

    /// 创建不可变常量缓冲，按完整字节范围绑定，返回后不再借用输入。
    virtual Result<UniformBufferHandle>
    CreateUniformBuffer(std::span<const std::byte> bytes) noexcept = 0;
    /// 定义 set 0 的槽位类型和 Shader 可见性。布局和集合创建后不可变。
    virtual Result<BindingLayoutHandle>
    CreateBindingLayout(const BindingLayoutDesc &desc) noexcept = 0;
    virtual Result<BindingSetHandle>
    CreateBindingSet(const BindingSetDesc &desc) noexcept = 0;
    /// 使用现有绑定集合与 vec2 位置/vec2 UV 顶点配置绘制。
    virtual Result<void>
    SetResourceTriangleList(const ResourceTriangleListDesc &desc) noexcept = 0;
    virtual Result<void> Destroy(BindingSetHandle handle) noexcept = 0;
    virtual Result<void> Destroy(BindingLayoutHandle handle) noexcept = 0;
    virtual Result<void> Destroy(UniformBufferHandle handle) noexcept = 0;

protected:
    IPresenter() = default;
};

} // namespace voryx

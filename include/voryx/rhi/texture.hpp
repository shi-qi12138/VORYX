#pragma once
#include <voryx/rhi/types.hpp>
namespace voryx
{
class TextureHandle
{
public:
    [[nodiscard]] bool IsValid() const noexcept
    {
        return device != 0 && generation != 0;
    }
    bool operator==(const TextureHandle &) const = default;

private:
    friend struct detail::RhiHandleAccess;
    std::uint64_t device = 0;
    std::uint64_t generation = 0;
    std::uint32_t slot = 0;
};
/// 颜色图像由窗口持有，有效期截止本帧呈现或取消。
struct FrameInfo
{
    FrameToken token;
    TextureHandle color;
    Extent2D extent;
    Format colorFormat = Format::Auto;
};

enum class TextureUsage : std::uint32_t
{
    None = 0,
    Sampled = 1,
    ColorAttachment = 2,
    DepthAttachment = 4,
    CopySrc = 8,
    CopyDst = 16,
};
[[nodiscard]] constexpr TextureUsage operator|(TextureUsage left,
                                               TextureUsage right) noexcept
{
    return static_cast<TextureUsage>(static_cast<std::uint32_t>(left)
                                     | static_cast<std::uint32_t>(right));
}
enum class TextureUse : std::uint8_t
{
    Initial,
    ColorAttachment,
    DepthWrite,
    ShaderRead,
    CopySrc,
    CopyDst,
    Present
};
/// 首批支持 RGBA8_UNorm 颜色附件与 D32_Float 深度附件，单 mip、单层、单采样。
struct TextureDesc
{
    Extent2D extent;
    Format format = Format::Auto;
    TextureUsage usage = TextureUsage::None;
    std::string_view debugName;
};
struct TextureBarrier
{
    TextureHandle texture;
    TextureUse beforeUse = TextureUse::Initial;
    TextureUse afterUse = TextureUse::Initial;
};
/// 首批附件始终清除并保存，深度清除值必须在 [0,1] 内。
struct RenderPassDesc
{
    TextureHandle color;
    ClearColor clearColor;
    TextureHandle depth;
    float clearDepth = 1;
};
struct Viewport
{
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
    float minDepth = 0;
    float maxDepth = 1;
};
struct Rect2D
{
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};
enum class IndexType : std::uint8_t
{
    UInt16,
    UInt32
};
struct Rgba8TextureDesc
{
    Extent2D extent;
    /// 0 表示紧密排列。像素自左向右、由上至下，通道按 RGBA 顺序。
    std::size_t rowPitchBytes = 0;
    std::span<const std::byte> pixels;
};

} // namespace voryx

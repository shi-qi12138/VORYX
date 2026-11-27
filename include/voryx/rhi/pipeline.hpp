#pragma once

#include <voryx/rhi/binding.hpp>

namespace voryx
{
class PipelineHandle
{
public:
    /// 仅检查结构，设备操作同时检查归属与代数。
    [[nodiscard]] bool IsValid() const noexcept
    {
        return device != 0 && generation != 0;
    }
    bool operator==(const PipelineHandle &) const = default;

private:
    friend struct detail::RhiHandleAccess;
    std::uint64_t device = 0;
    std::uint64_t generation = 0;
    std::uint32_t slot = 0;
};

enum class VertexFormat : std::uint8_t
{
    Float2,
    Float3,
    Float4
};
using PipelineColorFormat = Format;

enum class CullMode : std::uint8_t
{
    None,
    Front,
    Back
};
enum class FrontFace : std::uint8_t
{
    CounterClockwise,
    Clockwise
};
enum class BlendMode : std::uint8_t
{
    Opaque,
    Alpha,
    PremultipliedAlpha
};
enum class DepthCompare : std::uint8_t
{
    Less,
    LessEqual,
    Always
};

struct VertexAttribute
{
    std::uint32_t location = 0;
    VertexFormat format = VertexFormat::Float2;
    std::uint32_t offsetBytes = 0;
};

/// 首批管线使用单顶点流、TriangleList、main 入口、单颜色附件与单采样。
/// 按后端提供 SPIR-V 或 GLSL 阶段对。有资源时必须指定兼容布局。
struct GraphicsPipelineDesc
{
    std::span<const std::uint32_t> vertexSpirv;
    std::span<const std::uint32_t> fragmentSpirv;
    std::uint32_t vertexStrideBytes = 0;
    std::span<const VertexAttribute> vertexAttributes;
    PipelineColorFormat colorFormat = PipelineColorFormat::RGBA8_UNorm;
    CullMode cullMode = CullMode::None;
    FrontFace frontFace = FrontFace::CounterClockwise;
    BlendMode blendMode = BlendMode::Opaque;
    bool depthTest = false;
    bool depthWrite = false;
    DepthCompare depthCompare = DepthCompare::LessEqual;
    std::string_view debugName;
    DeviceBindingLayoutHandle bindingLayout{};
    /// OpenGL GLSL 源码，可与 SPIR-V 同时提供。源码在创建调用返回前消费。
    std::string_view vertexGlsl{};
    std::string_view fragmentGlsl{};
};
} // namespace voryx

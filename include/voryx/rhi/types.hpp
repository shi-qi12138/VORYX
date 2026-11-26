#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <voryx/core/log.hpp>

namespace voryx
{
namespace detail
{
struct RhiHandleAccess;
}

struct ClearColor
{
    float red = 0;
    float green = 0;
    float blue = 0;
    float alpha = 1;
};
struct Extent2D
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};
enum class Format : std::uint8_t
{
    RGBA8_UNorm,
    RGBA8_sRGB,
    BGRA8_UNorm,
    D32_Float,
    Auto = 255,
};

enum class BackendType : std::uint8_t
{
    Auto,
    Null,
    OpenGL,
    Vulkan,
    DX12,
};

class BufferHandle
{
public:
    /// 仅判断结构有效性，不保证资源仍存活；操作时由设备检查归属及代数。
    [[nodiscard]] bool IsValid() const noexcept
    {
        return device != 0 && generation != 0;
    }
    bool operator==(const BufferHandle &) const = default;

private:
    friend struct detail::RhiHandleAccess;
    std::uint64_t device = 0;
    std::uint64_t generation = 0;
    std::uint32_t slot = 0;
};

/// 一次窗口图像获取的令牌，消费后即失效。
class FrameToken
{
public:
    [[nodiscard]] bool IsValid() const noexcept
    {
        return device != 0 && serial != 0;
    }
    bool operator==(const FrameToken &) const = default;

private:
    friend struct detail::RhiHandleAccess;
    std::uint64_t device = 0;
    std::uint64_t serial = 0;
};

class SubmissionToken
{
public:
    [[nodiscard]] bool IsValid() const noexcept
    {
        return device != 0 && serial != 0;
    }
    bool operator==(const SubmissionToken &) const = default;

private:
    friend struct detail::RhiHandleAccess;
    std::uint64_t device = 0;
    std::uint64_t serial = 0;
};

enum class BufferUsage : std::uint32_t
{
    None = 0,
    Vertex = 1,
    Index = 2,
    Constant = 4,
    CopySrc = 8,
    CopyDst = 16,
};

[[nodiscard]] constexpr BufferUsage operator|(BufferUsage left,
                                              BufferUsage right) noexcept
{
    return static_cast<BufferUsage>(static_cast<std::uint32_t>(left)
                                    | static_cast<std::uint32_t>(right));
}

enum class BufferUse : std::uint8_t
{
    Initial,
    VertexRead,
    IndexRead,
    ConstantRead,
    CopySrc,
    CopyDst,
};

enum class CommandState : std::uint8_t
{
    Initial,
    Recording,
    Executable,
    Pending,
    Completed,
};

struct DeviceDesc
{
    BackendType backend = BackendType::Auto;
    std::string_view debugName;
    bool enableValidation = false;
    bool allowSoftwareAdapter = false;
    LogCallback logCallback = nullptr;
    void *logUserData = nullptr;
};

struct DeviceInfo
{
    BackendType backend = BackendType::Null;
    std::string name;
    std::string debugName;
    bool softwareAdapter = false;
    bool validationEnabled = false;
};

struct DeviceCapabilities
{
    // 实际限制由后端填充，调用方必须查询，不依赖 Null 的固定上限。
    std::uint64_t maxBufferSizeBytes = 0;
    std::uint32_t maxBuffers = 0;
    std::uint32_t maxCommandsPerList = 0;
    std::uint32_t maxCommandListsPerSubmit = 0;
    std::uint32_t maxGraphicsPipelines = 0;
    std::uint32_t maxVertexAttributes = 0;
    std::uint32_t maxVertexStrideBytes = 0;
    std::uint32_t maxTextures = 0;
    std::uint32_t maxTexture2DSize = 0;
    std::uint64_t maxUniformBufferRangeBytes = 0;
    std::uint64_t uniformBufferOffsetAlignmentBytes = 1;
    std::uint32_t maxSamplers = 0;
    std::uint32_t maxBindingLayouts = 0;
    std::uint32_t maxBindingSets = 0;
    /// 0 表示设备不提供异步提交。
    std::uint32_t maxPendingSubmissions = 0;
    /// 窗口异步呈现的帧槽上限。0 表示设备不提供该能力。
    std::uint32_t maxPendingWindowFrames = 0;
};

using ByteSpan = std::span<const std::byte>;
using MutableByteSpan = std::span<std::byte>;
using TimeoutNs = std::uint64_t;

struct BufferDesc
{
    std::uint64_t sizeBytes = 0;
    BufferUsage usage = BufferUsage::None;
    std::string_view debugName;
};

struct BufferBarrier
{
    /// 显式提供前后用途，避免未初始化的状态或误用默认转换。
    BufferBarrier(BufferHandle buffer, BufferUse beforeUse,
                  BufferUse afterUse) noexcept
        : buffer(buffer)
        , beforeUse(beforeUse)
        , afterUse(afterUse)
    {
    }
    BufferHandle buffer;
    BufferUse beforeUse;
    BufferUse afterUse;
};

struct BufferCopyDesc
{
    BufferHandle src;
    BufferHandle dst;
    std::uint64_t srcOffsetBytes = 0;
    std::uint64_t dstOffsetBytes = 0;
    std::uint64_t sizeBytes = 0;
};
} // namespace voryx

#pragma once

#include <voryx/core/result.hpp>
#include <voryx/rhi/device.hpp>

#include <cstdint>
#include <memory>
#include <span>

namespace voryx::rhi_detail
{
/// 内部存储边界；范围、用途、线程和句柄由公共执行层先行验证。
class BufferStorage
{
public:
    virtual ~BufferStorage() = default;
    virtual Result<void> Write(std::uint64_t offset,
                               ByteSpan data) noexcept = 0;
    virtual Result<void> Read(std::uint64_t offset,
                              MutableByteSpan data) noexcept = 0;
};

class TextureStorage
{
public:
    virtual ~TextureStorage() = default;
};
class PipelineStorage;
class SamplerStorage
{
public:
    virtual ~SamplerStorage() = default;
};
class BindingLayoutStorage
{
public:
    virtual ~BindingLayoutStorage() = default;
};
class BindingSetStorage
{
public:
    virtual ~BindingSetStorage() = default;
};
struct BackendUniformBinding
{
    BufferStorage *buffer;
    std::uint64_t offset;
    std::uint64_t size;
};
using BackendBinding =
    std::variant<BackendUniformBinding, TextureStorage *, SamplerStorage *>;

enum class OperationKind
{
    Barrier,
    Copy,
    TextureBarrier,
    BeginPass,
    EndPass,
    Pipeline,
    BindingSet,
    Vertex,
    Index,
    Viewport,
    Scissor,
    Draw,
    DrawIndexed,
    ReadTexture,
};

struct BackendOperation
{
    using Kind = OperationKind;
    Kind kind = Kind::Barrier;
    BufferStorage *src = nullptr;
    BufferStorage *dst = nullptr;
    BufferUse before = BufferUse::Initial;
    BufferUse after = BufferUse::Initial;
    std::uint64_t srcOffset = 0;
    std::uint64_t dstOffset = 0;
    std::uint64_t size = 0;
    TextureStorage *texture = nullptr;
    TextureStorage *depth = nullptr;
    PipelineStorage *pipeline = nullptr;
    BindingSetStorage *bindingSet = nullptr;
    TextureUse textureBefore = TextureUse::Initial;
    TextureUse textureAfter = TextureUse::Initial;
    ClearColor clear;
    float clearDepth = 1;
    voryx::Viewport viewport;
    Rect2D scissor;
    IndexType indexType = IndexType::UInt32;
    std::uint32_t count = 0;
    std::uint32_t first = 0;
    std::int32_t vertexOffset = 0;
};

/// 后端拥有的不可变管线对象，通过虚析构回收原生状态。
class PipelineStorage
{
public:
    virtual ~PipelineStorage() = default;
};

/// 后端提交对象持有原生命令与同步资源直到 GPU 完成。
class BackendSubmission
{
public:
    virtual ~BackendSubmission() = default;
    virtual Result<bool> Poll() noexcept = 0;
    virtual Result<void> Wait(TimeoutNs timeoutNs) noexcept = 0;
    /// 同队列的较新 fence 完成或原生等待确认设备丢失时，可回收此前批次。
    virtual void AcknowledgeCompletion() noexcept = 0;
};

class BufferBackend
{
public:
    virtual ~BufferBackend() = default;
    virtual Result<std::unique_ptr<BufferStorage>>
    CreateStorage(const BufferDesc &desc) = 0;
    virtual Result<std::unique_ptr<PipelineStorage>>
    CreatePipeline(const GraphicsPipelineDesc &desc,
                   BindingLayoutStorage *layout) = 0;
    virtual Result<std::unique_ptr<TextureStorage>>
    CreateSampledTexture(const Rgba8TextureDesc &desc) = 0;
    virtual Result<std::unique_ptr<SamplerStorage>>
    CreateSampler(const SamplerDesc &desc) = 0;
    virtual Result<std::unique_ptr<BindingLayoutStorage>>
    CreateBindingLayout(const BindingLayoutDesc &desc) = 0;
    virtual Result<std::unique_ptr<BindingSetStorage>>
    CreateBindingSet(const BindingLayoutStorage &layout,
                     std::span<const BackendBinding> resources) = 0;
    virtual Result<std::unique_ptr<TextureStorage>>
    CreateTexture(const TextureDesc &desc) = 0;
    virtual Result<void> ReadTexture(TextureStorage &texture,
                                     MutableByteSpan output) noexcept = 0;
    virtual Result<void>
    Execute(std::span<const BackendOperation> operations) noexcept = 0;
    virtual Result<std::unique_ptr<BackendSubmission>>
    EnqueueFrame(std::span<const BackendOperation>, TextureStorage &) noexcept
    {
        return Result<std::unique_ptr<BackendSubmission>>::Failure(
            {ErrorCode::Unsupported, {}, {}, {}});
    }
    virtual Result<std::unique_ptr<BackendSubmission>>
    Enqueue(std::span<const BackendOperation>) noexcept
    {
        return Result<std::unique_ptr<BackendSubmission>>::Failure(
            {ErrorCode::Unsupported, {}, {}, {}});
    }
};

Result<std::unique_ptr<IDevice>>
CreateBufferDevice(std::uint64_t identity, DeviceInfo info,
                   DeviceCapabilities capabilities,
                   std::unique_ptr<BufferBackend> backend) noexcept;
} // namespace voryx::rhi_detail

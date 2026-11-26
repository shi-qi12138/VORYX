#pragma once

#include <voryx/core/result.hpp>
#include <voryx/rhi/pipeline.hpp>
#include <voryx/rhi/texture.hpp>
#include <voryx/rhi/types.hpp>

#include <span>

namespace voryx
{
class ICommandList
{
public:
    virtual ~ICommandList() = default;
    ICommandList(const ICommandList &) = delete;
    ICommandList &operator=(const ICommandList &) = delete;

    virtual Result<CommandState> GetState() const noexcept = 0;
    /// 仅允许 Initial 状态开始录制；复用前必须 Reset。
    virtual Result<void> Begin() noexcept = 0;
    virtual Result<void>
    Barrier(std::span<const BufferBarrier> barriers) noexcept = 0;
    /// 记录时验证范围与用途，提交时按整批顺序验证资源状态。
    virtual Result<void> CopyBuffer(const BufferCopyDesc &copy) noexcept = 0;
    virtual Result<void>
    Barrier(std::span<const TextureBarrier> barriers) noexcept = 0;
    virtual Result<void>
    BeginRenderPass(const RenderPassDesc &desc) noexcept = 0;
    virtual Result<void> SetPipeline(PipelineHandle pipeline) noexcept = 0;
    /// 活动 Pass 内绑定同一布局的集合。SetPipeline 后需要重新绑定。
    virtual Result<void> SetBindingSet(DeviceBindingSetHandle set) noexcept = 0;
    virtual Result<void>
    SetVertexBuffer(BufferHandle buffer,
                    std::uint64_t offsetBytes = 0) noexcept = 0;
    virtual Result<void>
    SetIndexBuffer(BufferHandle buffer, IndexType type,
                   std::uint64_t offsetBytes = 0) noexcept = 0;
    virtual Result<void> SetViewport(const Viewport &viewport) noexcept = 0;
    virtual Result<void> SetScissor(const Rect2D &scissor) noexcept = 0;
    virtual Result<void> Draw(std::uint32_t vertexCount,
                              std::uint32_t firstVertex = 0) noexcept = 0;
    /// 校验索引字节范围，索引内容与有符号顶点偏移的有效性由调用方保证。
    virtual Result<void>
    DrawIndexed(std::uint32_t indexCount, std::uint32_t firstIndex = 0,
                std::int32_t vertexOffset = 0) noexcept = 0;
    virtual Result<void> EndRenderPass() noexcept = 0;
    virtual Result<void> End() noexcept = 0;
    /// 释放未提交引用并回到 Initial；真实后端执行中不能直接重置。
    virtual Result<void> Reset() noexcept = 0;

protected:
    ICommandList() = default;
};

enum class SubmitMode : std::uint8_t
{
    Synchronous,
    Async,
};

struct SubmitDesc
{
    std::span<ICommandList *const> commandLists;
    /// 窗口图像参与提交时填写对应令牌，离屏提交使用默认值。
    FrameToken frame{};
    /// 异步模式要求设备支持对应的离屏或窗口提交能力。
    SubmitMode mode = SubmitMode::Synchronous;
};
} // namespace voryx

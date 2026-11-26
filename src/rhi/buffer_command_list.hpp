#pragma once

#include "buffer_state.hpp"

#include <voryx/rhi/command_list.hpp>

#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace voryx::rhi_detail
{
class BufferCommandList final : public ICommandList
{
public:
    explicit BufferCommandList(std::shared_ptr<State> device);
    ~BufferCommandList() override;
    Result<CommandState> GetState() const noexcept override;
    Result<void> Begin() noexcept override;
    Result<void>
    Barrier(std::span<const BufferBarrier> barriers) noexcept override;
    Result<void> CopyBuffer(const BufferCopyDesc &copy) noexcept override;
    Result<void>
    Barrier(std::span<const TextureBarrier> barriers) noexcept override;
    Result<void> BeginRenderPass(const RenderPassDesc &desc) noexcept override;
    Result<void> SetPipeline(PipelineHandle pipeline) noexcept override;
    Result<void> SetBindingSet(DeviceBindingSetHandle set) noexcept override;
    Result<void> SetVertexBuffer(BufferHandle buffer,
                                 std::uint64_t offset) noexcept override;
    Result<void> SetIndexBuffer(BufferHandle buffer, IndexType type,
                                std::uint64_t offset) noexcept override;
    Result<void> SetViewport(const Viewport &viewport) noexcept override;
    Result<void> SetScissor(const Rect2D &scissor) noexcept override;
    Result<void> Draw(std::uint32_t count,
                      std::uint32_t first) noexcept override;
    Result<void> DrawIndexed(std::uint32_t count, std::uint32_t first,
                             std::int32_t offset) noexcept override;
    Result<void> EndRenderPass() noexcept override;
    Result<void> End() noexcept override;
    Result<void> Reset() noexcept override;

private:
    friend class BufferDevice;
    void ReleaseReferences() noexcept;
    Result<void> Append(Operation operation);
    std::optional<ErrorCode> Recording(bool inPass) const noexcept;
    void ClearBindings() noexcept;
    Result<void> RecordDraw(std::uint32_t count, std::uint32_t first,
                            std::int32_t offset, bool indexed);
    Texture *color = nullptr;
    Texture *depth = nullptr;
    Pipeline *pipeline = nullptr;
    Buffer *vertex = nullptr;
    Buffer *index = nullptr;
    std::uint64_t vertexOffset = 0;
    std::uint64_t indexOffset = 0;
    IndexType indexType = IndexType::UInt32;
    bool bindingSetBound = false;
    bool viewportSet = false;
    bool scissorSet = false;
    [[nodiscard]] std::optional<BufferUse>
    RecordedUse(const Buffer *buffer) const noexcept;
    std::shared_ptr<State> device;
    CommandState state = CommandState::Initial;
    std::uint64_t pendingSerial = 0;
    std::vector<Operation> operations;
};
} // namespace voryx::rhi_detail

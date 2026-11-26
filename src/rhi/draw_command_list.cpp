#include "buffer_command_list.hpp"
#include <cmath>
#include <utility>

namespace voryx::rhi_detail
{
std::optional<ErrorCode>
BufferCommandList::Recording(bool inPass) const noexcept
{
    if (const auto error = device->CheckAccess()) {
        return error;
    }
    if (state != CommandState::Recording || (color != nullptr) != inPass) {
        return ErrorCode::InvalidState;
    }
    return std::nullopt;
}
void BufferCommandList::ClearBindings() noexcept
{
    pipeline = nullptr;
    vertex = nullptr;
    index = nullptr;
    vertexOffset = 0;
    indexOffset = 0;
    bindingSetBound = false;
    viewportSet = false;
    scissorSet = false;
}
Result<void> BufferCommandList::Append(Operation operation)
{
    if (operations.size() == device->capabilities.maxCommandsPerList) {
        return Fail<void>(ErrorCode::Unsupported);
    }
    operations.push_back(std::move(operation));
    const auto &added = operations.back();
    if (added.src) {
        ++added.src->recordingRefs;
    }
    if (added.dst) {
        ++added.dst->recordingRefs;
    }
    if (added.texture) {
        ++added.texture->recordingRefs;
    }
    if (added.depth) {
        ++added.depth->recordingRefs;
    }
    if (added.bindingSet) {
        added.bindingSet->AddRecordingRefs();
    }
    if (added.pipeline) {
        ++added.pipeline->recordingRefs;
    }
    return Result<void>::Success();
}
Result<void>
BufferCommandList::Barrier(std::span<const TextureBarrier> barriers) noexcept
{
    return Invoke<void>("TextureBarrier", [&]() {
        if (const auto error = Recording(false)) {
            return Fail<void>(*error);
        }
        if (barriers.size()
            > device->capabilities.maxCommandsPerList - operations.size()) {
            return Fail<void>(ErrorCode::Unsupported);
        }
        std::vector<Operation> additions;
        additions.reserve(barriers.size());
        for (const auto &barrier : barriers) {
            auto found = device->Lookup(barrier.texture);
            if (!found) {
                return Fail<void>(found.GetError().errorCode);
            }
            const auto &texture = found.Value();
            if (barrier.afterUse == TextureUse::Initial
                || !Allows(*texture, barrier.beforeUse)
                || !Allows(*texture, barrier.afterUse)) {
                return Fail<void>(ErrorCode::InvalidArgument);
            }
            if (texture->windowImage
                && !((barrier.beforeUse == TextureUse::Present
                      && barrier.afterUse == TextureUse::ColorAttachment)
                     || (barrier.beforeUse == TextureUse::ColorAttachment
                         && barrier.afterUse == TextureUse::Present))) {
                return Fail<void>(ErrorCode::InvalidArgument);
            }
            std::optional<TextureUse> previous;
            for (const auto &op : operations) {
                if (op.kind == OperationKind::TextureBarrier
                    && op.texture == texture) {
                    previous = op.native.textureAfter;
                }
            }
            for (const auto &op : additions) {
                if (op.texture == texture) {
                    previous = op.native.textureAfter;
                }
            }
            if (previous && *previous != barrier.beforeUse) {
                return Fail<void>(ErrorCode::InvalidState);
            }
            Operation op{};
            op.kind = OperationKind::TextureBarrier;
            op.texture = texture;
            op.native.textureBefore = barrier.beforeUse;
            op.native.textureAfter = barrier.afterUse;
            additions.push_back(std::move(op));
        }
        operations.reserve(operations.size() + additions.size());
        for (auto &op : additions) {
            const auto appended = Append(std::move(op));
            if (!appended) {
                std::terminate();
            }
        }
        return Result<void>::Success();
    });
}
Result<void>
BufferCommandList::BeginRenderPass(const RenderPassDesc &desc) noexcept
{
    return Invoke<void>("BeginRenderPass", [&]() {
        if (const auto error = Recording(false)) {
            return Fail<void>(*error);
        }
        if (!std::isfinite(desc.clearColor.red)
            || !std::isfinite(desc.clearColor.green)
            || !std::isfinite(desc.clearColor.blue)
            || !std::isfinite(desc.clearColor.alpha)
            || !std::isfinite(desc.clearDepth) || desc.clearDepth < 0
            || desc.clearDepth > 1) {
            return Fail<void>(ErrorCode::InvalidArgument);
        }
        auto target = device->Lookup(desc.color);
        if (!target) {
            return Fail<void>(target.GetError().errorCode);
        }
        std::shared_ptr<Texture> depthTarget;
        if (desc.depth.IsValid()) {
            auto found = device->Lookup(desc.depth);
            if (!found) {
                return Fail<void>(found.GetError().errorCode);
            }
            depthTarget = std::move(found).Value();
        }
        if (!HasUsage(target.Value()->usage, TextureUsage::ColorAttachment)
            || (depthTarget
                && (!HasUsage(depthTarget->usage, TextureUsage::DepthAttachment)
                    || depthTarget->extent.width != target.Value()->extent.width
                    || depthTarget->extent.height
                           != target.Value()->extent.height))) {
            return Fail<void>(ErrorCode::InvalidArgument);
        }
        Operation op{};
        op.kind = OperationKind::BeginPass;
        op.texture = target.Value();
        op.depth = depthTarget;
        op.native.clear = desc.clearColor;
        op.native.clearDepth = desc.clearDepth;
        auto added = Append(std::move(op));
        if (!added) {
            return added;
        }
        color = target.Value().get();
        depth = depthTarget.get();
        ClearBindings();
        return Result<void>::Success();
    });
}
Result<void> BufferCommandList::SetPipeline(PipelineHandle handle) noexcept
{
    return Invoke<void>("SetPipeline", [&]() {
        if (const auto error = Recording(true)) {
            return Fail<void>(*error);
        }
        auto found = device->Lookup(handle);
        if (!found) {
            return Fail<void>(found.GetError().errorCode);
        }
        if (found.Value()->colorFormat != color->format
            || found.Value()->depthTest != (depth != nullptr)) {
            return Fail<void>(ErrorCode::InvalidArgument);
        }
        Operation op{};
        op.kind = OperationKind::Pipeline;
        op.pipeline = found.Value();
        auto added = Append(std::move(op));
        if (!added) {
            return added;
        }
        pipeline = found.Value().get();
        bindingSetBound = false;
        return Result<void>::Success();
    });
}
Result<void>
BufferCommandList::SetBindingSet(DeviceBindingSetHandle handle) noexcept
{
    return Invoke<void>("SetBindingSet", [&]() {
        if (const auto error = Recording(true)) {
            return Fail<void>(*error);
        }
        if (!pipeline) {
            return Fail<void>(ErrorCode::InvalidState);
        }
        auto found = device->Lookup(handle);
        if (!found) {
            return Fail<void>(found.GetError().errorCode);
        }
        if (found.Value()->layout != pipeline->bindingLayout) {
            return Fail<void>(ErrorCode::InvalidArgument);
        }
        for (const auto &buffer : found.Value()->buffers) {
            const auto use = RecordedUse(buffer.get());
            if (use && *use != BufferUse::ConstantRead) {
                return Fail<void>(ErrorCode::InvalidState);
            }
        }
        Operation operation{};
        operation.kind = OperationKind::BindingSet;
        operation.bindingSet = std::move(found).Value();
        auto added = Append(std::move(operation));
        if (added) {
            bindingSetBound = true;
        }
        return added;
    });
}

Result<void> BufferCommandList::SetVertexBuffer(BufferHandle handle,
                                                std::uint64_t offset) noexcept
{
    return Invoke<void>("SetVertexBuffer", [&]() {
        if (const auto error = Recording(true)) {
            return Fail<void>(*error);
        }
        auto found = device->Lookup(handle);
        if (!found) {
            return Fail<void>(found.GetError().errorCode);
        }
        if (!HasUsage(found.Value()->usage, BufferUsage::Vertex)
            || offset >= found.Value()->sizeBytes || offset % 4 != 0) {
            return Fail<void>(ErrorCode::InvalidArgument);
        }
        const auto use = RecordedUse(found.Value().get());
        if (use && *use != BufferUse::VertexRead) {
            return Fail<void>(ErrorCode::InvalidState);
        }
        Operation op{};
        op.kind = OperationKind::Vertex;
        op.src = found.Value();
        op.srcOffset = offset;
        auto added = Append(std::move(op));
        if (!added) {
            return added;
        }
        vertex = found.Value().get();
        vertexOffset = offset;
        return Result<void>::Success();
    });
}
Result<void> BufferCommandList::SetIndexBuffer(BufferHandle handle,
                                               IndexType type,
                                               std::uint64_t offset) noexcept
{
    return Invoke<void>("SetIndexBuffer", [&]() {
        if (const auto error = Recording(true)) {
            return Fail<void>(*error);
        }
        if (type > IndexType::UInt32) {
            return Fail<void>(ErrorCode::InvalidArgument);
        }
        auto found = device->Lookup(handle);
        if (!found) {
            return Fail<void>(found.GetError().errorCode);
        }
        const auto width = type == IndexType::UInt16 ? 2u : 4u;
        if (!HasUsage(found.Value()->usage, BufferUsage::Index)
            || offset >= found.Value()->sizeBytes || offset % width != 0) {
            return Fail<void>(ErrorCode::InvalidArgument);
        }
        const auto use = RecordedUse(found.Value().get());
        if (use && *use != BufferUse::IndexRead) {
            return Fail<void>(ErrorCode::InvalidState);
        }
        Operation op{};
        op.kind = OperationKind::Index;
        op.src = found.Value();
        op.srcOffset = offset;
        op.native.indexType = type;
        auto added = Append(std::move(op));
        if (!added) {
            return added;
        }
        index = found.Value().get();
        indexOffset = offset;
        indexType = type;
        return Result<void>::Success();
    });
}
Result<void> BufferCommandList::SetViewport(const Viewport &value) noexcept
{
    return Invoke<void>("SetViewport", [&]() {
        if (const auto error = Recording(true)) {
            return Fail<void>(*error);
        }
        if (!std::isfinite(value.x) || !std::isfinite(value.y)
            || !std::isfinite(value.width) || !std::isfinite(value.height)
            || !std::isfinite(value.minDepth) || !std::isfinite(value.maxDepth)
            || value.x < 0 || value.y < 0 || value.width <= 0
            || value.height <= 0 || value.x + value.width > color->extent.width
            || value.y + value.height > color->extent.height
            || value.minDepth < 0 || value.maxDepth > 1
            || value.minDepth > value.maxDepth) {
            return Fail<void>(ErrorCode::InvalidArgument);
        }
        Operation op{};
        op.kind = OperationKind::Viewport;
        op.native.viewport = value;
        auto added = Append(std::move(op));
        if (added) {
            viewportSet = true;
        }
        return added;
    });
}
Result<void> BufferCommandList::SetScissor(const Rect2D &value) noexcept
{
    return Invoke<void>("SetScissor", [&]() {
        if (const auto error = Recording(true)) {
            return Fail<void>(*error);
        }
        if (value.width == 0 || value.height == 0
            || !InRange(value.x, value.width, color->extent.width)
            || !InRange(value.y, value.height, color->extent.height)) {
            return Fail<void>(ErrorCode::InvalidArgument);
        }
        Operation op{};
        op.kind = OperationKind::Scissor;
        op.native.scissor = value;
        auto added = Append(std::move(op));
        if (added) {
            scissorSet = true;
        }
        return added;
    });
}
Result<void> BufferCommandList::RecordDraw(std::uint32_t count,
                                           std::uint32_t first,
                                           std::int32_t offset, bool indexed)
{
    if (const auto error = Recording(true)) {
        return Fail<void>(*error);
    }
    if (!pipeline || (pipeline->bindingLayout && !bindingSetBound)
        || !viewportSet || !scissorSet || (pipeline->hasVertices && !vertex)
        || (indexed && !index)) {
        return Fail<void>(ErrorCode::InvalidState);
    }
    if (count == 0 || count % 3 != 0) {
        return Fail<void>(ErrorCode::InvalidArgument);
    }
    if (indexed) {
        const auto width = indexType == IndexType::UInt16 ? 2u : 4u;
        if (!InRange(indexOffset, (std::uint64_t(first) + count) * width,
                     index->sizeBytes)
            || (pipeline->hasVertices
                && !InRange(vertexOffset, pipeline->stride,
                            vertex->sizeBytes))) {
            return Fail<void>(ErrorCode::InvalidArgument);
        }
    } else if (pipeline->hasVertices
               && !InRange(vertexOffset,
                           (std::uint64_t(first) + count) * pipeline->stride,
                           vertex->sizeBytes)) {
        return Fail<void>(ErrorCode::InvalidArgument);
    }
    Operation op{};
    op.kind = indexed ? OperationKind::DrawIndexed : OperationKind::Draw;
    op.native.count = count;
    op.native.first = first;
    op.native.vertexOffset = offset;
    return Append(std::move(op));
}
Result<void> BufferCommandList::Draw(std::uint32_t count,
                                     std::uint32_t first) noexcept
{
    return Invoke<void>("Draw", [&]() {
        return RecordDraw(count, first, 0, false);
    });
}
Result<void> BufferCommandList::DrawIndexed(std::uint32_t count,
                                            std::uint32_t first,
                                            std::int32_t offset) noexcept
{
    return Invoke<void>("DrawIndexed", [&]() {
        return RecordDraw(count, first, offset, true);
    });
}
Result<void> BufferCommandList::EndRenderPass() noexcept
{
    return Invoke<void>("EndRenderPass", [&]() {
        if (const auto error = Recording(true)) {
            return Fail<void>(*error);
        }
        Operation op{};
        op.kind = OperationKind::EndPass;
        auto added = Append(std::move(op));
        if (added) {
            color = nullptr;
            depth = nullptr;
            ClearBindings();
        }
        return added;
    });
}
} // namespace voryx::rhi_detail

#include "buffer_device.hpp"
#include "handle_access.hpp"
#include "pipeline_validation.hpp"
#include <limits>

namespace voryx::rhi_detail
{
Result<PipelineHandle>
BufferDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc &desc) noexcept
{
    return Invoke<PipelineHandle>("CreateGraphicsPipeline", [&]() {
        if (const auto error = state->CheckAccess()) {
            return Fail<PipelineHandle>(*error);
        }
        std::shared_ptr<BindingLayout> layout;
        if (desc.bindingLayout.IsValid()) {
            auto found = state->Lookup(desc.bindingLayout);
            if (!found) {
                return Fail<PipelineHandle>(found.GetError().errorCode);
            }
            layout = std::move(found).Value();
        }
        const auto entries =
            layout ? std::span<const BindingLayoutEntry>(layout->entries)
                   : std::span<const BindingLayoutEntry>{};
        if (!ValidGraphicsPipeline(desc, entries)) {
            return Fail<PipelineHandle>(ErrorCode::InvalidArgument);
        }
        std::size_t index = 0;
        for (; index < state->pipelines.size(); ++index) {
            const auto &slot = state->pipelines[index];
            if (!slot.pipeline
                && slot.generation
                       != std::numeric_limits<std::uint64_t>::max()) {
                break;
            }
        }
        if (index == state->pipelines.size()
            && index == state->capabilities.maxGraphicsPipelines) {
            return Fail<PipelineHandle>(ErrorCode::OutOfMemory);
        }
        auto pipeline = std::make_shared<Pipeline>();
        pipeline->stride = desc.vertexStrideBytes;
        pipeline->hasVertices = !desc.vertexAttributes.empty();
        pipeline->colorFormat = desc.colorFormat;
        pipeline->depthTest = desc.depthTest;
        pipeline->bindingLayout = std::move(layout);
        auto *nativeLayout = pipeline->bindingLayout
                                 ? pipeline->bindingLayout->storage.get()
                                 : nullptr;
        auto created = state->backend->CreatePipeline(desc, nativeLayout);
        if (!created) {
            if (created.GetError().errorCode == ErrorCode::DeviceLost) {
                state->lost = true;
            }
            return Result<PipelineHandle>::Failure(
                std::move(created).GetError());
        }
        if (index == state->pipelines.size()) {
            state->pipelines.emplace_back();
        }
        auto &slot = state->pipelines[index];
        pipeline->storage = std::move(created).Value();
        slot.pipeline = std::move(pipeline);
        return Result<PipelineHandle>::Success(
            detail::RhiHandleAccess::Pipeline(state->identity,
                                              static_cast<std::uint32_t>(index),
                                              slot.generation));
    });
}

Result<void> BufferDevice::Destroy(PipelineHandle handle) noexcept
{
    return Invoke<void>("DestroyPipeline", [&]() {
        if (const auto error = state->CheckAccess(DeviceAccess::Cleanup)) {
            return Fail<void>(*error);
        }
        using Access = detail::RhiHandleAccess;
        if (!handle.IsValid()) {
            return Fail<void>(ErrorCode::InvalidHandle);
        }
        if (Access::Owner(handle) != state->identity) {
            return Fail<void>(ErrorCode::WrongDevice);
        }
        const auto index = Access::Slot(handle);
        if (index >= state->pipelines.size()
            || !state->pipelines[index].pipeline
            || state->pipelines[index].generation
                   != Access::Generation(handle)) {
            return Fail<void>(ErrorCode::InvalidHandle);
        }
        auto &slot = state->pipelines[index];
        if (slot.pipeline->recordingRefs != 0) {
            return Fail<void>(ErrorCode::ResourceBusy);
        }
        slot.pipeline.reset();
        if (slot.generation != std::numeric_limits<std::uint64_t>::max()) {
            ++slot.generation;
        }
        return Result<void>::Success();
    });
}
} // namespace voryx::rhi_detail

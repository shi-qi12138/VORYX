#include "buffer_backend.hpp"
#include "buffer_device.hpp"

#include <array>
#include <limits>
#include <memory>
#include <support/rhi_support.hpp>

namespace voryx::test
{
namespace
{
using namespace rhi_detail;

struct CompletionGate
{
    unsigned accepted = 0;
    unsigned completed = 0;
    unsigned retired = 0;
    TimeoutNs lastTimeout = 0;
};

class ControlledFence final : public BackendSubmission
{
public:
    ControlledFence(CompletionGate &gate, unsigned serial)
        : gate(gate)
        , serial(serial)
    {
    }

    Result<bool> Poll() noexcept override
    {
        return Result<bool>::Success(serial <= gate.completed);
    }

    Result<void> Wait(TimeoutNs timeoutNs) noexcept override
    {
        gate.lastTimeout = timeoutNs;
        if (serial > gate.completed) {
            return Result<void>::Failure({ErrorCode::Timeout, {}, {}, {}});
        }
        return Result<void>::Success();
    }

    void AcknowledgeCompletion() noexcept override
    {
        ++gate.retired;
    }

private:
    CompletionGate &gate;
    unsigned serial;
};

template<typename T = void>
Result<T> Unused()
{
    return Result<T>::Failure({ErrorCode::Unsupported, {}, {}, {}});
}

class ControlledBackend final : public BufferBackend
{
public:
    explicit ControlledBackend(CompletionGate &gate)
        : gate(gate)
    {
    }

    Result<std::unique_ptr<BufferStorage>>
    CreateStorage(const BufferDesc &) override
    {
        return Unused<std::unique_ptr<BufferStorage>>();
    }

    Result<std::unique_ptr<PipelineStorage>>
    CreatePipeline(const GraphicsPipelineDesc &,
                   BindingLayoutStorage *) override
    {
        return Unused<std::unique_ptr<PipelineStorage>>();
    }

    Result<std::unique_ptr<TextureStorage>>
    CreateSampledTexture(const Rgba8TextureDesc &) override
    {
        return Unused<std::unique_ptr<TextureStorage>>();
    }

    Result<std::unique_ptr<SamplerStorage>>
    CreateSampler(const SamplerDesc &) override
    {
        return Unused<std::unique_ptr<SamplerStorage>>();
    }

    Result<std::unique_ptr<BindingLayoutStorage>>
    CreateBindingLayout(const BindingLayoutDesc &) override
    {
        return Unused<std::unique_ptr<BindingLayoutStorage>>();
    }

    Result<std::unique_ptr<BindingSetStorage>>
    CreateBindingSet(const BindingLayoutStorage &,
                     std::span<const BackendBinding>) override
    {
        return Unused<std::unique_ptr<BindingSetStorage>>();
    }

    Result<std::unique_ptr<TextureStorage>>
    CreateTexture(const TextureDesc &) override
    {
        return Unused<std::unique_ptr<TextureStorage>>();
    }

    Result<void> ReadTexture(TextureStorage &,
                             MutableByteSpan) noexcept override
    {
        return Unused();
    }

    Result<void> Execute(std::span<const BackendOperation>) noexcept override
    {
        return Unused();
    }

    Result<std::unique_ptr<BackendSubmission>>
    Enqueue(std::span<const BackendOperation>) noexcept override
    {
        using Completion = std::unique_ptr<BackendSubmission>;
        try {
            auto fence =
                std::make_unique<ControlledFence>(gate, gate.accepted + 1);
            ++gate.accepted;
            return Result<Completion>::Success(std::move(fence));
        } catch (const std::bad_alloc &) {
            return Result<Completion>::Failure(
                {ErrorCode::OutOfMemory, {}, {}, {}});
        }
    }

private:
    CompletionGate &gate;
};

std::unique_ptr<ICommandList> EmptyCommand(IDevice &device)
{
    auto command = Take(device.CreateCommandList());
    Take(command->Begin());
    Take(command->End());
    return command;
}
} // namespace

TEST(AsyncStateTest, PendingTimeoutAndQueuePressureAllowRetry)
{
    CompletionGate gate;
    DeviceCapabilities capabilities{};
    capabilities.maxCommandListsPerSubmit = 2;
    capabilities.maxPendingSubmissions = 2;
    auto device = Take(CreateBufferDevice(
        1001, {}, capabilities, std::make_unique<ControlledBackend>(gate)));
    auto first = EmptyCommand(*device);
    auto second = EmptyCommand(*device);
    auto retry = EmptyCommand(*device);
    const std::array firstBatch{first.get()};
    const std::array secondBatch{second.get()};
    const std::array retryBatch{retry.get()};
    const auto earlier =
        Take(device->Submit({firstBatch, {}, SubmitMode::Async}));
    const auto later =
        Take(device->Submit({secondBatch, {}, SubmitMode::Async}));
    EXPECT_EQ(Take(first->GetState()), CommandState::Pending);
    ExpectError(first->Reset(), ErrorCode::ResourceBusy);
    EXPECT_FALSE(Take(device->IsComplete(earlier)));
    ExpectError(device->Wait(later, 0), ErrorCode::Timeout);
    ExpectError(device->Wait(later, 125), ErrorCode::Timeout);
    EXPECT_EQ(gate.lastTimeout, 125u);
    ExpectError(device->Submit({retryBatch, {}, SubmitMode::Async}),
                ErrorCode::ResourceBusy);
    EXPECT_EQ(Take(retry->GetState()), CommandState::Executable);
    EXPECT_EQ(gate.accepted, 2u);
    EXPECT_EQ(gate.retired, 0u);

    gate.completed = 2;
    Take(device->Wait(later, 0));
    EXPECT_EQ(gate.retired, 2u);
    EXPECT_TRUE(Take(device->IsComplete(earlier)));
    EXPECT_EQ(Take(first->GetState()), CommandState::Completed);
    Take(first->Reset());
    const auto accepted =
        Take(device->Submit({retryBatch, {}, SubmitMode::Async}));
    EXPECT_NE(accepted, later);
    gate.completed = 3;
    device.reset();
    EXPECT_EQ(gate.retired, 3u);
    EXPECT_EQ(gate.lastTimeout, std::numeric_limits<TimeoutNs>::max());
}

TEST(AsyncStateTest, ShutdownRetainsPendingWorkAfterTimeoutAndAllowsRetry)
{
    CompletionGate gate;
    DeviceCapabilities capabilities{};
    capabilities.maxCommandListsPerSubmit = 2;
    capabilities.maxPendingSubmissions = 2;
    auto device = Take(CreateBufferDevice(
        1002, {}, capabilities, std::make_unique<ControlledBackend>(gate)));
    auto &closing = static_cast<BufferDevice &>(*device);
    auto list = EmptyCommand(*device);
    const std::array batch{list.get()};
    const auto token = Take(device->Submit({batch, {}, SubmitMode::Async}));
    ExpectError(closing.PrepareShutdown(0), ErrorCode::ResourceBusy);
    ExpectError(device->CreateCommandList(), ErrorCode::InvalidState);
    ExpectError(device->Submit({batch}), ErrorCode::InvalidState);
    ExpectError(list->Reset(), ErrorCode::ResourceBusy);
    list.reset();

    ExpectError(closing.PrepareShutdown(125), ErrorCode::Timeout);
    EXPECT_EQ(gate.lastTimeout, 125u);
    EXPECT_EQ(gate.retired, 0u);
    EXPECT_FALSE(Take(device->IsComplete(token)));
    ExpectError(device->CreateCommandList(), ErrorCode::InvalidState);

    gate.completed = 1;
    Take(closing.PrepareShutdown(0));
    EXPECT_EQ(gate.retired, 1u);
    EXPECT_TRUE(Take(device->IsComplete(token)));
    closing.FinishShutdown();
    ExpectError(device->Wait(token, 0), ErrorCode::InvalidState);
}
} // namespace voryx::test

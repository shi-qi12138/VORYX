#include "buffer_backend.hpp"
#include "buffer_device.hpp"
#include "vulkan_frame_wait.hpp"

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

    Result<std::unique_ptr<BackendSubmission>>
    EnqueueFrame(std::span<const BackendOperation> operations,
                 TextureStorage &) noexcept override
    {
        return Enqueue(operations);
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
TEST(AsyncStateTest, FrameRetirementKeepsTheSubmissionOnTimeout)
{
    CompletionGate gate;
    DeviceCapabilities caps{};
    caps.maxCommandsPerList = 8;
    caps.maxCommandListsPerSubmit = 1;
    caps.maxPendingSubmissions = 1;
    caps.maxPendingWindowFrames = 1;
    auto device = Take(CreateBufferDevice(
        1003, {}, caps, std::make_unique<ControlledBackend>(gate)));
    auto &frameDevice = static_cast<BufferDevice &>(*device);
    const auto frame = Take(frameDevice.PublishFrame(
        {4, 4}, Format::RGBA8_UNorm, std::make_unique<TextureStorage>()));
    auto command = Take(device->CreateCommandList());
    Take(command->Begin());
    const std::array toColor{TextureBarrier{frame.color, TextureUse::Present,
                                            TextureUse::ColorAttachment}};
    const std::array toPresent{TextureBarrier{
        frame.color, TextureUse::ColorAttachment, TextureUse::Present}};
    Take(command->Barrier(toColor));
    Take(command->BeginRenderPass({frame.color, {0, 0, 0, 1}, {}, 1}));
    Take(command->EndRenderPass());
    Take(command->Barrier(toPresent));
    Take(command->End());
    const std::array commands{command.get()};
    const auto submit =
        Take(device->Submit({commands, frame.token, SubmitMode::Async}));
    frameDevice.ConsumeFrame();

    ExpectError(frameDevice.WaitForFrameSubmissions(0), ErrorCode::Timeout);
    EXPECT_EQ(gate.lastTimeout, 0u);
    EXPECT_EQ(gate.retired, 0u);
    EXPECT_FALSE(Take(device->IsComplete(submit)));
    gate.completed = gate.accepted;
    Take(frameDevice.WaitForFrameSubmissions(0));
    EXPECT_EQ(gate.retired, 1u);
    EXPECT_TRUE(Take(device->IsComplete(submit)));
}

TEST(AsyncStateTest, ImageFenceRetryDoesNotAcquireAnotherImage)
{
    using namespace vulkan_backend;
    struct FencePending
    {
    };
    bool ready = false;
    unsigned acquires = 0;
    unsigned slotWaits = 0;
    unsigned imageWaits = 0;
    std::optional<std::uint32_t> held;
    const FrameWaitBudget zero(0);
    const auto run = [&]() {
        return AcquireImageWithinBudget(
            held, zero,
            [&](TimeoutNs remaining) {
                EXPECT_EQ(remaining, 0u);
                ++slotWaits;
            },
            [&](TimeoutNs remaining) {
                EXPECT_EQ(remaining, 0u);
                ++acquires;
                return 2u;
            },
            [&](TimeoutNs remaining) {
                EXPECT_EQ(remaining, 0u);
                ++imageWaits;
                if (!ready) {
                    throw FencePending{};
                }
            });
    };
    EXPECT_THROW(run(), FencePending);
    ASSERT_EQ(held, 2u);
    ready = true;
    EXPECT_EQ(run(), 2u);
    EXPECT_EQ(acquires, 1u);
    EXPECT_EQ(slotWaits, 1u);
    EXPECT_EQ(imageWaits, 2u);
}
} // namespace voryx::test

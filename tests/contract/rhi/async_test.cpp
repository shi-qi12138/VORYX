#include <support/rhi_support.hpp>

#include <array>
#include <cstddef>

namespace voryx::test
{
#if VORYX_TEST_VULKAN
TEST_F(BufferRhiTest, AsyncCopiesRunInOrderEvenAfterHandleAndListRelease)
{
    ASSERT_GT(device->GetCapabilities().maxPendingSubmissions, 1u);
    EXPECT_EQ(device->GetCapabilities().maxPendingWindowFrames, 0u);
    const std::array input{std::byte{12}, std::byte{34}, std::byte{56},
                           std::byte{78}};
    const auto source =
        Buffer(4, BufferUsage::CopySrc | BufferUsage::CopyDst, input);
    const auto intermediate = Buffer();
    const auto destination = Buffer();
    auto first = Command();
    Take(first->Begin());
    const std::array firstBarriers{
        BufferBarrier{source, BufferUse::Initial, BufferUse::CopySrc},
        BufferBarrier{intermediate, BufferUse::Initial, BufferUse::CopyDst}};
    Take(first->Barrier(firstBarriers));
    Take(first->CopyBuffer({source, intermediate, 0, 0, input.size()}));
    Take(first->End());
    const std::array firstBatch{first.get()};
    const auto earlier =
        Take(device->Submit({firstBatch, {}, SubmitMode::Async}));
    std::array<std::byte, input.size()> output{};
    ExpectError(device->ReadBuffer(intermediate, 0, output),
                ErrorCode::ResourceBusy);
    ExpectError(device->WriteBuffer(intermediate, 0, input),
                ErrorCode::ResourceBusy);
    Take(device->Destroy(source));
    ExpectError(device->ReadBuffer(source, 0, output),
                ErrorCode::InvalidHandle);
    first.reset();

    auto second = Command();
    Take(second->Begin());
    const std::array secondBarriers{
        BufferBarrier{intermediate, BufferUse::CopyDst, BufferUse::CopySrc},
        BufferBarrier{destination, BufferUse::Initial, BufferUse::CopyDst}};
    Take(second->Barrier(secondBarriers));
    Take(second->CopyBuffer({intermediate, destination, 0, 0, input.size()}));
    Take(second->End());
    const std::array secondBatch{second.get()};
    const auto later =
        Take(device->Submit({secondBatch, {}, SubmitMode::Async}));
    EXPECT_NE(later, earlier);
    Take(device->Destroy(intermediate));
    const auto immediate = device->Wait(later, 0);
    if (!immediate) {
        EXPECT_EQ(immediate.GetError().errorCode, ErrorCode::Timeout);
    }
    Take(device->Wait(later, 5'000'000'000));
    EXPECT_TRUE(Take(device->IsComplete(earlier)));
    EXPECT_TRUE(Take(device->IsComplete(later)));
    EXPECT_EQ(Take(second->GetState()), CommandState::Completed);
    Take(second->Reset());
    Take(device->ReadBuffer(destination, 0, output));
    EXPECT_EQ(output, input);
    Take(device->Destroy(destination));
}
#else
TEST_F(BufferRhiTest, NullRejectsAsyncWithoutConsumingTheCommandList)
{
    EXPECT_EQ(device->GetCapabilities().maxPendingSubmissions, 0u);
    EXPECT_EQ(device->GetCapabilities().maxPendingWindowFrames, 0u);
    auto command = Command();
    Take(command->Begin());
    Take(command->End());
    const std::array batch{command.get()};
    ExpectError(device->Submit({batch, {}, SubmitMode::Async}),
                ErrorCode::Unsupported);
    EXPECT_EQ(Take(command->GetState()), CommandState::Executable);
    const auto token = Take(device->Submit({batch}));
    EXPECT_TRUE(Take(device->IsComplete(token)));
}
#endif
} // namespace voryx::test

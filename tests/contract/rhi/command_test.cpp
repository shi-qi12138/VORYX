#include <support/rhi_support.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <vector>

namespace voryx::test
{
TEST_F(BufferRhiTest, CommandsRequireExplicitBeginEndAndSingleSubmission)
{
    auto command = Command();
    EXPECT_EQ(Take(command->GetState()), CommandState::Initial);
    ExpectError(command->End(), ErrorCode::InvalidState);
    ExpectError(command->CopyBuffer({}), ErrorCode::InvalidState);
    Take(command->Begin());
    ExpectError(command->Begin(), ErrorCode::InvalidState);
    Take(command->End());
    ExpectError(command->End(), ErrorCode::InvalidState);
    const auto token = Submit(*command);
    EXPECT_TRUE(Take(device->IsComplete(token)));
    Take(device->Wait(token, 0));
    EXPECT_EQ(Take(command->GetState()), CommandState::Completed);
    ICommandList *pointer = command.get();
    ExpectError(device->Submit({std::span<ICommandList *const>(&pointer, 1)}),
                ErrorCode::InvalidState);
    Take(command->Reset());
    Take(command->Begin());
    Take(command->End());
    const auto next = Submit(*command);
    EXPECT_NE(token, next);
    EXPECT_TRUE(Take(device->IsComplete(token)));
}

TEST_F(BufferRhiTest, CopiesRealBytesAndReleasesCompletedReferences)
{
    const std::array input{std::byte{1}, std::byte{2}, std::byte{3},
                           std::byte{4}};
    const auto src =
        Buffer(4, BufferUsage::CopySrc | BufferUsage::CopyDst, input);
    const std::array<std::byte, 4> zeros{};
    const auto dst =
        Buffer(4, BufferUsage::CopySrc | BufferUsage::CopyDst, zeros);
    auto command = Command();
    Take(command->Begin());
    const std::array barriers{
        BufferBarrier{src, BufferUse::Initial, BufferUse::CopySrc},
        BufferBarrier{dst, BufferUse::Initial, BufferUse::CopyDst}};
    Take(command->Barrier(barriers));
    Take(command->CopyBuffer({src, dst, 1, 2, 2}));
    Take(command->End());
    Submit(*command);
    // 设备写入后局部主机更新，必须保留复制得到的相邻字节。
    const std::array patch{std::byte{9}};
    Take(device->WriteBuffer(dst, 0, patch));
    std::array<std::byte, 4> output{};
    Take(device->ReadBuffer(dst, 0, output));
    EXPECT_EQ(output, (std::array{std::byte{9}, std::byte{0}, std::byte{2},
                                  std::byte{3}}));
    Take(device->Destroy(src));
    Take(device->Destroy(dst));
}

TEST_F(BufferRhiTest, BarrierBatchFailureDoesNotPinEarlierResources)
{
    const auto buffer = Buffer();
    auto command = Command();
    Take(command->Begin());
    const std::array barriers{
        BufferBarrier{buffer, BufferUse::Initial, BufferUse::CopySrc},
        BufferBarrier{{}, BufferUse::Initial, BufferUse::CopyDst}};
    ExpectError(command->Barrier(barriers), ErrorCode::InvalidHandle);
    Take(device->Destroy(buffer));
    Take(command->End());
    EXPECT_TRUE(Submit(*command).IsValid());
}

TEST_F(BufferRhiTest, RejectsInvalidBarrierUseAndCommandCapacity)
{
    const auto buffer = Buffer(4, BufferUsage::CopySrc);
    auto command = Command();
    Take(command->Begin());
    const std::array invalid{
        BufferBarrier{buffer, BufferUse::Initial, BufferUse::VertexRead}};
    ExpectError(command->Barrier(invalid), ErrorCode::InvalidArgument);
    const std::array unknown{
        BufferBarrier{buffer, BufferUse::Initial, static_cast<BufferUse>(255)}};
    ExpectError(command->Barrier(unknown), ErrorCode::InvalidArgument);
    const std::vector excess(
        device->GetCapabilities().maxCommandsPerList + 1,
        BufferBarrier{buffer, BufferUse::Initial, BufferUse::CopySrc});
    ExpectError(command->Barrier(excess), ErrorCode::Unsupported);
    Take(device->Destroy(buffer));
}

TEST_F(BufferRhiTest, RejectsCopyRangeOverflowAndSelfCopy)
{
    const auto src = Buffer();
    const auto dst = Buffer();
    auto command = Command();
    Take(command->Begin());
    ExpectError(command->CopyBuffer({src, dst, 0, 0, 0}),
                ErrorCode::InvalidArgument);
    ExpectError(
        command->CopyBuffer(
            {src, dst, std::numeric_limits<std::uint64_t>::max(), 0, 4}),
        ErrorCode::InvalidArgument);
    ExpectError(command->CopyBuffer({src, src, 0, 0, 1}),
                ErrorCode::Unsupported);
    Take(device->Destroy(src));
    Take(device->Destroy(dst));
}

TEST_F(BufferRhiTest, MissingBarrierIsRejectedAtSubmit)
{
    const auto src = Buffer();
    const auto dst = Buffer();
    auto command = Command();
    Take(command->Begin());
    Take(command->CopyBuffer({src, dst, 0, 0, 4}));
    Take(command->End());
    ICommandList *pointer = command.get();
    ExpectError(device->Submit({std::span<ICommandList *const>(&pointer, 1)}),
                ErrorCode::InvalidState);
    EXPECT_EQ(Take(command->GetState()), CommandState::Executable);
}

TEST_F(BufferRhiTest, SubmissionBatchFailurePreservesBytesAndStates)
{
    const std::array input{std::byte{1}, std::byte{2}, std::byte{3},
                           std::byte{4}};
    const std::array<std::byte, 4> zeros{};
    const auto src =
        Buffer(4, BufferUsage::CopySrc | BufferUsage::CopyDst, input);
    const auto dst =
        Buffer(4, BufferUsage::CopySrc | BufferUsage::CopyDst, zeros);
    auto first = Command();
    auto second = Command();
    Take(first->Begin());
    const std::array valid{
        BufferBarrier{src, BufferUse::Initial, BufferUse::CopySrc},
        BufferBarrier{dst, BufferUse::Initial, BufferUse::CopyDst}};
    Take(first->Barrier(valid));
    Take(first->CopyBuffer({src, dst, 0, 0, 4}));
    Take(first->End());
    Take(second->Begin());
    const std::array conflict{
        BufferBarrier{src, BufferUse::Initial, BufferUse::CopyDst}};
    Take(second->Barrier(conflict));
    Take(second->End());
    const std::array<ICommandList *, 2> lists{first.get(), second.get()};
    ExpectError(device->Submit({lists}), ErrorCode::InvalidState);
    EXPECT_EQ(Take(first->GetState()), CommandState::Executable);
    Take(first->Reset());
    Take(second->Reset());
    std::array<std::byte, 4> output{};
    Take(device->ReadBuffer(dst, 0, output));
    EXPECT_EQ(output, zeros);
    Take(first->Begin());
    Take(first->Barrier(valid));
    Take(first->CopyBuffer({src, dst, 0, 0, 4}));
    Take(first->End());
    Submit(*first);
    Take(device->ReadBuffer(dst, 0, output));
    EXPECT_EQ(output, input);
}

TEST_F(BufferRhiTest, OrderedListsSharePredictedState)
{
    const std::array input{std::byte{1}, std::byte{2}, std::byte{3},
                           std::byte{4}};
    const auto src =
        Buffer(4, BufferUsage::CopySrc | BufferUsage::CopyDst, input);
    const auto dst = Buffer();
    auto first = Command();
    auto second = Command();
    Take(first->Begin());
    const std::array barriers{
        BufferBarrier{src, BufferUse::Initial, BufferUse::CopySrc},
        BufferBarrier{dst, BufferUse::Initial, BufferUse::CopyDst}};
    Take(first->Barrier(barriers));
    Take(first->CopyBuffer({src, dst, 0, 0, 4}));
    Take(first->End());
    Take(second->Begin());
    // 保持相同用途，后一个列表的重叠写入应覆盖前一个列表。
    Take(second->CopyBuffer({src, dst, 0, 2, 2}));
    Take(second->End());
    const std::array<ICommandList *, 2> lists{first.get(), second.get()};
    Take(device->Submit({lists}));
    std::array<std::byte, 4> output{};
    Take(device->ReadBuffer(dst, 0, output));
    EXPECT_EQ(output, (std::array{std::byte{1}, std::byte{2}, std::byte{1},
                                  std::byte{2}}));
}

TEST_F(BufferRhiTest, SubmissionRejectsEmptyNullDuplicateAndForeignLists)
{
    ExpectError(device->Submit({}), ErrorCode::InvalidArgument);
    auto command = Command();
    Take(command->Begin());
    Take(command->End());
    const std::array<ICommandList *, 2> duplicate{command.get(), command.get()};
    ExpectError(device->Submit({duplicate}), ErrorCode::InvalidArgument);
    const std::array<ICommandList *, 1> empty{nullptr};
    ExpectError(device->Submit({empty}), ErrorCode::InvalidArgument);
    auto other = NewDevice("foreign device");
    ICommandList *pointer = command.get();
    ExpectError(other->Submit({std::span<ICommandList *const>(&pointer, 1)}),
                ErrorCode::WrongDevice);
    EXPECT_EQ(Take(command->GetState()), CommandState::Executable);
}

TEST_F(BufferRhiTest, SubmissionTokensRejectOtherDevicesAndDefaultValues)
{
    auto command = Command();
    Take(command->Begin());
    Take(command->End());
    const auto token = Submit(*command);
    auto other = NewDevice("foreign device");
    ExpectError(other->Wait(token, 0), ErrorCode::WrongDevice);
    ExpectError(device->IsComplete({}), ErrorCode::InvalidHandle);
    Take(device->Wait(token, std::numeric_limits<TimeoutNs>::max()));
}

TEST_F(BufferRhiTest, LateCommandDetectsDeviceDestructionSafely)
{
    const auto buffer = Buffer();
    auto command = Command();
    Take(command->Begin());
    const std::array barriers{
        BufferBarrier{buffer, BufferUse::Initial, BufferUse::CopySrc}};
    Take(command->Barrier(barriers));
    device.reset();
    ExpectError(command->GetState(), ErrorCode::InvalidState);
    ExpectError(command->Reset(), ErrorCode::InvalidState);
    command.reset();
}
} // namespace voryx::test

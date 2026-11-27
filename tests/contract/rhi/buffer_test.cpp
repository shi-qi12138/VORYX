#include <support/rhi_support.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace voryx::test
{
TEST_F(BufferRhiTest, UploadOwnsBytesAndReadWritePreserveRanges)
{
    std::array input{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    const auto buffer =
        Buffer(4, BufferUsage::CopySrc | BufferUsage::CopyDst, input);
    input.fill(std::byte{9});
    std::array<std::byte, 4> output{};
    Take(device->ReadBuffer(buffer, 0, output));
    EXPECT_EQ(output, (std::array{std::byte{1}, std::byte{2}, std::byte{3},
                                  std::byte{4}}));
    Take(device->WriteBuffer(buffer, 1, std::span(input).first(2)));
    Take(device->ReadBuffer(buffer, 0, output));
    EXPECT_EQ(output, (std::array{std::byte{1}, std::byte{9}, std::byte{9},
                                  std::byte{4}}));
}

TEST_F(BufferRhiTest, RejectsInvalidDescriptorsBeforeAllocation)
{
    ExpectError(device->CreateBuffer({0, BufferUsage::CopySrc, {}}),
                ErrorCode::InvalidArgument);
    ExpectError(device->CreateBuffer({4, BufferUsage::None, {}}),
                ErrorCode::InvalidArgument);
    ExpectError(
        device->CreateBuffer({4, static_cast<BufferUsage>(1u << 31), {}}),
        ErrorCode::InvalidArgument);
    ExpectError(
        device->CreateBuffer({device->GetCapabilities().maxBufferSizeBytes + 1,
                              BufferUsage::CopySrc,
                              {}}),
        ErrorCode::Unsupported);
    std::array<std::byte, 3> input{};
    ExpectError(device->CreateBuffer({4, BufferUsage::CopyDst, {}}, input),
                ErrorCode::InvalidArgument);
    ExpectError(device->CreateBuffer({3, BufferUsage::Vertex, {}}, input),
                ErrorCode::InvalidArgument);
}

TEST_F(BufferRhiTest, BoundsChecksDoNotOverflowAndAllowEmptyEndRange)
{
    const auto buffer = Buffer();
    std::array<std::byte, 4> bytes{};
    ExpectError(device->WriteBuffer(
                    buffer, std::numeric_limits<std::uint64_t>::max(), bytes),
                ErrorCode::InvalidArgument);
    ExpectError(device->ReadBuffer(buffer, 1, bytes),
                ErrorCode::InvalidArgument);
    ExpectError(device->WriteBuffer(buffer, 5, {}), ErrorCode::InvalidArgument);
    Take(device->WriteBuffer(buffer, 4, {}));
    Take(device->ReadBuffer(buffer, 4, {}));
}

TEST_F(BufferRhiTest, EnforcesTransferUsage)
{
    const auto buffer = Buffer(4, BufferUsage::Vertex);
    std::array<std::byte, 4> bytes{};
    ExpectError(device->ReadBuffer(buffer, 0, bytes),
                ErrorCode::InvalidArgument);
    ExpectError(device->WriteBuffer(buffer, 0, bytes),
                ErrorCode::InvalidArgument);
}

TEST_F(BufferRhiTest, DestroyedHandleCannotAliasReusedSlot)
{
    const auto old = Buffer();
    Take(device->Destroy(old));
    const auto replacement = Buffer();
    EXPECT_NE(old, replacement);
    // IsValid 只判断句柄结构，资源是否仍存活必须由设备校验。
    EXPECT_TRUE(old.IsValid());
    ExpectError(device->Destroy(old), ErrorCode::InvalidHandle);
    Take(device->Destroy(replacement));
    ExpectError(device->Destroy(replacement), ErrorCode::InvalidHandle);
    ExpectError(device->Destroy(BufferHandle{}), ErrorCode::InvalidHandle);
}

TEST_F(BufferRhiTest, RejectsForeignAndPreviousDeviceHandles)
{
    auto other = NewDevice("foreign device");
    const auto handle =
        Take(other->CreateBuffer({4, BufferUsage::CopySrc, {}}));
    ExpectError(device->Destroy(handle), ErrorCode::WrongDevice);
    other.reset();
    ExpectError(device->Destroy(handle), ErrorCode::WrongDevice);
}

TEST_F(BufferRhiTest, CapacityLimitAndSlotReuseAreExplicit)
{
    std::vector<BufferHandle> handles;
    for (std::uint32_t index = 0; index < device->GetCapabilities().maxBuffers;
         ++index) {
        handles.push_back(Buffer(1));
    }
    ExpectError(device->CreateBuffer({1, BufferUsage::CopySrc, {}}),
                ErrorCode::OutOfMemory);
    Take(device->Destroy(handles.front()));
    EXPECT_TRUE(Buffer(1).IsValid());
}

TEST_F(BufferRhiTest, ReferencesBlockHostAccessAndDestroyUntilReset)
{
    const auto buffer = Buffer();
    auto command = Command();
    Take(command->Begin());
    const std::array barriers{
        BufferBarrier{buffer, BufferUse::Initial, BufferUse::CopySrc}};
    Take(command->Barrier(barriers));
    std::array<std::byte, 4> bytes{};
    ExpectError(device->ReadBuffer(buffer, 0, bytes), ErrorCode::ResourceBusy);
    ExpectError(device->WriteBuffer(buffer, 0, bytes), ErrorCode::ResourceBusy);
    ExpectError(device->Destroy(buffer), ErrorCode::ResourceBusy);
    Take(command->Reset());
    Take(device->WriteBuffer(buffer, 0, bytes));
    Take(device->Destroy(buffer));
}

TEST_F(BufferRhiTest, EachCommandReleasesOnlyItsOwnReferences)
{
    const auto buffer = Buffer();
    auto first = Command();
    auto second = Command();
    const std::array barriers{
        BufferBarrier{buffer, BufferUse::Initial, BufferUse::CopySrc}};
    Take(first->Begin());
    Take(second->Begin());
    Take(first->Barrier(barriers));
    Take(second->Barrier(barriers));
    Take(first->Reset());
    ExpectError(device->Destroy(buffer), ErrorCode::ResourceBusy);
    second.reset();
    Take(device->Destroy(buffer));
}
} // namespace voryx::test

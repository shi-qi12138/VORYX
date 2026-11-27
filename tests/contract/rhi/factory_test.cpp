#include <support/rhi_support.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <thread>

namespace voryx::test
{
TEST(DeviceFactoryTest, AutoNeverSelectsNull)
{
    const auto automatic = CreateDevice({});
    if (automatic) {
        EXPECT_NE(automatic.Value()->GetInfo().backend, BackendType::Null);
    }
#if !VORYX_TEST_HAS_VULKAN && !VORYX_TEST_HAS_OPENGL
    ASSERT_FALSE(automatic);
    EXPECT_EQ(automatic.GetError().errorCode, ErrorCode::BackendNotBuilt);
    EXPECT_NE(automatic.GetError().message.find("Vulkan=BackendNotBuilt"),
              std::string::npos);
    EXPECT_NE(automatic.GetError().message.find("OpenGL=BackendNotBuilt"),
              std::string::npos);
#endif
#if !VORYX_TEST_HAS_VULKAN
    ExpectError(CreateDevice({BackendType::Vulkan, {}}),
                ErrorCode::BackendNotBuilt);
#endif
#if !VORYX_TEST_HAS_OPENGL
    ExpectError(CreateDevice({BackendType::OpenGL, {}}),
                ErrorCode::BackendNotBuilt);
#endif
    ExpectError(CreateDevice({static_cast<BackendType>(255), {}}),
                ErrorCode::InvalidArgument);
#if defined(_WIN32)
    ExpectError(CreateDevice({BackendType::DX12, {}}),
                ErrorCode::BackendNotBuilt);
#else
    ExpectError(CreateDevice({BackendType::DX12, {}}), ErrorCode::Unsupported);
#endif
}

TEST(DeviceFactoryTest, DeviceCopiesNameAndReportsSyntheticCapabilities)
{
    std::string name = "Null contract device";
    auto device = Take(CreateDevice({BackendType::Null, name}));
    name.clear();
    EXPECT_EQ(device->GetInfo().backend, BackendType::Null);
    EXPECT_EQ(device->GetInfo().debugName, "Null contract device");
    EXPECT_GT(device->GetCapabilities().maxBufferSizeBytes, 0u);
    EXPECT_GT(device->GetCapabilities().maxCommandsPerList, 0u);
}

TEST(DeviceFactoryTest, CreationFailureIdentifiesTheOperation)
{
    const auto result = CreateDevice({static_cast<BackendType>(255), {}});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.GetError().operation, "CreateDevice");
    EXPECT_EQ(result.GetError().errorCode, ErrorCode::InvalidArgument);
}

TEST_F(BufferRhiTest, WrongThreadOperationsFailWithoutChangingCommands)
{
    const auto buffer = Buffer();
    auto command = Command();
    ErrorCode writeCode = ErrorCode::InternalError;
    ErrorCode beginCode = ErrorCode::InternalError;
    std::jthread worker([&]() {
        const std::array<std::byte, 4> bytes{};
        auto write = device->WriteBuffer(buffer, 0, bytes);
        auto begin = command->Begin();
        if (!write) {
            writeCode = write.GetError().errorCode;
        }
        if (!begin) {
            beginCode = begin.GetError().errorCode;
        }
    });
    worker.join();
    EXPECT_EQ(writeCode, ErrorCode::WrongThread);
    EXPECT_EQ(beginCode, ErrorCode::WrongThread);
    EXPECT_EQ(Take(command->GetState()), CommandState::Initial);
    Take(device->Destroy(buffer));
}
} // namespace voryx::test

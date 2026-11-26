#pragma once

#include <voryx/runtime/device_factory.hpp>

#include <atomic>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <gtest/gtest.h>

namespace voryx::test
{
template<typename T>
T Take(Result<T> result)
{
    if (!result) {
        throw std::runtime_error(
            std::string(GetErrorCodeName(result.GetError().errorCode)) + " in "
            + result.GetError().operation);
    }
    return std::move(result).Value();
}

template<typename T>
void ExpectError(Result<T> result, ErrorCode expected)
{
    ASSERT_FALSE(result) << "Expected " << GetErrorCodeName(expected);
    EXPECT_EQ(result.GetError().errorCode, expected);
}

class BufferRhiTest : public testing::Test
{
protected:
    void SetUp() override
    {
        DeviceDesc desc{BackendType::Null, "contract test"};
#if VORYX_TEST_VULKAN
        desc.backend = BackendType::Vulkan;
        desc.enableValidation = true;
        desc.allowSoftwareAdapter = VORYX_TEST_SOFTWARE != 0;
        desc.logUserData = &validationErrors;
        desc.logCallback = [](LogLevel level, std::string_view message,
                              void *data) noexcept {
            if (level == LogLevel::Error) {
                static_cast<std::atomic<int> *>(data)->fetch_add(1);
                std::fwrite(message.data(), 1, message.size(), stderr);
                std::fputc('\n', stderr);
            }
        };
#endif
        device = Take(CreateDevice(desc));
#if VORYX_TEST_VULKAN
        ASSERT_TRUE(device->GetInfo().validationEnabled);
        std::printf("Vulkan device: %s; software=%s\n",
                    device->GetInfo().name.c_str(),
                    device->GetInfo().softwareAdapter ? "true" : "false");
#endif
    }

    void TearDown() override
    {
        device.reset();
        EXPECT_EQ(validationErrors.load(), 0);
    }

    BufferHandle Buffer(std::uint64_t size = 4,
                        BufferUsage usage = BufferUsage::CopySrc
                                            | BufferUsage::CopyDst,
                        ByteSpan data = {})
    {
        return Take(device->CreateBuffer({size, usage, {}}, data));
    }

    std::unique_ptr<ICommandList> Command()
    {
        return Take(device->CreateCommandList());
    }

    SubmissionToken Submit(ICommandList &command)
    {
        ICommandList *pointer = &command;
        return Take(
            device->Submit({std::span<ICommandList *const>(&pointer, 1)}));
    }

    std::atomic<int> validationErrors{0};
    std::unique_ptr<IDevice> device;
};
} // namespace voryx::test

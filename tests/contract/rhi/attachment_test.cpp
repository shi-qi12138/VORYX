#include <support/attachment_support.hpp>

#include <thread>

namespace voryx::test
{
using namespace attachment;

TEST_F(BufferRhiTest, DevicePassClearsRepeatedAttachmentsAndReadsExactBytes)
{
    const auto color = Color(*device);
    Pixels pixels{};
    ExpectError(device->ReadTexture(color, pixels), ErrorCode::InvalidState);
    auto command = Command();
    Take(command->Begin());
    Transition(*command, color, TextureUse::Initial,
               TextureUse::ColorAttachment);
    Take(command->BeginRenderPass({color, {1, 0, 0, 1}, {}, 1}));
    Take(command->EndRenderPass());
    // 同一用途下连续清除，验证 Pass 间的附件同步。
    Take(command->BeginRenderPass({color, {0, 1, 0, 1}, {}, 1}));
    Take(command->EndRenderPass());
    Transition(*command, color, TextureUse::ColorAttachment,
               TextureUse::CopySrc);
    Take(command->End());
    Submit(*command);
    ExpectError(device->ReadTexture(color, std::span(pixels).first(3)),
                ErrorCode::InvalidArgument);
    Take(device->ReadTexture(color, pixels));
    Pixel(pixels, 0, 0, {0, 255, 0, 255});
    Pixel(pixels, 63, 63, {0, 255, 0, 255});
    Take(device->Destroy(color));
}

TEST_F(BufferRhiTest, DevicePassBatchFailurePreservesStatesAndClearData)
{
    const auto color = Color(*device);
    auto first = Command();
    Take(first->Begin());
    Transition(*first, color, TextureUse::Initial, TextureUse::ColorAttachment);
    Take(first->BeginRenderPass({color, {1, 0, 0, 1}, {}, 1}));
    Take(first->EndRenderPass());
    Transition(*first, color, TextureUse::ColorAttachment, TextureUse::CopySrc);
    Take(first->End());
    auto second = Command();
    Take(second->Begin());
    Transition(*second, color, TextureUse::Initial,
               TextureUse::ColorAttachment);
    Take(second->End());
    const std::array<ICommandList *, 2> lists{first.get(), second.get()};
    ExpectError(device->Submit({lists}), ErrorCode::InvalidState);
    EXPECT_EQ(Take(first->GetState()), CommandState::Executable);
    Take(second->Reset());
    Submit(*first);
    Pixels pixels{};
    Take(device->ReadTexture(color, pixels));
    Pixel(pixels, 32, 32, {255, 0, 0, 255});
    // 同一附件在另一列表继续使用，提交时按整批顺序预测状态。
    Take(first->Reset());
    Take(first->Begin());
    Transition(*first, color, TextureUse::CopySrc, TextureUse::ColorAttachment);
    Take(first->End());
    Take(second->Begin());
    Take(second->BeginRenderPass({color, {0, 0, 1, 1}, {}, 1}));
    Take(second->EndRenderPass());
    Transition(*second, color, TextureUse::ColorAttachment,
               TextureUse::CopySrc);
    Take(second->End());
    Take(device->Submit({lists}));
    Take(device->ReadTexture(color, pixels));
    Pixel(pixels, 32, 32, {0, 0, 255, 255});
}

TEST_F(BufferRhiTest,
       DeviceTexturesValidateDescriptorsOwnershipGenerationAndThread)
{
    ExpectError(device->CreateTexture({}), ErrorCode::InvalidArgument);
    ExpectError(device->CreateTexture(
                    {{1, 1}, Format::RGBA8_UNorm, TextureUsage::Sampled, {}}),
                ErrorCode::Unsupported);
    ExpectError(device->CreateTexture({{1, 1},
                                       Format::RGBA8_UNorm,
                                       TextureUsage::ColorAttachment
                                           | TextureUsage::DepthAttachment,
                                       {}}),
                ErrorCode::InvalidArgument);
    ExpectError(device->CreateTexture(
                    {{device->GetCapabilities().maxTexture2DSize + 1, 1},
                     Format::RGBA8_UNorm,
                     TextureUsage::ColorAttachment,
                     {}}),
                ErrorCode::Unsupported);
    const auto first = Color(*device);
    auto other = Take(CreateDevice({BackendType::Null, "foreign texture"}));
    ExpectError(other->Destroy(first), ErrorCode::WrongDevice);
    std::thread worker([&]() {
        ExpectError(device->Destroy(first), ErrorCode::WrongThread);
    });
    worker.join();
    Take(device->Destroy(first));
    const auto second = Color(*device);
    EXPECT_NE(first, second);
    ExpectError(device->Destroy(first), ErrorCode::InvalidHandle);
    Take(device->Destroy(second));
}

TEST_F(BufferRhiTest, DeviceOffscreenAttachmentsRejectPresentState)
{
    const auto color = Color(*device);
    auto command = Command();
    Take(command->Begin());
    const std::array invalid{
        TextureBarrier{color, TextureUse::Initial, TextureUse::Present}};
    ExpectError(command->Barrier(invalid), ErrorCode::InvalidArgument);
    Transition(*command, color, TextureUse::Initial,
               TextureUse::ColorAttachment);
    Take(command->BeginRenderPass({color, {0, 1, 0, 1}, {}, 1}));
    Take(command->EndRenderPass());
    Transition(*command, color, TextureUse::ColorAttachment,
               TextureUse::CopySrc);
    Take(command->End());
    Take(device->Wait(Submit(*command), 0));
    Pixels pixels{};
    Take(device->ReadTexture(color, pixels));
    Pixel(pixels, 32, 32, {0, 255, 0, 255});
}
} // namespace voryx::test

#include <support/rhi_support.hpp>

#include <array>
#include <cstddef>

namespace voryx::test
{
TEST_F(BufferRhiTest, OpenGLDevicesKeepIndependentContexts)
{
    constexpr std::array firstBytes{std::byte{1}, std::byte{2}, std::byte{3},
                                    std::byte{4}};
    constexpr std::array secondBytes{std::byte{8}, std::byte{7}, std::byte{6},
                                     std::byte{5}};
    constexpr auto usage = BufferUsage::CopySrc | BufferUsage::CopyDst;
    const auto first = Buffer(firstBytes.size(), usage, firstBytes);
    DeviceDesc desc{BackendType::OpenGL, "second device"};
    desc.allowSoftwareAdapter = VORYX_TEST_SOFTWARE != 0;
    auto secondDevice = Take(CreateDevice(desc));
    const auto second = Take(secondDevice->CreateBuffer(
        {secondBytes.size(), usage, {}}, secondBytes));
    std::array<std::byte, secondBytes.size()> output{};
    ExpectError(secondDevice->ReadBuffer(first, 0, output),
                ErrorCode::WrongDevice);
    Take(device->ReadBuffer(first, 0, output));
    EXPECT_EQ(output, firstBytes);
    device.reset();
    Take(secondDevice->ReadBuffer(second, 0, output));
    EXPECT_EQ(output, secondBytes);
    Take(secondDevice->Destroy(second));
}

TEST_F(BufferRhiTest, OpenGLReportsFirstSliceCapabilities)
{
    const auto &caps = device->GetCapabilities();
    EXPECT_GT(caps.maxTexture2DSize, 0u);
    EXPECT_GT(caps.maxTextures, 0u);
    ExpectError(
        device->CreateTexture(
            {{1, 1}, Format::D32_Float, TextureUsage::DepthAttachment, {}}),
        ErrorCode::Unsupported);
    EXPECT_GT(caps.maxGraphicsPipelines, 0u);
    EXPECT_EQ(caps.maxVertexAttributes, 16u);
    EXPECT_EQ(caps.maxVertexStrideBytes, 2048u);
    EXPECT_EQ(caps.maxPendingSubmissions, 0u);
    EXPECT_EQ(caps.maxPendingWindowFrames, 0u);
    EXPECT_FALSE(device->GetInfo().validationEnabled);
    DeviceDesc validation{BackendType::OpenGL, {}};
    validation.enableValidation = true;
    ExpectError(CreateDevice(validation), ErrorCode::Unsupported);

    if (device->GetInfo().softwareAdapter) {
        ExpectError(CreateDevice({BackendType::OpenGL, {}}),
                    ErrorCode::BackendUnavailable);
    }

    auto commands = Command();
    Take(commands->Begin());
    Take(commands->End());
    const std::array batch{commands.get()};
    ExpectError(device->Submit({batch, {}, SubmitMode::Async}),
                ErrorCode::Unsupported);
    EXPECT_EQ(Take(commands->GetState()), CommandState::Executable);
    EXPECT_TRUE(Take(device->IsComplete(Take(device->Submit({batch})))));
}
TEST_F(BufferRhiTest, OpenGLColorTargetsKeepIndependentAttachments)
{
    constexpr std::size_t byteCount = 5 * 3 * 4;
    const TextureDesc desc{{5, 3},
                           Format::RGBA8_UNorm,
                           TextureUsage::ColorAttachment
                               | TextureUsage::CopySrc,
                           {}};
    const auto red = Take(device->CreateTexture(desc));
    const auto blue = Take(device->CreateTexture(desc));
    std::array<std::byte, byteCount> pixels{};
    ExpectError(device->ReadTexture(red, pixels), ErrorCode::InvalidState);

    auto commands = Command();
    Take(commands->Begin());
    const std::array initial{
        TextureBarrier{red, TextureUse::Initial, TextureUse::ColorAttachment},
        TextureBarrier{blue, TextureUse::Initial, TextureUse::ColorAttachment}};
    Take(commands->Barrier(initial));
    Take(commands->BeginRenderPass({red, {1, 0, 0, 1}, {}, 1}));
    Take(commands->EndRenderPass());
    Take(commands->BeginRenderPass({blue, {0, 0, 1, 0}, {}, 1}));
    Take(commands->EndRenderPass());
    const std::array readable{
        TextureBarrier{red, TextureUse::ColorAttachment, TextureUse::CopySrc},
        TextureBarrier{blue, TextureUse::ColorAttachment, TextureUse::CopySrc}};
    Take(commands->Barrier(readable));
    Take(commands->End());
    ExpectError(device->Destroy(red), ErrorCode::ResourceBusy);
    EXPECT_TRUE(Take(device->IsComplete(Submit(*commands))));

    Take(device->ReadTexture(red, pixels));
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        EXPECT_EQ(pixels[offset], std::byte{255});
        EXPECT_EQ(pixels[offset + 1], std::byte{0});
        EXPECT_EQ(pixels[offset + 2], std::byte{0});
        EXPECT_EQ(pixels[offset + 3], std::byte{255});
    }
    Take(device->Destroy(red));
    Take(device->ReadTexture(blue, pixels));
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        EXPECT_EQ(pixels[offset], std::byte{0});
        EXPECT_EQ(pixels[offset + 1], std::byte{0});
        EXPECT_EQ(pixels[offset + 2], std::byte{255});
        EXPECT_EQ(pixels[offset + 3], std::byte{0});
    }
    Take(device->Destroy(blue));
}
TEST_F(BufferRhiTest, OpenGLReportsAbsentBindingFeaturesAsUnsupported)
{
    const std::array entries{BindingLayoutEntry{0, BindingType::UniformBuffer,
                                                BindingVisibility::Vertex}};
    ExpectError(device->CreateSampler({}), ErrorCode::Unsupported);
    ExpectError(device->CreateBindingLayout({entries}), ErrorCode::Unsupported);
    ExpectError(device->CreateBindingLayout({}), ErrorCode::InvalidArgument);
}

#if !VORYX_TEST_HAS_VULKAN
TEST_F(BufferRhiTest, AutoSelectsOpenGLWithoutVulkan)
{
    DeviceDesc desc{BackendType::Auto, "automatic GL selection"};
    desc.allowSoftwareAdapter = VORYX_TEST_SOFTWARE != 0;
    auto selected = Take(CreateDevice(desc));
    EXPECT_EQ(selected->GetInfo().backend, BackendType::OpenGL);
    desc.enableValidation = true;
    const auto rejected = CreateDevice(desc);
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.GetError().errorCode, ErrorCode::Unsupported);
    EXPECT_NE(rejected.GetError().message.find("OpenGL=Unsupported"),
              std::string::npos);
}
#endif
} // namespace voryx::test

#pragma once

#include "vulkan_context.hpp"

#include <voryx/platform/native_window.hpp>
#include <voryx/rhi/presenter.hpp>

#include <thread>

namespace voryx::vulkan_backend
{
struct Swapchain;

class TriangleDraw;
class PresenterResources;

class Presenter final : public IPresenter
{
public:
    using ReadExtent = VkExtent2D (*)(NativeWindowDesc);
    Presenter(std::shared_ptr<Context> context, std::unique_ptr<IDevice> device,
              NativeWindowDesc window, ReadExtent readExtent);
    ~Presenter() override;
    IDevice &GetDevice() noexcept override;
    Result<void> Shutdown(TimeoutNs timeoutNs) noexcept override;
    Result<FrameInfo> AcquireFrame(TimeoutNs timeoutNs) noexcept override;
    Result<void> Present(FrameToken frame,
                         SubmissionToken submission) noexcept override;
    Result<void> CancelFrame(FrameToken frame) noexcept override;
    Result<void> ClearAndPresent(ClearColor color) noexcept override;
    Result<void> SetFramebufferExtent(Extent2D extent) noexcept override;
    Result<void>
    SetTriangleList(const TriangleListDesc &desc) noexcept override;
    Result<void> DrawAndPresent(ClearColor background) noexcept override;
    Result<void> SetTexturedTriangleList(
        const TexturedTriangleListDesc &desc) noexcept override;
    Result<void>
    SetDepthTriangleList(const DepthTriangleListDesc &desc) noexcept override;
    Result<void> SetIndexedTriangleList(
        const IndexedTriangleListDesc &desc) noexcept override;

    Result<Texture2DHandle>
    CreateTexture2D(const Rgba8TextureDesc &desc) noexcept override;
    Result<SamplerHandle>
    CreateSampler(const SamplerDesc &desc) noexcept override;
    Result<TextureBindingHandle>
    CreateTextureBinding(const TextureBindingDesc &desc) noexcept override;
    Result<void> SetBoundTexturedTriangleList(
        const BoundTexturedTriangleListDesc &desc) noexcept override;
    Result<UniformBufferHandle>
    CreateUniformBuffer(ByteSpan bytes) noexcept override;
    Result<BindingLayoutHandle>
    CreateBindingLayout(const BindingLayoutDesc &desc) noexcept override;
    Result<BindingSetHandle>
    CreateBindingSet(const BindingSetDesc &desc) noexcept override;
    Result<void> SetResourceTriangleList(
        const ResourceTriangleListDesc &desc) noexcept override;
    Result<void> Destroy(UniformBufferHandle handle) noexcept override;
    Result<void> Destroy(BindingLayoutHandle handle) noexcept override;
    Result<void> Destroy(BindingSetHandle handle) noexcept override;
    Result<void> ResetDraw() noexcept override;
    Result<void> Destroy(TextureBindingHandle handle) noexcept override;
    Result<void> Destroy(Texture2DHandle handle) noexcept override;
    Result<void> Destroy(SamplerHandle handle) noexcept override;

private:
    template<typename T, typename Action>
    Result<T> AccessResources(std::string_view operation,
                              Action &&action) noexcept;
    template<typename Desc>
    Result<void> ConfigureDraw(const Desc &desc,
                               std::string_view operation) noexcept;
    Result<void> PresentFrame(ClearColor color, bool drawTriangle,
                              std::string_view operation) noexcept;
    VkExtent2D WindowExtent() const;
    void DestroySurface() noexcept;
    void ResetChain() noexcept;
    void ResetFailedFrame(VkResult result) noexcept;
    std::shared_ptr<Context> contextOwner;
    Context &context;
    std::unique_ptr<IDevice> device;
    const NativeWindowDesc window;
    const ReadExtent readExtent;
    Extent2D framebufferExtent;
    std::unique_ptr<PresenterResources> resources;
    std::unique_ptr<TriangleDraw> triangle;
    std::unique_ptr<Swapchain> chain;
    std::thread::id owner;
    bool failed = false;
    bool closing = false;
};

Result<std::unique_ptr<IPresenter>>
CreateSurfacePresenter(std::uint64_t identity, const DeviceDesc &device,
                       NativeWindowDesc window, const char *platformExtension,
                       VkSurfaceKHR (*makeSurface)(VkInstance, const void *),
                       Presenter::ReadExtent readExtent) noexcept;
} // namespace voryx::vulkan_backend

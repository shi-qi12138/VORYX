#pragma once

#include "buffer_state.hpp"

#include <voryx/rhi/device.hpp>

#include <memory>

namespace voryx::rhi_detail
{
class BufferDevice final : public IDevice
{
public:
    explicit BufferDevice(std::shared_ptr<State> state);
    ~BufferDevice() override;
    const DeviceInfo &GetInfo() const noexcept override;
    const DeviceCapabilities &GetCapabilities() const noexcept override;
    Result<BufferHandle> CreateBuffer(const BufferDesc &desc,
                                      ByteSpan initialData) noexcept override;
    Result<void> WriteBuffer(BufferHandle buffer, std::uint64_t offsetBytes,
                             ByteSpan data) noexcept override;
    Result<void> ReadBuffer(BufferHandle buffer, std::uint64_t offsetBytes,
                            MutableByteSpan output) noexcept override;
    Result<void> Destroy(BufferHandle buffer) noexcept override;
    Result<PipelineHandle>
    CreateGraphicsPipeline(const GraphicsPipelineDesc &desc) noexcept override;
    Result<void> Destroy(PipelineHandle pipeline) noexcept override;
    Result<TextureHandle>
    CreateTexture(const TextureDesc &desc) noexcept override;
    Result<void> ReadTexture(TextureHandle texture,
                             MutableByteSpan output) noexcept override;
    Result<void> Destroy(TextureHandle texture) noexcept override;
    Result<TextureHandle>
    CreateTexture2D(const Rgba8TextureDesc &desc) noexcept override;
    Result<DeviceSamplerHandle>
    CreateSampler(const SamplerDesc &desc) noexcept override;
    Result<DeviceBindingLayoutHandle>
    CreateBindingLayout(const BindingLayoutDesc &desc) noexcept override;
    Result<DeviceBindingSetHandle>
    CreateBindingSet(const DeviceBindingSetDesc &desc) noexcept override;
    Result<void> Destroy(DeviceSamplerHandle handle) noexcept override;
    Result<void> Destroy(DeviceBindingLayoutHandle handle) noexcept override;
    Result<void> Destroy(DeviceBindingSetHandle handle) noexcept override;
    Result<std::unique_ptr<ICommandList>> CreateCommandList() noexcept override;
    Result<SubmissionToken> Submit(const SubmitDesc &desc) noexcept override;
    Result<bool> IsComplete(SubmissionToken token) const noexcept override;
    Result<void> Wait(SubmissionToken token,
                      TimeoutNs timeoutNs) noexcept override;

    // WSI 使用的私有设备入口，原生图像由窗口交换链负责回收。
    Result<void> CheckAcquireFrame() const noexcept;
    Result<FrameInfo>
    PublishFrame(Extent2D extent, Format format,
                 std::unique_ptr<TextureStorage> image) noexcept;
    Result<void> CheckPresent(FrameToken frame,
                              SubmissionToken submission) const noexcept;
    Result<void> CheckCancel(FrameToken frame) const noexcept;
    [[nodiscard]] bool IsFrameAsynchronous() const noexcept;
    void ConsumeFrame() noexcept;
    void DrainFrameSubmissions() noexcept;
    Result<void> PrepareShutdown(TimeoutNs timeoutNs) noexcept;
    void FinishShutdown() noexcept;
    void MarkLost() noexcept;

private:
    Result<void> CheckFinishFrame(FrameToken frame) const noexcept;
    Result<TextureHandle> CreateTextureImpl(const TextureDesc &desc,
                                            const Rgba8TextureDesc *upload);
    std::shared_ptr<State> state;
};
} // namespace voryx::rhi_detail

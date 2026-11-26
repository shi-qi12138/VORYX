#pragma once

#include <voryx/core/result.hpp>
#include <voryx/rhi/command_list.hpp>
#include <voryx/rhi/pipeline.hpp>
#include <voryx/rhi/texture.hpp>
#include <voryx/rhi/types.hpp>

#include <memory>

namespace voryx
{
class IDevice
{
public:
    /// 设备和命令对象必须在创建设备的线程析构。
    virtual ~IDevice() = default;
    IDevice(const IDevice &) = delete;
    IDevice &operator=(const IDevice &) = delete;

    /// 返回不可变借用；调用方必须保证设备仍存活。
    virtual const DeviceInfo &GetInfo() const noexcept = 0;
    virtual const DeviceCapabilities &GetCapabilities() const noexcept = 0;
    /// 非空初始数据必须覆盖整个缓冲区，且资源包含 CopyDst 用途。
    virtual Result<BufferHandle>
    CreateBuffer(const BufferDesc &desc,
                 ByteSpan initialData = {}) noexcept = 0;
    /// 返回前复制完成；未提交命令引用会导致 ResourceBusy。
    virtual Result<void> WriteBuffer(BufferHandle buffer,
                                     std::uint64_t offsetBytes,
                                     ByteSpan data) noexcept = 0;
    /// 要求 CopySrc 用途，复制到调用方存储，不返回内部地址。
    virtual Result<void> ReadBuffer(BufferHandle buffer,
                                    std::uint64_t offsetBytes,
                                    MutableByteSpan output) noexcept = 0;
    /// 成功即逻辑失效；重复销毁或代数不匹配返回 InvalidHandle。
    virtual Result<void> Destroy(BufferHandle buffer) noexcept = 0;
    /// 创建独立不可变管线。深度启用时采用 D32_Float，输入在返回前复制或编译。
    virtual Result<PipelineHandle>
    CreateGraphicsPipeline(const GraphicsPipelineDesc &desc) noexcept = 0;
    virtual Result<void> Destroy(PipelineHandle pipeline) noexcept = 0;
    virtual Result<TextureHandle>
    CreateTexture(const TextureDesc &desc) noexcept = 0;
    /// 同步读回整个 RGBA8 纹理，要求 CopySrc
    /// 状态、用途和有效内容，输出长度恰好匹配。
    virtual Result<void> ReadTexture(TextureHandle texture,
                                     MutableByteSpan output) noexcept = 0;
    virtual Result<void> Destroy(TextureHandle texture) noexcept = 0;
    /// 同步上传不可变 RGBA8 纹理，成功后为 ShaderRead。
    virtual Result<TextureHandle>
    CreateTexture2D(const Rgba8TextureDesc &desc) noexcept = 0;
    virtual Result<DeviceSamplerHandle>
    CreateSampler(const SamplerDesc &desc) noexcept = 0;
    virtual Result<DeviceBindingLayoutHandle>
    CreateBindingLayout(const BindingLayoutDesc &desc) noexcept = 0;
    virtual Result<DeviceBindingSetHandle>
    CreateBindingSet(const DeviceBindingSetDesc &desc) noexcept = 0;
    virtual Result<void> Destroy(DeviceSamplerHandle handle) noexcept = 0;
    virtual Result<void> Destroy(DeviceBindingLayoutHandle handle) noexcept = 0;
    virtual Result<void> Destroy(DeviceBindingSetHandle handle) noexcept = 0;
    virtual Result<std::unique_ptr<ICommandList>>
    CreateCommandList() noexcept = 0;
    /// 整批验证通过后执行；Null 同步完成，不代表其他后端也立即完成。
    virtual Result<SubmissionToken> Submit(const SubmitDesc &desc) noexcept = 0;
    virtual Result<bool> IsComplete(SubmissionToken token) const noexcept = 0;
    virtual Result<void> Wait(SubmissionToken token,
                              TimeoutNs timeoutNs) noexcept = 0;

protected:
    IDevice() = default;
};
} // namespace voryx

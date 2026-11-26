#pragma once

#include "vulkan_mapped_buffer.hpp"

#include <buffer_backend.hpp>

namespace voryx::vulkan_backend
{
bool ValidTexture(const Rgba8TextureDesc &desc) noexcept;

/// 持有不可变采样图像及其视图。依赖所属 Presenter 的 Context。
class SampledTexture final : public rhi_detail::TextureStorage
{
public:
    explicit SampledTexture(Context &context) noexcept;
    ~SampledTexture() override;
    SampledTexture(const SampledTexture &) = delete;
    SampledTexture &operator=(const SampledTexture &) = delete;

    Result<void> Initialize(const Rgba8TextureDesc &desc);
    VkImageView view = VK_NULL_HANDLE;

private:
    void AllocateImage(Extent2D extent);
    Result<void> Upload(const Rgba8TextureDesc &desc);
    void CreateView();

    Context &context;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};
} // namespace voryx::vulkan_backend

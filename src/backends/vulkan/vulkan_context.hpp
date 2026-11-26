#pragma once

#include <voryx/core/error.hpp>
#include <voryx/core/log.hpp>
#include <voryx/rhi/types.hpp>

#include <vulkan/vulkan.h>

#include <memory>
#include <span>

namespace voryx::vulkan_backend
{
struct NativeFailure
{
    VkResult result;
};

void Check(VkResult result);
Error NativeError(VkResult result, std::string_view operation) noexcept;

struct SurfaceSource
{
    std::span<const char *const> extensions;
    VkSurfaceKHR (*create)(VkInstance, const void *);
    const void *window;
    std::span<const char *const> deviceExtensions;
    const void *deviceFeatures;
    bool (*supports)(VkPhysicalDevice);
};

struct Context
{
    explicit Context(const DeviceDesc &desc);
    ~Context();
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    void Initialize(const DeviceDesc &desc,
                    const SurfaceSource *source = nullptr);

    /// 调用方已排空工作并释放所有设备子对象。
    void DestroyDevice() noexcept;

    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queueFamily = 0;
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    LogSink log;
};

/// 为指定图像分配兼容内存，优先设备本地类型。调用方负责绑定和释放。
VkDeviceMemory AllocateImageMemory(const Context &context, VkImage image);
} // namespace voryx::vulkan_backend

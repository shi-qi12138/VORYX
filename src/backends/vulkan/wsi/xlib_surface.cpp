#include "vulkan_presenter.hpp"

#include <presenter_factory.hpp>

#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>

namespace voryx::detail
{
namespace
{
VkSurfaceKHR MakeSurface(VkInstance instance, const void *data)
{
    const auto &window = *static_cast<const NativeWindowDesc *>(data);
    VkXlibSurfaceCreateInfoKHR create{};
    create.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    create.dpy = reinterpret_cast<Display *>(window.display);
    create.window = static_cast<Window>(window.window);
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    vulkan_backend::Check(
        vkCreateXlibSurfaceKHR(instance, &create, nullptr, &surface));
    return surface;
}

VkExtent2D ReadClientExtent(NativeWindowDesc window)
{
    XWindowAttributes attributes{};
    if (XGetWindowAttributes(reinterpret_cast<Display *>(window.display),
                             static_cast<Window>(window.window), &attributes)
        == 0) {
        throw vulkan_backend::NativeFailure{VK_ERROR_SURFACE_LOST_KHR};
    }
    // 窗口管理器最小化时通常取消映射。父窗口不可见时同样暂停。
    if (attributes.map_state != IsViewable || attributes.width <= 0
        || attributes.height <= 0) {
        return {};
    }
    return {static_cast<std::uint32_t>(attributes.width),
            static_cast<std::uint32_t>(attributes.height)};
}
} // namespace

Result<std::unique_ptr<IPresenter>>
CreateXlibVulkanPresenter(std::uint64_t identity, const DeviceDesc &device,
                          NativeWindowDesc window) noexcept
{
    return vulkan_backend::CreateSurfacePresenter(
        identity, device, window, VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
        MakeSurface, ReadClientExtent);
}
} // namespace voryx::detail

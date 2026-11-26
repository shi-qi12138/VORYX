#include "vulkan_presenter.hpp"

#include <presenter_factory.hpp>

#include <vulkan/vulkan_wayland.h>

namespace voryx::detail
{
namespace
{
VkSurfaceKHR MakeSurface(VkInstance instance, const void *data)
{
    const auto &window = *static_cast<const NativeWindowDesc *>(data);
    VkWaylandSurfaceCreateInfoKHR create{};
    create.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    create.display = reinterpret_cast<wl_display *>(window.display);
    create.surface = reinterpret_cast<wl_surface *>(window.window);
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    vulkan_backend::Check(
        vkCreateWaylandSurfaceKHR(instance, &create, nullptr, &surface));
    return surface;
}
} // namespace

Result<std::unique_ptr<IPresenter>>
CreateWaylandVulkanPresenter(std::uint64_t identity, const DeviceDesc &device,
                             NativeWindowDesc window) noexcept
{
    // Wayland 的尺寸来自宿主确认过的 configure，不能通过表面查询。
    return vulkan_backend::CreateSurfacePresenter(
        identity, device, window, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
        MakeSurface, nullptr);
}
} // namespace voryx::detail

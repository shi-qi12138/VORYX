#define VK_USE_PLATFORM_WIN32_KHR
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "vulkan_presenter.hpp"

#include <error_context.hpp>
#include <presenter_factory.hpp>

namespace voryx::detail
{
namespace
{
VkSurfaceKHR MakeSurface(VkInstance instance, const void *data)
{
    const auto &window = *static_cast<const NativeWindowDesc *>(data);
    VkWin32SurfaceCreateInfoKHR create{};
    create.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create.hwnd = reinterpret_cast<HWND>(window.window);
    create.hinstance = reinterpret_cast<HINSTANCE>(window.instance);
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    vulkan_backend::Check(
        vkCreateWin32SurfaceKHR(instance, &create, nullptr, &surface));
    return surface;
}

VkExtent2D ReadClientExtent(NativeWindowDesc window)
{
    const auto hwnd = reinterpret_cast<HWND>(window.window);
    RECT bounds{};
    if (!GetClientRect(hwnd, &bounds)) {
        throw vulkan_backend::NativeFailure{VK_ERROR_SURFACE_LOST_KHR};
    }
    if (IsIconic(hwnd) || bounds.right <= bounds.left
        || bounds.bottom <= bounds.top) {
        return {};
    }
    return {static_cast<std::uint32_t>(bounds.right - bounds.left),
            static_cast<std::uint32_t>(bounds.bottom - bounds.top)};
}
} // namespace

Result<std::unique_ptr<IPresenter>>
CreateWin32VulkanPresenter(std::uint64_t identity, const DeviceDesc &device,
                           NativeWindowDesc window) noexcept
{
    using Outcome = Result<std::unique_ptr<IPresenter>>;
    const auto fail = [](ErrorCode code) {
        return Outcome::Failure(
            WithOperation({code, {}, {}, {}}, "CreatePresenter"));
    };
    const auto hwnd = reinterpret_cast<HWND>(window.window);
    DWORD process = 0;
    const auto thread = GetWindowThreadProcessId(hwnd, &process);
    if (thread == 0 || process != GetCurrentProcessId()
        || static_cast<std::uintptr_t>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE))
               != window.instance) {
        return fail(ErrorCode::SurfaceIncompatible);
    }
    if (thread != GetCurrentThreadId()) {
        return fail(ErrorCode::WrongThread);
    }
    return vulkan_backend::CreateSurfacePresenter(
        identity, device, window, VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        MakeSurface, ReadClientExtent);
}
} // namespace voryx::detail

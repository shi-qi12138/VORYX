#include <voryx/runtime/presenter.hpp>

#include "device_identity.hpp"

#include <error_context.hpp>

#if VORYX_HAS_WIN32_PRESENTER || VORYX_HAS_XLIB_PRESENTER                      \
    || VORYX_HAS_WAYLAND_PRESENTER
#include <presenter_factory.hpp>
#endif

namespace voryx
{
Result<std::unique_ptr<IPresenter>>
CreatePresenter(const PresenterDesc &desc) noexcept
{
    const auto fail = [](ErrorCode code) {
        return Result<std::unique_ptr<IPresenter>>::Failure(
            detail::WithOperation({code, {}, {}, {}}, "CreatePresenter"));
    };
    const bool win32 = desc.window.kind == NativeWindowDesc::Kind::Win32;
    const bool xlib = desc.window.kind == NativeWindowDesc::Kind::Xlib;
    const bool wayland = desc.window.kind == NativeWindowDesc::Kind::Wayland;
    if ((!win32 && !xlib && !wayland) || desc.window.window == 0
        || (win32 && desc.window.instance == 0)
        || ((xlib || wayland) && desc.window.display == 0)
        || desc.device.backend > BackendType::DX12) {
        return fail(ErrorCode::InvalidArgument);
    }
    if (desc.device.backend != BackendType::Auto
        && desc.device.backend != BackendType::Vulkan) {
        return fail(ErrorCode::Unsupported);
    }
#if VORYX_HAS_WIN32_PRESENTER || VORYX_HAS_XLIB_PRESENTER                      \
    || VORYX_HAS_WAYLAND_PRESENTER
    const auto identity = detail::AllocateDeviceIdentity();
    if (identity == 0) {
        return fail(ErrorCode::InternalError);
    }
#endif
    if (win32) {
#if VORYX_HAS_WIN32_PRESENTER
        return detail::CreateWin32VulkanPresenter(identity, desc.device,
                                                  desc.window);
#elif VORYX_PLATFORM_WINDOWS
        return fail(ErrorCode::BackendNotBuilt);
#else
        return fail(ErrorCode::SurfaceIncompatible);
#endif
    }
    if (xlib) {
#if VORYX_HAS_XLIB_PRESENTER
        return detail::CreateXlibVulkanPresenter(identity, desc.device,
                                                 desc.window);
#elif VORYX_PLATFORM_LINUX
        return fail(ErrorCode::BackendNotBuilt);
#else
        return fail(ErrorCode::SurfaceIncompatible);
#endif
    }
#if VORYX_HAS_WAYLAND_PRESENTER
    return detail::CreateWaylandVulkanPresenter(identity, desc.device,
                                                desc.window);
#elif VORYX_PLATFORM_LINUX
    return fail(ErrorCode::BackendNotBuilt);
#else
    return fail(ErrorCode::SurfaceIncompatible);
#endif
}
} // namespace voryx

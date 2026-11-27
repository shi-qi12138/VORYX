#include <voryx/runtime/presenter.hpp>

#include "backend_selection.hpp"
#include "device_identity.hpp"

#include <error_context.hpp>

#if VORYX_HAS_WIN32_PRESENTER || VORYX_HAS_XLIB_PRESENTER                      \
    || VORYX_HAS_WAYLAND_PRESENTER
#include <presenter_factory.hpp>
#endif

namespace voryx
{
namespace
{
bool ValidWindow(const NativeWindowDesc &window) noexcept
{
    using Kind = NativeWindowDesc::Kind;
    return window.window != 0
           && ((window.kind == Kind::Win32 && window.instance != 0)
               || ((window.kind == Kind::Xlib || window.kind == Kind::Wayland)
                   && window.display != 0));
}

template<typename Presenter>
Result<std::unique_ptr<Presenter>>
PresenterFailure(ErrorCode code, std::string_view operation) noexcept
{
    return Result<std::unique_ptr<Presenter>>::Failure(
        detail::WithOperation({code, {}, {}, {}}, operation));
}

Result<std::unique_ptr<IPresenter>>
CreateVulkanPresenter(const PresenterDesc &desc,
                      std::string_view operation) noexcept
{
    const auto fail = [operation](ErrorCode code) {
        return PresenterFailure<IPresenter>(code, operation);
    };
    const bool win32 = desc.window.kind == NativeWindowDesc::Kind::Win32;
    const bool xlib = desc.window.kind == NativeWindowDesc::Kind::Xlib;
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
} // namespace

Result<std::unique_ptr<IPresenter>>
CreatePresenter(const PresenterDesc &desc) noexcept
{
    constexpr std::string_view operation = "CreatePresenter";
    if (!ValidWindow(desc.window) || desc.device.backend > BackendType::DX12) {
        return PresenterFailure<IPresenter>(ErrorCode::InvalidArgument,
                                            operation);
    }
    if (desc.device.backend != BackendType::Auto
        && desc.device.backend != BackendType::Vulkan) {
        return PresenterFailure<IPresenter>(ErrorCode::Unsupported, operation);
    }
    return CreateVulkanPresenter(desc, operation);
}

Result<std::unique_ptr<IFramePresenter>>
CreateFramePresenter(const PresenterDesc &desc) noexcept
{
    constexpr std::string_view operation = "CreateFramePresenter";
    using FramePresenterPtr = std::unique_ptr<IFramePresenter>;
    if (!ValidWindow(desc.window)) {
        return PresenterFailure<IFramePresenter>(ErrorCode::InvalidArgument,
                                                 operation);
    }
    return detail::SelectBackend<FramePresenterPtr>(
        desc.device.backend, operation,
        [&](BackendType backend) -> Result<FramePresenterPtr> {
            if (backend == BackendType::Vulkan) {
                auto candidate = desc;
                candidate.device.backend = backend;
                auto presenter = CreateVulkanPresenter(candidate, operation);
                if (!presenter) {
                    return Result<FramePresenterPtr>::Failure(
                        std::move(presenter).GetError());
                }
                return Result<FramePresenterPtr>::Success(
                    std::move(presenter).Value());
            }
            if (backend == BackendType::Null
                || (backend == BackendType::DX12 && !VORYX_PLATFORM_WINDOWS)) {
                return PresenterFailure<IFramePresenter>(ErrorCode::Unsupported,
                                                         operation);
            }
            return PresenterFailure<IFramePresenter>(ErrorCode::BackendNotBuilt,
                                                     operation);
        });
}
} // namespace voryx

#include <voryx/runtime/presenter.hpp>

#include "triangle_sample.hpp"
#include "window_frame_sample.hpp"

#include "xdg-shell-client.h"

#include <poll.h>
#include <wayland-client.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <utility>

namespace
{
struct HostWindow
{
    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    wl_compositor *compositor = nullptr;
    wl_surface *surface = nullptr;
    xdg_wm_base *shell = nullptr;
    xdg_surface *shellSurface = nullptr;
    xdg_toplevel *toplevel = nullptr;
    voryx::Extent2D pending{640, 420};
    voryx::Extent2D configured{};
    bool closed = false;

    ~HostWindow()
    {
        if (toplevel != nullptr) {
            xdg_toplevel_destroy(toplevel);
        }
        if (shellSurface != nullptr) {
            xdg_surface_destroy(shellSurface);
        }
        if (surface != nullptr) {
            wl_surface_destroy(surface);
        }
        if (shell != nullptr) {
            xdg_wm_base_destroy(shell);
        }
        if (compositor != nullptr) {
            wl_compositor_destroy(compositor);
        }
        if (registry != nullptr) {
            wl_registry_destroy(registry);
        }
        if (display != nullptr) {
            wl_display_disconnect(display);
        }
    }

    bool Open()
    {
        display = wl_display_connect(nullptr);
        if (display == nullptr) {
            std::fputs("Cannot connect to Wayland compositor. Check "
                       "WAYLAND_DISPLAY.\n",
                       stderr);
            return false;
        }
        registry = wl_display_get_registry(display);
        if (registry == nullptr) {
            return false;
        }
        static const wl_registry_listener globals{
            [](void *data, wl_registry *source, std::uint32_t name,
               const char *interface, std::uint32_t) {
                auto &host = *static_cast<HostWindow *>(data);
                if (std::strcmp(interface, wl_compositor_interface.name) == 0
                    && host.compositor == nullptr) {
                    host.compositor =
                        static_cast<wl_compositor *>(wl_registry_bind(
                            source, name, &wl_compositor_interface, 1));
                } else if (std::strcmp(interface, xdg_wm_base_interface.name)
                               == 0
                           && host.shell == nullptr) {
                    host.shell = static_cast<xdg_wm_base *>(wl_registry_bind(
                        source, name, &xdg_wm_base_interface, 1));
                }
            },
            [](void *, wl_registry *, std::uint32_t) {
            },
        };
        if (wl_registry_add_listener(registry, &globals, this) != 0
            || wl_display_roundtrip(display) < 0 || compositor == nullptr
            || shell == nullptr) {
            return false;
        }
        static const xdg_wm_base_listener ping{
            [](void *, xdg_wm_base *base, std::uint32_t serial) {
                xdg_wm_base_pong(base, serial);
            },
        };
        surface = wl_compositor_create_surface(compositor);
        if (surface == nullptr
            || xdg_wm_base_add_listener(shell, &ping, this) != 0) {
            return false;
        }
        shellSurface = xdg_wm_base_get_xdg_surface(shell, surface);
        if (shellSurface == nullptr) {
            return false;
        }
        toplevel = xdg_surface_get_toplevel(shellSurface);
        if (toplevel == nullptr) {
            return false;
        }
        static const xdg_surface_listener surfaceEvents{
            [](void *data, xdg_surface *configuredSurface,
               std::uint32_t serial) {
                auto &host = *static_cast<HostWindow *>(data);
                xdg_surface_ack_configure(configuredSurface, serial);
                // 只在 xdg_surface.configure 提交整组窗口配置。
                host.configured = host.pending;
            },
        };
        static const xdg_toplevel_listener topEvents = [] {
            xdg_toplevel_listener listeners{};
            listeners.configure = [](void *data, xdg_toplevel *,
                                     std::int32_t width, std::int32_t height,
                                     wl_array *) {
                auto &host = *static_cast<HostWindow *>(data);
                host.pending = {
                    width > 0 ? static_cast<std::uint32_t>(width) : 640u,
                    height > 0 ? static_cast<std::uint32_t>(height) : 420u};
            };
            listeners.close = [](void *data, xdg_toplevel *) {
                static_cast<HostWindow *>(data)->closed = true;
            };
            return listeners;
        }();
        if (xdg_surface_add_listener(shellSurface, &surfaceEvents, this) != 0
            || xdg_toplevel_add_listener(toplevel, &topEvents, this) != 0) {
            return false;
        }
        xdg_toplevel_set_title(toplevel, "VORYX Vulkan Wayland clear");
        xdg_toplevel_set_app_id(toplevel, "org.voryx.wayland-clear");
        // 首次提交不附缓冲，收到并确认 configure 后才允许 Vulkan 呈现。
        wl_surface_commit(surface);
        for (int attempt = 0; attempt < 30 && configured.width == 0;
             ++attempt) {
            if (!Pump(100) || closed) {
                return false;
            }
        }
        return configured.width != 0;
    }

    bool Pump(int timeoutMs) const
    {
        while (wl_display_prepare_read(display) != 0) {
            if (wl_display_dispatch_pending(display) < 0) {
                return false;
            }
        }
        const int flushed = wl_display_flush(display);
        if (flushed < 0 && errno != EAGAIN) {
            wl_display_cancel_read(display);
            return false;
        }
        pollfd events{wl_display_get_fd(display),
                      static_cast<short>(POLLIN | (flushed < 0 ? POLLOUT : 0)),
                      0};
        const int result = poll(&events, 1, timeoutMs);
        const bool interrupted = result < 0 && errno == EINTR;
        if (result > 0 && (events.revents & POLLIN) != 0) {
            if (wl_display_read_events(display) < 0) {
                return false;
            }
        } else {
            wl_display_cancel_read(display);
        }
        if ((result < 0 && !interrupted)
            || (events.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            return false;
        }
        return wl_display_dispatch_pending(display) >= 0;
    }
};

void PrintError(const voryx::Error &error)
{
    const auto name = voryx::GetErrorCodeName(error.errorCode);
    std::fprintf(stderr, "%s: %.*s (%lld)\n", error.operation.c_str(),
                 static_cast<int>(name.size()), name.data(),
                 static_cast<long long>(error.backendCode.value_or(0)));
}

bool Retryable(voryx::ErrorCode code)
{
    return code == voryx::ErrorCode::SwapchainOutOfDate
           || code == voryx::ErrorCode::Timeout;
}

bool SetExtent(voryx::IPresenter &presenter, voryx::Extent2D extent)
{
    const auto result = presenter.SetFramebufferExtent(extent);
    if (!result) {
        PrintError(result.GetError());
        return false;
    }
    return true;
}

bool PresentFrames(const HostWindow &host, voryx::IPresenter &presenter,
                   voryx::ClearColor color, bool triangle = false)
{
    int completed = 0;
    for (int attempt = 0; attempt < 24 && completed < 3; ++attempt) {
        if (!host.Pump(0) || host.closed
            || !SetExtent(presenter, host.configured)) {
            return false;
        }
        const auto result = voryx::sample::Present(presenter, color, triangle);
        if (result) {
            ++completed;
        } else if (!Retryable(result.GetError().errorCode)) {
            PrintError(result.GetError());
            return false;
        }
    }
    return completed == 3;
}

bool Verify(const HostWindow &host, voryx::IPresenter &presenter)
{
    const auto initial = presenter.ClearAndPresent({});
    if (initial
        || initial.GetError().errorCode != voryx::ErrorCode::SurfaceUnavailable
        || !PresentFrames(host, presenter, {0.15f, 0.35f, 0.8f, 1.0f})) {
        return false;
    }
    if (!voryx::sample::VerifyTriangleSetup(presenter)
        || !PresentFrames(host, presenter, {0.1f, 0.1f, 0.1f, 1.0f}, true)
        || !voryx::sample::VerifyTextureSetup(presenter)
        || !PresentFrames(host, presenter, {0.1f, 0.1f, 0.1f, 1.0f}, true)
        || !voryx::sample::VerifyDepthSetup(presenter)
        || !PresentFrames(host, presenter, {0.1f, 0.1f, 0.1f, 1.0f}, true)
        || !voryx::sample::VerifyIndexedSetup(presenter)
        || !PresentFrames(host, presenter, {0.1f, 0.1f, 0.1f, 1.0f}, true)
        || !voryx::sample::VerifyBoundTextureSetup(presenter)
        || !PresentFrames(host, presenter, {0.1f, 0.1f, 0.1f, 1.0f}, true)
        || !voryx::sample::VerifyResourceSetup(presenter)
        || !PresentFrames(host, presenter, {0.1f, 0.1f, 0.1f, 1.0f}, true)
        || !voryx::sample::VerifyWindowFrames(presenter)) {
        return false;
    }
    for (const auto maximize : {true, false}) {
        const auto previous = host.configured;
        if (maximize) {
            xdg_toplevel_set_maximized(host.toplevel);
        } else {
            xdg_toplevel_unset_maximized(host.toplevel);
        }
        for (int attempt = 0;
             attempt < 30 && host.configured.width == previous.width
             && host.configured.height == previous.height;
             ++attempt) {
            if (!host.Pump(100) || host.closed) {
                return false;
            }
        }
        if ((host.configured.width == previous.width
             && host.configured.height == previous.height)
            || !SetExtent(presenter, host.configured)
            || !voryx::sample::VerifyResizedAsyncFrame(presenter,
                                                       host.configured)) {
            std::fputs("Wayland resize did not present at the new size.\n",
                       stderr);
            return false;
        }
    }
    if (!SetExtent(presenter, {})) {
        return false;
    }
    const auto paused = presenter.DrawAndPresent({});
    if (paused
        || paused.GetError().errorCode != voryx::ErrorCode::SurfaceUnavailable
        || !voryx::sample::VerifyWindowFramePaused(presenter)) {
        return false;
    }
    return PresentFrames(host, presenter, {0.8f, 0.25f, 0.15f, 1.0f}, true)
           && voryx::sample::VerifyWindowFrames(presenter)
           && voryx::sample::VerifyWindowShutdown(presenter);
}

bool RunLoop(const HostWindow &host, voryx::IPresenter &presenter,
             bool triangle)
{
    int delay = 0;
    for (;;) {
        if (!host.Pump(delay)) {
            return false;
        }
        if (host.closed) {
            return true;
        }
        // 示例使用缓冲缩放 1，configure 尺寸可直接作为像素尺寸。
        if (!SetExtent(presenter, host.configured)) {
            return false;
        }
        const auto result = voryx::sample::Present(
            presenter, {0.15f, 0.35f, 0.8f, 1.0f}, triangle);
        delay = 0;
        if (!result) {
            const auto code = result.GetError().errorCode;
            if (code != voryx::ErrorCode::SurfaceUnavailable
                && !Retryable(code)) {
                PrintError(result.GetError());
                return false;
            }
            delay = 50;
        }
    }
}
} // namespace

int main(int argc, char **argv)
{
    bool verify = false;
    using voryx::sample::DrawMode;
    auto drawMode = DrawMode::Clear;
    bool allowSoftware = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--verify") {
            verify = true;
        } else if (argument == "--triangle") {
            drawMode = DrawMode::Color;
        } else if (argument == "--texture") {
            drawMode = DrawMode::Texture;
        } else if (argument == "--depth") {
            drawMode = DrawMode::Depth;
        } else if (argument == "--indexed") {
            drawMode = DrawMode::Indexed;
        } else if (argument == "--binding") {
            drawMode = DrawMode::Binding;
        } else if (argument == "--resources") {
            drawMode = DrawMode::Resources;
        } else if (argument == "--allow-software") {
            allowSoftware = true;
        } else {
            std::fputs("Usage: wayland_clear [--verify] [--triangle] "
                       "[--allow-software]\n",
                       stderr);
            return 2;
        }
    }
    HostWindow host;
    if (!host.Open()) {
        std::fputs("Wayland xdg-shell window setup failed.\n", stderr);
        return 1;
    }
    std::atomic<int> errors{0};
    voryx::PresenterDesc desc{};
    desc.window = voryx::NativeWindowDesc::ForWayland(
        reinterpret_cast<std::uintptr_t>(host.display),
        reinterpret_cast<std::uintptr_t>(host.surface));
    desc.device.backend = voryx::BackendType::Vulkan;
    desc.device.allowSoftwareAdapter = allowSoftware;
    desc.device.enableValidation = verify;
    desc.device.logUserData = &errors;
    desc.device.logCallback = [](voryx::LogLevel level,
                                 std::string_view message,
                                 void *user) noexcept {
        if (level == voryx::LogLevel::Error) {
            static_cast<std::atomic<int> *>(user)->fetch_add(1);
        }
        std::fwrite(message.data(), 1, message.size(), stderr);
        std::fputc('\n', stderr);
    };
    auto result = voryx::CreatePresenter(desc);
    if (!result) {
        PrintError(result.GetError());
        return 1;
    }
    auto presenter = std::move(result).Value();
    if (!verify) {
        const auto configured =
            voryx::sample::ConfigureDrawMode(*presenter, drawMode);
        if (!configured) {
            PrintError(configured.GetError());
            return 1;
        }
    }
    const bool passed =
        verify ? Verify(host, *presenter)
               : RunLoop(host, *presenter, drawMode != DrawMode::Clear);
    // Vulkan 表面释放后才可销毁 xdg-shell 角色、wl_surface 和连接。
    presenter.reset();
    if (!passed || errors.load() != 0
        || wl_display_get_error(host.display) != 0) {
        std::fputs("Wayland clear verification failed.\n", stderr);
        return 1;
    }
    if (verify) {
        std::puts("Wayland Vulkan clear, configure resize, pause and resume "
                  "verified.");
    }
    return 0;
}

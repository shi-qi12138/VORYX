#include <voryx/runtime/presenter.hpp>

#include "triangle_sample.hpp"
#include "window_frame_sample.hpp"

#include <X11/Xlib.h>
#include <poll.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <string_view>
#include <utility>

namespace
{
struct HostWindow
{
    Display *display = nullptr;
    Window window = 0;
    Atom protocols = 0;
    Atom close = 0;

    ~HostWindow()
    {
        if (window != 0) {
            XDestroyWindow(display, window);
        }
        if (display != nullptr) {
            XCloseDisplay(display);
        }
    }

    bool Open(bool verify)
    {
        if (XInitThreads() == 0) {
            std::fputs("XInitThreads failed.\n", stderr);
            return false;
        }
        display = XOpenDisplay(nullptr);
        if (display == nullptr) {
            std::fputs(
                "Cannot open X11 display. Check DISPLAY and session access.\n",
                stderr);
            return false;
        }
        XSetWindowAttributes attributes{};
        attributes.event_mask = StructureNotifyMask | ExposureMask;
        // 测试直接控制映射与尺寸，避免窗口管理器异步调整。
        attributes.override_redirect = verify ? True : False;
        window = XCreateWindow(display, DefaultRootWindow(display), 0, 0, 640,
                               420, 0, CopyFromParent, InputOutput, nullptr,
                               CWEventMask | CWOverrideRedirect, &attributes);
        if (window == 0) {
            return false;
        }
        XStoreName(display, window, "VORYX Vulkan X11 clear");
        protocols = XInternAtom(display, "WM_PROTOCOLS", False);
        close = XInternAtom(display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(display, window, &close, 1);
        XMapWindow(display, window);
        XSync(display, False);
        return true;
    }

    bool Pump() const
    {
        while (XPending(display) != 0) {
            XEvent event{};
            XNextEvent(display, &event);
            if (event.type == ClientMessage && event.xclient.window == window
                && event.xclient.message_type == protocols
                && event.xclient.format == 32
                && static_cast<Atom>(event.xclient.data.l[0]) == close) {
                return false;
            }
        }
        return true;
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

bool PresentFrames(const HostWindow &host, voryx::IPresenter &presenter,
                   voryx::ClearColor color, bool triangle = false)
{
    int completed = 0;
    for (int attempt = 0; attempt < 24 && completed < 3; ++attempt) {
        if (!host.Pump()) {
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
    const auto invalid = voryx::CreatePresenter(
        {{}, voryx::NativeWindowDesc::ForXlib(0, host.window)});
    if (invalid
        || invalid.GetError().errorCode != voryx::ErrorCode::InvalidArgument
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
    for (const auto size :
         std::array{voryx::Extent2D{760, 500}, voryx::Extent2D{680, 460}}) {
        XResizeWindow(host.display, host.window, size.width, size.height);
        XSync(host.display, False);
        XWindowAttributes resized{};
        if (XGetWindowAttributes(host.display, host.window, &resized) == 0
            || resized.width != static_cast<int>(size.width)
            || resized.height != static_cast<int>(size.height)
            || !voryx::sample::VerifyResizedAsyncFrame(presenter, size)) {
            return false;
        }
    }
    XUnmapWindow(host.display, host.window);
    XSync(host.display, False);
    const auto paused = presenter.DrawAndPresent({});
    if (paused
        || paused.GetError().errorCode != voryx::ErrorCode::SurfaceUnavailable
        || !voryx::sample::VerifyWindowFramePaused(presenter)) {
        return false;
    }
    XMapWindow(host.display, host.window);
    XSync(host.display, False);
    return PresentFrames(host, presenter, {0.8f, 0.25f, 0.15f, 1.0f}, true)
           && voryx::sample::VerifyWindowFrames(presenter)
           && voryx::sample::VerifyWindowShutdown(presenter);
}

bool RunLoop(const HostWindow &host, voryx::IPresenter &presenter,
             bool triangle)
{
    while (host.Pump()) {
        const auto result = voryx::sample::Present(
            presenter, {0.15f, 0.35f, 0.8f, 1.0f}, triangle);
        if (!result) {
            const auto code = result.GetError().errorCode;
            if (code != voryx::ErrorCode::SurfaceUnavailable
                && !Retryable(code)) {
                PrintError(result.GetError());
                return false;
            }
            pollfd events{ConnectionNumber(host.display), POLLIN, 0};
            poll(&events, 1, 50);
        }
    }
    return true;
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
            std::fputs("Usage: xlib_clear [--verify] [--triangle] "
                       "[--allow-software]\n",
                       stderr);
            return 2;
        }
    }
    HostWindow host;
    if (!host.Open(verify)) {
        return 1;
    }
    std::atomic<int> errors{0};
    voryx::PresenterDesc desc{};
    desc.window = voryx::NativeWindowDesc::ForXlib(
        reinterpret_cast<std::uintptr_t>(host.display), host.window);
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
    // 先销毁表面与交换链，再释放 X11 窗口和连接。
    presenter.reset();
    if (!passed || errors.load() != 0) {
        std::fputs("X11 clear verification failed.\n", stderr);
        return 1;
    }
    if (verify) {
        std::puts("X11 Vulkan clear, resize, unmap and remap verified.");
    }
    return 0;
}

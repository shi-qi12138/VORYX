#include <voryx/runtime/presenter.hpp>

#include "triangle_sample.hpp"
#include "window_frame_sample.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <string_view>
#include <thread>
#include <utility>

namespace
{
LRESULT CALLBACK HandleMessage(HWND window, UINT message, WPARAM wparam,
                               LPARAM lparam)
{
    if (message == WM_CLOSE) {
        // 先退出循环，保证 SDK 对象在宿主窗口之前释放。
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

struct Window
{
    HWND handle = nullptr;
    ~Window()
    {
        if (handle != nullptr) {
            DestroyWindow(handle);
        }
    }
};

bool PumpMessages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return true;
}

void PrintError(const voryx::Error &error)
{
    const auto name = voryx::GetErrorCodeName(error.errorCode);
    std::fprintf(stderr, "%s: %.*s (%lld)\n", error.operation.c_str(),
                 static_cast<int>(name.size()), name.data(),
                 static_cast<long long>(error.backendCode.value_or(0)));
}

bool Retryable(voryx::ErrorCode error)
{
    return error == voryx::ErrorCode::SwapchainOutOfDate
           || error == voryx::ErrorCode::Timeout;
}

bool PresentFrames(voryx::IPresenter &presenter, voryx::ClearColor color,
                   bool triangle = false)
{
    int completed = 0;
    for (int attempt = 0; attempt < 24 && completed < 3; ++attempt) {
        if (!PumpMessages()) {
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

bool Verify(voryx::IPresenter &presenter, HWND window)
{
    const auto invalid = presenter.ClearAndPresent({-0.1f, 0.0f, 0.0f, 1.0f});
    if (invalid
        || invalid.GetError().errorCode != voryx::ErrorCode::InvalidArgument) {
        return false;
    }
    bool rejected = false;
    {
        std::jthread thread([&]() {
            const auto result = presenter.ClearAndPresent({});
            rejected =
                !result
                && result.GetError().errorCode == voryx::ErrorCode::WrongThread;
        });
    }
    if (!rejected || !PresentFrames(presenter, {0.15f, 0.35f, 0.8f, 1.0f})) {
        return false;
    }
    if (!voryx::sample::VerifyTriangleSetup(presenter)
        || !PresentFrames(presenter, {0.1f, 0.1f, 0.1f, 1.0f}, true)
        || !voryx::sample::VerifyTextureSetup(presenter)
        || !PresentFrames(presenter, {0.1f, 0.1f, 0.1f, 1.0f}, true)
        || !voryx::sample::VerifyDepthSetup(presenter)
        || !PresentFrames(presenter, {0.1f, 0.1f, 0.1f, 1.0f}, true)
        || !voryx::sample::VerifyIndexedSetup(presenter)
        || !PresentFrames(presenter, {0.1f, 0.1f, 0.1f, 1.0f}, true)
        || !voryx::sample::VerifyBoundTextureSetup(presenter)
        || !PresentFrames(presenter, {0.1f, 0.1f, 0.1f, 1.0f}, true)
        || !voryx::sample::VerifyResourceSetup(presenter)
        || !PresentFrames(presenter, {0.1f, 0.1f, 0.1f, 1.0f}, true)
        || !voryx::sample::VerifyWindowFrames(presenter)) {
        return false;
    }
    for (const auto size : std::array{SIZE{760, 500}, SIZE{680, 460}}) {
        RECT client{};
        if (!SetWindowPos(window, nullptr, 0, 0, size.cx, size.cy,
                          SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE)
            || !PumpMessages() || !GetClientRect(window, &client)
            || !voryx::sample::VerifyResizedAsyncFrame(
                presenter, {static_cast<std::uint32_t>(client.right),
                            static_cast<std::uint32_t>(client.bottom)})) {
            return false;
        }
    }
    ShowWindow(window, SW_MINIMIZE);
    const auto paused = presenter.DrawAndPresent({});
    if (paused
        || paused.GetError().errorCode != voryx::ErrorCode::SurfaceUnavailable
        || !voryx::sample::VerifyWindowFramePaused(presenter)) {
        return false;
    }
    ShowWindow(window, SW_RESTORE);
    return PresentFrames(presenter, {0.8f, 0.25f, 0.15f, 1.0f}, true)
           && voryx::sample::VerifyWindowFrames(presenter)
           && voryx::sample::VerifyWindowShutdown(presenter);
}

bool RunLoop(voryx::IPresenter &presenter, bool triangle)
{
    while (PumpMessages()) {
        const auto result = voryx::sample::Present(
            presenter, {0.15f, 0.35f, 0.8f, 1.0f}, triangle);
        if (!result) {
            const auto code = result.GetError().errorCode;
            if (code != voryx::ErrorCode::SurfaceUnavailable
                && !Retryable(code)) {
                PrintError(result.GetError());
                return false;
            }
            MsgWaitForMultipleObjectsEx(0, nullptr, 50, QS_ALLINPUT,
                                        MWMO_INPUTAVAILABLE);
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
        } else {
            return 2;
        }
    }
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW definition{};
    definition.hInstance = instance;
    definition.lpfnWndProc = HandleMessage;
    definition.lpszClassName = L"VoryxWin32Clear";
    if (RegisterClassW(&definition) == 0) {
        std::fprintf(stderr, "RegisterClassW failed: %lu\n", GetLastError());
        return 1;
    }
    Window window{CreateWindowExW(0, definition.lpszClassName,
                                  L"VORYX Vulkan clear", WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 640, 420,
                                  nullptr, nullptr, instance, nullptr)};
    if (window.handle == nullptr) {
        std::fprintf(stderr, "CreateWindowExW failed: %lu\n", GetLastError());
        return 1;
    }
    ShowWindow(window.handle, SW_SHOW);
    std::atomic<int> errors{0};
    voryx::PresenterDesc desc{};
    desc.window = voryx::NativeWindowDesc::ForWindows(
        reinterpret_cast<std::uintptr_t>(window.handle),
        reinterpret_cast<std::uintptr_t>(instance));
    desc.device.backend = voryx::BackendType::Vulkan;
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
    const bool passed = verify
                            ? Verify(*presenter, window.handle)
                            : RunLoop(*presenter, drawMode != DrawMode::Clear);
    presenter.reset();
    if (passed && verify) {
        // 在同一宿主窗口验证只依赖通用帧接口的接入。
        desc.device.backend = voryx::BackendType::Auto;
        auto frame = voryx::CreateFramePresenter(desc);
        if (!frame) {
            PrintError(frame.GetError());
            return 1;
        }
        RECT client{};
        if (!GetClientRect(window.handle, &client)
            || !voryx::sample::VerifyResizedAsyncFrame(
                *frame.Value(), {static_cast<std::uint32_t>(client.right),
                                 static_cast<std::uint32_t>(client.bottom)})
            || !voryx::sample::VerifyWindowShutdown(*frame.Value())) {
            return 1;
        }
    }
    if (!passed || errors.load() != 0) {
        std::fputs("Win32 clear verification failed.\n", stderr);
        return 1;
    }
    if (verify) {
        std::puts("Win32 Vulkan clear, resize, minimize and restore verified.");
    }
    return 0;
}

#include "opengl_context.hpp"
#include "opengl_failure.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <bit>
#include <cstdint>

namespace voryx::opengl_backend
{
namespace
{
void Check(bool succeeded, ErrorCode code)
{
    if (!succeeded) {
        throw Failure{code, GetLastError()};
    }
}
} // namespace

struct Context::Native
{
    ~Native()
    {
        if (handle) {
            wglDeleteContext(handle);
        }
        if (dc) {
            ReleaseDC(window, dc);
        }
        if (window) {
            DestroyWindow(window);
        }
    }

    HWND window = nullptr;
    HDC dc = nullptr;
    HGLRC handle = nullptr;
    bool software = false;
};

Context::Context()
    : native(std::make_unique<Native>())
{
}

Context::~Context() = default;

void Context::Initialize()
{
    native->window =
        CreateWindowExW(0, L"STATIC", L"VORYX OpenGL", WS_POPUP, 0, 0, 1, 1,
                        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(native->window != nullptr, ErrorCode::BackendUnavailable);
    native->dc = GetDC(native->window);
    Check(native->dc != nullptr, ErrorCode::BackendUnavailable);

    PIXELFORMATDESCRIPTOR format{};
    format.nSize = sizeof(format);
    format.nVersion = 1;
    format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    format.iPixelType = PFD_TYPE_RGBA;
    format.cColorBits = 24;
    const auto pixelFormat = ChoosePixelFormat(native->dc, &format);
    Check(pixelFormat != 0, ErrorCode::BackendUnavailable);
    Check(DescribePixelFormat(native->dc, pixelFormat, sizeof(format), &format)
              != 0,
          ErrorCode::BackendUnavailable);
    Check(SetPixelFormat(native->dc, pixelFormat, &format),
          ErrorCode::BackendUnavailable);
    native->software = (format.dwFlags & PFD_GENERIC_FORMAT) != 0
                       && (format.dwFlags & PFD_GENERIC_ACCELERATED) == 0;

    native->handle = wglCreateContext(native->dc);
    Check(native->handle != nullptr, ErrorCode::BackendUnavailable);
    HGLRC core = nullptr;
    {
        // Windows 需要先用临时上下文取得创建核心上下文的入口。
        const Current current(*this);
        using Create = HGLRC(WINAPI *)(HDC, HGLRC, const int *);
        const auto create =
            std::bit_cast<Create>(Resolve("wglCreateContextAttribsARB"));
        if (!create) {
            throw Failure{ErrorCode::BackendUnavailable};
        }
        constexpr int contextMajorVersion = 0x2091;
        constexpr int contextMinorVersion = 0x2092;
        constexpr int contextProfileMask = 0x9126;
        constexpr int coreProfile = 0x00000001;
        constexpr int attributes[]{
            contextMajorVersion, 3, contextMinorVersion, 3, contextProfileMask,
            coreProfile,         0};
        core = create(native->dc, nullptr, attributes);
        Check(core != nullptr, ErrorCode::BackendUnavailable);
    }
    wglDeleteContext(native->handle);
    native->handle = core;
}

Proc Context::Resolve(const char *name) const noexcept
{
    const auto proc = wglGetProcAddress(name);
    const auto address = std::bit_cast<std::uintptr_t>(proc);
    if (address > 3 && address != static_cast<std::uintptr_t>(-1)) {
        return std::bit_cast<Proc>(proc);
    }
    return std::bit_cast<Proc>(
        GetProcAddress(GetModuleHandleW(L"opengl32.dll"), name));
}

bool Context::SoftwarePixelFormat() const noexcept
{
    return native->software;
}

Context::Current::Current(Context &context)
{
    previous[0] = wglGetCurrentDC();
    previous[1] = wglGetCurrentContext();
    Check(wglMakeCurrent(context.native->dc, context.native->handle),
          ErrorCode::BackendUnavailable);
}

Context::Current::~Current()
{
    if (!wglMakeCurrent(static_cast<HDC>(previous[0]),
                        static_cast<HGLRC>(previous[1]))) {
        wglMakeCurrent(nullptr, nullptr);
    }
}
} // namespace voryx::opengl_backend

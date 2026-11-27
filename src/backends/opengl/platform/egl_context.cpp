#include "opengl_context.hpp"
#include "opengl_failure.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <bit>
#include <mutex>

namespace voryx::opengl_backend
{
namespace
{
void Check(bool succeeded)
{
    if (!succeeded) {
        const auto code = eglGetError();
        const auto error = code == EGL_CONTEXT_LOST ? ErrorCode::DeviceLost
                           : code == EGL_BAD_ALLOC
                               ? ErrorCode::OutOfMemory
                               : ErrorCode::BackendUnavailable;
        throw Failure{error, code};
    }
}

struct DisplayReferences
{
    std::mutex mutex;
    EGLDisplay display = EGL_NO_DISPLAY;
    std::size_t users = 0;
};

// 同一无窗口平台的 EGLDisplay 可以由多个设备共用，最后一个设备才终止它。
DisplayReferences &SharedDisplay()
{
    static DisplayReferences state;
    return state;
}

EGLDisplay AcquireDisplay()
{
    auto &state = SharedDisplay();
    const std::lock_guard lock(state.mutex);
    if (state.users == 0) {
        const auto display = eglGetPlatformDisplay(
            EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
        Check(display != EGL_NO_DISPLAY);
        EGLint major = 0;
        EGLint minor = 0;
        Check(eglInitialize(display, &major, &minor));
        if (major < 1 || (major == 1 && minor < 5)) {
            eglTerminate(display);
            throw Failure{ErrorCode::BackendUnavailable};
        }
        state.display = display;
    }
    ++state.users;
    return state.display;
}

void ReleaseDisplay() noexcept
{
    auto &state = SharedDisplay();
    const std::lock_guard lock(state.mutex);
    if (--state.users == 0) {
        eglTerminate(state.display);
        state.display = EGL_NO_DISPLAY;
    }
}

class DesktopApi
{
public:
    DesktopApi()
        : previous(eglQueryAPI())
    {
        Check(eglBindAPI(EGL_OPENGL_API));
    }

    ~DesktopApi()
    {
        eglBindAPI(previous);
    }

private:
    EGLenum previous;
};
} // namespace

struct Context::Native
{
    ~Native()
    {
        if (handle != EGL_NO_CONTEXT) {
            eglDestroyContext(display, handle);
        }
        if (surface != EGL_NO_SURFACE) {
            eglDestroySurface(display, surface);
        }
        if (display != EGL_NO_DISPLAY) {
            ReleaseDisplay();
        }
    }

    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext handle = EGL_NO_CONTEXT;
};

Context::Context()
    : native(std::make_unique<Native>())
{
}

Context::~Context() = default;

void Context::Initialize()
{
    native->display = AcquireDisplay();
    const DesktopApi api;
    constexpr EGLint format[]{EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                              EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE};
    EGLConfig config{};
    EGLint count = 0;
    Check(eglChooseConfig(native->display, format, &config, 1, &count));
    if (count == 0) {
        throw Failure{ErrorCode::BackendUnavailable};
    }
    constexpr EGLint surfaceAttributes[]{EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    native->surface =
        eglCreatePbufferSurface(native->display, config, surfaceAttributes);
    Check(native->surface != EGL_NO_SURFACE);
    constexpr EGLint attributes[]{EGL_CONTEXT_MAJOR_VERSION,
                                  3,
                                  EGL_CONTEXT_MINOR_VERSION,
                                  3,
                                  EGL_CONTEXT_OPENGL_PROFILE_MASK,
                                  EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                  EGL_NONE};
    native->handle =
        eglCreateContext(native->display, config, EGL_NO_CONTEXT, attributes);
    Check(native->handle != EGL_NO_CONTEXT);
}

Proc Context::Resolve(const char *name) const noexcept
{
    return std::bit_cast<Proc>(eglGetProcAddress(name));
}

bool Context::SoftwarePixelFormat() const noexcept
{
    return false;
}

Context::Current::Current(Context &context)
    : context(context)
{
    previousApi = eglQueryAPI();
    Check(eglBindAPI(EGL_OPENGL_API));
    previous = {eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW),
                eglGetCurrentSurface(EGL_READ), eglGetCurrentContext()};
    if (!eglMakeCurrent(context.native->display, context.native->surface,
                        context.native->surface, context.native->handle)) {
        const auto error = eglGetError();
        eglBindAPI(previousApi);
        throw Failure{error == EGL_CONTEXT_LOST ? ErrorCode::DeviceLost
                                                : ErrorCode::BackendUnavailable,
                      error};
    }
}

Context::Current::~Current()
{
    if (previous[3] == EGL_NO_CONTEXT
        || !eglMakeCurrent(previous[0], previous[1], previous[2],
                           previous[3])) {
        eglMakeCurrent(context.native->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
    }
    eglBindAPI(previousApi);
}
} // namespace voryx::opengl_backend

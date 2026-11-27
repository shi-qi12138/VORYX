#pragma once

#include "opengl_context.hpp"

#include <cstddef>

#if defined(_WIN32)
#define VORYX_GL_CALL __stdcall
#else
#define VORYX_GL_CALL
#endif

namespace voryx::opengl_backend
{
inline constexpr unsigned int copyReadBuffer = 0x8F36;
inline constexpr unsigned int copyWriteBuffer = 0x8F37;
inline constexpr unsigned int dynamicDraw = 0x88E8;
inline constexpr unsigned int rendererName = 0x1F01;
inline constexpr unsigned int majorVersion = 0x821B;
inline constexpr unsigned int minorVersion = 0x821C;
inline constexpr unsigned int outOfMemory = 0x0505;
inline constexpr unsigned int contextLost = 0x0507;
inline constexpr unsigned int drawFramebuffer = 0x8CA9;
inline constexpr unsigned int maxTextureSize = 0x0D33;
inline constexpr unsigned int dither = 0x0BD0;

using GlSize = std::ptrdiff_t;

/// 只加载当前后端实际调用的 OpenGL 入口。
struct Api
{
    explicit Api(const Context &context);

    void(VORYX_GL_CALL *genBuffers)(int, unsigned int *);
    void(VORYX_GL_CALL *deleteBuffers)(int, const unsigned int *);
    void(VORYX_GL_CALL *bindBuffer)(unsigned int, unsigned int);
    void(VORYX_GL_CALL *bufferData)(unsigned int, GlSize, const void *,
                                    unsigned int);
    void(VORYX_GL_CALL *bufferSubData)(unsigned int, GlSize, GlSize,
                                       const void *);
    void(VORYX_GL_CALL *getBufferSubData)(unsigned int, GlSize, GlSize, void *);
    void(VORYX_GL_CALL *copyBufferSubData)(unsigned int, unsigned int, GlSize,
                                           GlSize, GlSize);
    void(VORYX_GL_CALL *genTextures)(int, unsigned int *);
    void(VORYX_GL_CALL *deleteTextures)(int, const unsigned int *);
    void(VORYX_GL_CALL *bindTexture)(unsigned int, unsigned int);
    void(VORYX_GL_CALL *texImage2D)(unsigned int, int, int, int, int, int,
                                    unsigned int, unsigned int, const void *);
    void(VORYX_GL_CALL *genFramebuffers)(int, unsigned int *);
    void(VORYX_GL_CALL *deleteFramebuffers)(int, const unsigned int *);
    void(VORYX_GL_CALL *bindFramebuffer)(unsigned int, unsigned int);
    void(VORYX_GL_CALL *framebufferTexture2D)(unsigned int, unsigned int,
                                              unsigned int, unsigned int, int);
    unsigned int(VORYX_GL_CALL *checkFramebufferStatus)(unsigned int);
    void(VORYX_GL_CALL *clearBufferfv)(unsigned int, int, const float *);
    void(VORYX_GL_CALL *readPixels)(int, int, int, int, unsigned int,
                                    unsigned int, void *);
    unsigned int(VORYX_GL_CALL *createShader)(unsigned int);
    void(VORYX_GL_CALL *shaderSource)(unsigned int, int, const char *const *,
                                      const int *);
    void(VORYX_GL_CALL *compileShader)(unsigned int);
    void(VORYX_GL_CALL *getShaderiv)(unsigned int, unsigned int, int *);
    void(VORYX_GL_CALL *getShaderInfoLog)(unsigned int, int, int *, char *);
    void(VORYX_GL_CALL *deleteShader)(unsigned int);
    unsigned int(VORYX_GL_CALL *createProgram)();
    void(VORYX_GL_CALL *attachShader)(unsigned int, unsigned int);
    void(VORYX_GL_CALL *detachShader)(unsigned int, unsigned int);
    void(VORYX_GL_CALL *linkProgram)(unsigned int);
    void(VORYX_GL_CALL *getProgramiv)(unsigned int, unsigned int, int *);
    void(VORYX_GL_CALL *getProgramInfoLog)(unsigned int, int, int *, char *);
    void(VORYX_GL_CALL *deleteProgram)(unsigned int);
    void(VORYX_GL_CALL *useProgram)(unsigned int);
    void(VORYX_GL_CALL *genVertexArrays)(int, unsigned int *);
    void(VORYX_GL_CALL *deleteVertexArrays)(int, const unsigned int *);
    void(VORYX_GL_CALL *bindVertexArray)(unsigned int);
    void(VORYX_GL_CALL *enableVertexAttribArray)(unsigned int);
    void(VORYX_GL_CALL *disableVertexAttribArray)(unsigned int);
    void(VORYX_GL_CALL *vertexAttribPointer)(unsigned int, int, unsigned int,
                                             unsigned char, int, const void *);
    void(VORYX_GL_CALL *viewport)(int, int, int, int);
    void(VORYX_GL_CALL *depthRange)(double, double);
    void(VORYX_GL_CALL *scissor)(int, int, int, int);
    void(VORYX_GL_CALL *drawArrays)(unsigned int, int, int);
    void(VORYX_GL_CALL *enable)(unsigned int);
    void(VORYX_GL_CALL *disable)(unsigned int);
    const unsigned char *(VORYX_GL_CALL *getString)(unsigned int);
    void(VORYX_GL_CALL *getIntegerv)(unsigned int, int *);
    unsigned int(VORYX_GL_CALL *getError)();
    void(VORYX_GL_CALL *finish)();

    void Check() const;
};
} // namespace voryx::opengl_backend

#undef VORYX_GL_CALL

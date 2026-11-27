#include "opengl_api.hpp"
#include "opengl_failure.hpp"

#include <bit>

namespace voryx::opengl_backend
{
namespace
{
template<typename Function>
Function Load(const Context &context, const char *name)
{
    const auto address = context.Resolve(name);
    if (!address) {
        throw Failure{ErrorCode::BackendUnavailable};
    }
    return std::bit_cast<Function>(address);
}
} // namespace

Api::Api(const Context &context)
    : genBuffers(Load<decltype(genBuffers)>(context, "glGenBuffers"))
    , deleteBuffers(Load<decltype(deleteBuffers)>(context, "glDeleteBuffers"))
    , bindBuffer(Load<decltype(bindBuffer)>(context, "glBindBuffer"))
    , bufferData(Load<decltype(bufferData)>(context, "glBufferData"))
    , bufferSubData(Load<decltype(bufferSubData)>(context, "glBufferSubData"))
    , getBufferSubData(
          Load<decltype(getBufferSubData)>(context, "glGetBufferSubData"))
    , copyBufferSubData(
          Load<decltype(copyBufferSubData)>(context, "glCopyBufferSubData"))
    , genTextures(Load<decltype(genTextures)>(context, "glGenTextures"))
    , deleteTextures(
          Load<decltype(deleteTextures)>(context, "glDeleteTextures"))
    , bindTexture(Load<decltype(bindTexture)>(context, "glBindTexture"))
    , texImage2D(Load<decltype(texImage2D)>(context, "glTexImage2D"))
    , genFramebuffers(
          Load<decltype(genFramebuffers)>(context, "glGenFramebuffers"))
    , deleteFramebuffers(
          Load<decltype(deleteFramebuffers)>(context, "glDeleteFramebuffers"))
    , bindFramebuffer(
          Load<decltype(bindFramebuffer)>(context, "glBindFramebuffer"))
    , framebufferTexture2D(Load<decltype(framebufferTexture2D)>(
          context, "glFramebufferTexture2D"))
    , checkFramebufferStatus(Load<decltype(checkFramebufferStatus)>(
          context, "glCheckFramebufferStatus"))
    , clearBufferfv(Load<decltype(clearBufferfv)>(context, "glClearBufferfv"))
    , readPixels(Load<decltype(readPixels)>(context, "glReadPixels"))
    , createShader(Load<decltype(createShader)>(context, "glCreateShader"))
    , shaderSource(Load<decltype(shaderSource)>(context, "glShaderSource"))
    , compileShader(Load<decltype(compileShader)>(context, "glCompileShader"))
    , getShaderiv(Load<decltype(getShaderiv)>(context, "glGetShaderiv"))
    , getShaderInfoLog(
          Load<decltype(getShaderInfoLog)>(context, "glGetShaderInfoLog"))
    , deleteShader(Load<decltype(deleteShader)>(context, "glDeleteShader"))
    , createProgram(Load<decltype(createProgram)>(context, "glCreateProgram"))
    , attachShader(Load<decltype(attachShader)>(context, "glAttachShader"))
    , detachShader(Load<decltype(detachShader)>(context, "glDetachShader"))
    , linkProgram(Load<decltype(linkProgram)>(context, "glLinkProgram"))
    , getProgramiv(Load<decltype(getProgramiv)>(context, "glGetProgramiv"))
    , getProgramInfoLog(
          Load<decltype(getProgramInfoLog)>(context, "glGetProgramInfoLog"))
    , deleteProgram(Load<decltype(deleteProgram)>(context, "glDeleteProgram"))
    , useProgram(Load<decltype(useProgram)>(context, "glUseProgram"))
    , genVertexArrays(
          Load<decltype(genVertexArrays)>(context, "glGenVertexArrays"))
    , deleteVertexArrays(
          Load<decltype(deleteVertexArrays)>(context, "glDeleteVertexArrays"))
    , bindVertexArray(
          Load<decltype(bindVertexArray)>(context, "glBindVertexArray"))
    , enableVertexAttribArray(Load<decltype(enableVertexAttribArray)>(
          context, "glEnableVertexAttribArray"))
    , disableVertexAttribArray(Load<decltype(disableVertexAttribArray)>(
          context, "glDisableVertexAttribArray"))
    , vertexAttribPointer(
          Load<decltype(vertexAttribPointer)>(context, "glVertexAttribPointer"))
    , viewport(Load<decltype(viewport)>(context, "glViewport"))
    , depthRange(Load<decltype(depthRange)>(context, "glDepthRange"))
    , scissor(Load<decltype(scissor)>(context, "glScissor"))
    , drawArrays(Load<decltype(drawArrays)>(context, "glDrawArrays"))
    , enable(Load<decltype(enable)>(context, "glEnable"))
    , disable(Load<decltype(disable)>(context, "glDisable"))
    , getString(Load<decltype(getString)>(context, "glGetString"))
    , getIntegerv(Load<decltype(getIntegerv)>(context, "glGetIntegerv"))
    , getError(Load<decltype(getError)>(context, "glGetError"))
    , finish(Load<decltype(finish)>(context, "glFinish"))
{
}

void Api::Check() const
{
    const auto result = getError();
    if (result == 0) {
        return;
    }
    if (result == contextLost) {
        throw Failure{ErrorCode::DeviceLost, result};
    }
    if (result == outOfMemory) {
        throw Failure{ErrorCode::OutOfMemory, result};
    }
    throw Failure{ErrorCode::InternalError, result};
}
} // namespace voryx::opengl_backend

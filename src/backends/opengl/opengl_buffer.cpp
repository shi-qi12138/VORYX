#include "opengl_buffer.hpp"

#include "opengl_api.hpp"
#include "opengl_context.hpp"
#include "opengl_failure.hpp"

namespace voryx::opengl_backend
{
Buffer::Buffer(Context &context, const Api &api) noexcept
    : context(context)
    , api(api)
{
}

Buffer::~Buffer()
{
    if (name != 0) {
        try {
            const Context::Current current(context);
            api.deleteBuffers(1, &name);
        } catch (const Failure &) {
            // 无法激活时由上下文销毁回收驱动对象。
        }
    }
}

void Buffer::Allocate(std::uint64_t size)
{
    api.genBuffers(1, &name);
    api.Check();
    Bind(copyWriteBuffer);
    api.bufferData(copyWriteBuffer, static_cast<GlSize>(size), nullptr,
                   dynamicDraw);
    api.Check();
}

void Buffer::Bind(unsigned int target) const
{
    api.bindBuffer(target, name);
    api.Check();
}

unsigned int Buffer::Name() const noexcept
{
    return name;
}

Result<void> Buffer::Write(std::uint64_t offset, ByteSpan data) noexcept
{
    return Invoke<void>("WriteOpenGLBuffer", [&]() {
        if (!data.empty()) {
            const Context::Current current(context);
            Bind(copyWriteBuffer);
            api.bufferSubData(copyWriteBuffer, static_cast<GlSize>(offset),
                              static_cast<GlSize>(data.size()), data.data());
            api.Check();
        }
        return Result<void>::Success();
    });
}

Result<void> Buffer::Read(std::uint64_t offset, MutableByteSpan output) noexcept
{
    return Invoke<void>("ReadOpenGLBuffer", [&]() {
        if (!output.empty()) {
            const Context::Current current(context);
            Bind(copyReadBuffer);
            api.getBufferSubData(copyReadBuffer, static_cast<GlSize>(offset),
                                 static_cast<GlSize>(output.size()),
                                 output.data());
            api.Check();
        }
        return Result<void>::Success();
    });
}
} // namespace voryx::opengl_backend

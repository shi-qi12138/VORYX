#include "opengl_color_target.hpp"

#include "opengl_api.hpp"
#include "opengl_context.hpp"
#include "opengl_failure.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace voryx::opengl_backend
{
namespace
{
constexpr unsigned int texture2D = 0x0DE1;
constexpr int rgba8 = 0x8058;
constexpr unsigned int rgba = 0x1908;
constexpr unsigned int unsignedByte = 0x1401;
constexpr unsigned int readFramebuffer = 0x8CA8;
constexpr unsigned int colorAttachment0 = 0x8CE0;
constexpr unsigned int framebufferComplete = 0x8CD5;
constexpr unsigned int colorBuffer = 0x1800;
} // namespace

ColorTarget::ColorTarget(Context &context, const Api &api,
                         Extent2D extent) noexcept
    : context(context)
    , api(api)
    , extent(extent)
{
}

ColorTarget::~ColorTarget()
{
    if (texture == 0 && framebuffer == 0) {
        return;
    }
    try {
        const Context::Current current(context);
        api.deleteFramebuffers(1, &framebuffer);
        api.deleteTextures(1, &texture);
    } catch (const Failure &) {
        // 无法激活时由上下文销毁回收驱动对象。
    }
}

void ColorTarget::Allocate()
{
    api.genTextures(1, &texture);
    api.Check();
    api.bindTexture(texture2D, texture);
    api.texImage2D(texture2D, 0, rgba8, static_cast<int>(extent.width),
                   static_cast<int>(extent.height), 0, rgba, unsignedByte,
                   nullptr);
    api.Check();

    api.genFramebuffers(1, &framebuffer);
    api.Check();
    api.bindFramebuffer(drawFramebuffer, framebuffer);
    api.framebufferTexture2D(drawFramebuffer, colorAttachment0, texture2D,
                             texture, 0);
    const auto status = api.checkFramebufferStatus(drawFramebuffer);
    api.Check();
    if (status != framebufferComplete) {
        throw Failure{ErrorCode::BackendUnavailable, status};
    }
    api.bindFramebuffer(drawFramebuffer, 0);
    api.bindTexture(texture2D, 0);
    api.Check();
}

void ColorTarget::Clear(ClearColor color) const
{
    const std::array channels{color.red, color.green, color.blue, color.alpha};
    api.bindFramebuffer(drawFramebuffer, framebuffer);
    api.clearBufferfv(colorBuffer, 0, channels.data());
    api.Check();
}

void ColorTarget::Read(MutableByteSpan output) const
{
    api.bindFramebuffer(readFramebuffer, framebuffer);
    api.readPixels(0, 0, static_cast<int>(extent.width),
                   static_cast<int>(extent.height), rgba, unsignedByte,
                   output.data());
    api.bindFramebuffer(readFramebuffer, 0);
    api.Check();

    // GL 从底行开始返回。导出像素须按 RHI 的左上角约定排列。
    const auto rowBytes = static_cast<std::size_t>(extent.width) * 4;
    for (std::uint32_t top = 0; top < extent.height / 2; ++top) {
        const auto bottom = extent.height - 1 - top;
        const auto row = output.subspan(top * rowBytes, rowBytes);
        std::swap_ranges(row.begin(), row.end(),
                         output.subspan(bottom * rowBytes).begin());
    }
}
Extent2D ColorTarget::Extent() const noexcept
{
    return extent;
}
} // namespace voryx::opengl_backend

#pragma once

#include <buffer_backend.hpp>

namespace voryx::opengl_backend
{
struct Api;
class Context;

/// 一个单采样 RGBA8 颜色附件与其私有帧缓冲。
class ColorTarget final : public rhi_detail::TextureStorage
{
public:
    ColorTarget(Context &context, const Api &api, Extent2D extent) noexcept;
    ~ColorTarget() override;
    ColorTarget(const ColorTarget &) = delete;
    ColorTarget &operator=(const ColorTarget &) = delete;

    // 调用者负责先激活该设备的上下文。
    void Allocate();
    void Clear(ClearColor color) const;
    void Read(MutableByteSpan output) const;
    [[nodiscard]] Extent2D Extent() const noexcept;

private:
    Context &context;
    const Api &api;
    const Extent2D extent;
    unsigned int texture = 0;
    unsigned int framebuffer = 0;
};
} // namespace voryx::opengl_backend

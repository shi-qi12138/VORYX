#pragma once

#include <voryx/rhi/pipeline.hpp>
#include <voryx/rhi/texture.hpp>

namespace voryx::opengl_backend
{
struct Api;
class Pipeline;

/// 单次提交的绘制状态。临时 VAO 不跨提交持有任何缓冲对象引用。
class DrawState
{
public:
    explicit DrawState(const Api &api) noexcept;
    ~DrawState();
    DrawState(const DrawState &) = delete;
    DrawState &operator=(const DrawState &) = delete;

    void BeginPass(std::uint32_t height);
    void SetPipeline(const Pipeline &value) noexcept;
    void SetVertexBuffer(unsigned int name, std::uint64_t offset) noexcept;
    void SetViewport(const Viewport &view);
    void SetScissor(const Rect2D &rect);
    void Draw(std::uint32_t count, std::uint32_t first);

private:
    const Api &api;
    const Pipeline *pipeline = nullptr;
    std::span<const VertexAttribute> enabledAttributes;
    unsigned int vao = 0;
    unsigned int vertexBuffer = 0;
    std::uint64_t vertexOffset = 0;
    std::uint32_t targetHeight = 0;
};
} // namespace voryx::opengl_backend

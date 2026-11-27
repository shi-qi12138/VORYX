#pragma once

#include <buffer_backend.hpp>

#include <vector>

namespace voryx::opengl_backend
{
struct Api;
class Context;

/// 管线持有已链接的程序与顶点布局副本。
class Pipeline final : public rhi_detail::PipelineStorage
{
public:
    Pipeline(Context &context, const Api &api,
             const GraphicsPipelineDesc &desc);
    ~Pipeline() override;

    void Link(std::string_view vertex, std::string_view fragment);
    void Use() const;

    [[nodiscard]] std::span<const VertexAttribute> Attributes() const noexcept;
    [[nodiscard]] std::uint32_t Stride() const noexcept;

private:
    Context &context;
    const Api &api;
    const std::vector<VertexAttribute> attributes;
    const std::uint32_t stride;
    unsigned int program = 0;
};
} // namespace voryx::opengl_backend

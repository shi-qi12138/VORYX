#include "opengl_draw.hpp"

#include "opengl_api.hpp"
#include "opengl_pipeline.hpp"

#include <array>

namespace voryx::opengl_backend
{
namespace
{
constexpr unsigned int arrayBuffer = 0x8892;
constexpr unsigned int floatType = 0x1406;
constexpr unsigned int scissorTest = 0x0C11;
constexpr unsigned int triangles = 0x0004;
} // namespace

DrawState::DrawState(const Api &api) noexcept
    : api(api)
{
}

DrawState::~DrawState()
{
    if (vao != 0) {
        api.useProgram(0);
        api.bindVertexArray(0);
        api.bindBuffer(arrayBuffer, 0);
        api.deleteVertexArrays(1, &vao);
    }
}

void DrawState::BeginPass(std::uint32_t height)
{
    pipeline = nullptr;
    vertexBuffer = 0;
    vertexOffset = 0;
    targetHeight = height;
    // 清除始终作用于完整附件，上一 Pass 的裁剪状态不能影响结果。
    api.disable(scissorTest);
    api.Check();
}

void DrawState::SetPipeline(const Pipeline &value) noexcept
{
    pipeline = &value;
}

void DrawState::SetVertexBuffer(unsigned int name,
                                std::uint64_t offset) noexcept
{
    vertexBuffer = name;
    vertexOffset = offset;
}

void DrawState::SetViewport(const Viewport &view)
{
    const auto bottom = static_cast<int>(targetHeight)
                        - static_cast<int>(view.y)
                        - static_cast<int>(view.height);
    api.viewport(static_cast<int>(view.x), bottom, static_cast<int>(view.width),
                 static_cast<int>(view.height));
    api.depthRange(view.minDepth, view.maxDepth);
    api.Check();
}

void DrawState::SetScissor(const Rect2D &rect)
{
    const auto bottom = targetHeight - rect.y - rect.height;
    api.scissor(static_cast<int>(rect.x), static_cast<int>(bottom),
                static_cast<int>(rect.width), static_cast<int>(rect.height));
    api.enable(scissorTest);
    api.Check();
}

void DrawState::Draw(std::uint32_t count, std::uint32_t first)
{
    if (vao == 0) {
        api.genVertexArrays(1, &vao);
        api.bindVertexArray(vao);
        api.Check();
    }
    pipeline->Use();
    for (const auto &attribute : enabledAttributes) {
        api.disableVertexAttribArray(attribute.location);
    }

    enabledAttributes = pipeline->Attributes();
    if (!enabledAttributes.empty()) {
        constexpr std::array components{2, 3, 4};
        api.bindBuffer(arrayBuffer, vertexBuffer);
        for (const auto &attribute : enabledAttributes) {
            const auto offset = static_cast<std::uintptr_t>(
                vertexOffset + attribute.offsetBytes);
            api.vertexAttribPointer(
                attribute.location,
                components[static_cast<std::size_t>(attribute.format)],
                floatType, 0, static_cast<int>(pipeline->Stride()),
                reinterpret_cast<const void *>(offset));
            api.enableVertexAttribArray(attribute.location);
        }
    }
    api.drawArrays(triangles, static_cast<int>(first), static_cast<int>(count));
    api.Check();
}
} // namespace voryx::opengl_backend

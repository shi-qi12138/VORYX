#pragma once

#include <string_view>

namespace voryx::sample_data
{
inline constexpr std::string_view triangleVertexGlsl = R"glsl(#version 330 core
layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;
out vec3 vertexColor;
void main()
{
    // 将 RHI 裁剪坐标的 Y 向下及 Z [0, w] 转为 OpenGL 约定。
    vec4 rhiPosition = vec4(position, 0.5, 1.0);
    gl_Position = vec4(rhiPosition.x, -rhiPosition.y,
                       2.0 * rhiPosition.z - rhiPosition.w, rhiPosition.w);
    vertexColor = color;
}
)glsl";

inline constexpr std::string_view triangleFragmentGlsl =
    R"glsl(#version 330 core
in vec3 vertexColor;
layout(location = 0) out vec4 fragmentColor;
void main()
{
    fragmentColor = vec4(vertexColor, 1.0);
}
)glsl";
} // namespace voryx::sample_data

#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
layout(location = 0) out vec2 texCoord;

layout(set = 0, binding = 3, std140) uniform Transform
{
    vec4 scaleOffset;
} transform;

void main()
{
    gl_Position = vec4(position.xy * transform.scaleOffset.xy + transform.scaleOffset.zw,
                       position.z, 1.0);
    texCoord = uv;
}

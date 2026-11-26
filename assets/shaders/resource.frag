#version 450
layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 7) uniform texture2D colorImage;
layout(set = 0, binding = 11) uniform sampler colorSampler;

void main()
{
    color = texture(sampler2D(colorImage, colorSampler), texCoord);
}

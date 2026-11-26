#version 450
layout(location = 0) in vec2 texCoord;
layout(set = 0, binding = 0) uniform sampler2D imageTexture;
layout(location = 0) out vec4 outputColor;
void main()
{
    outputColor = texture(imageTexture, texCoord);
}

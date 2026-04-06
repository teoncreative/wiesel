#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;
layout (location = 2) in flat uint inEntityId;

layout(location = 0) out vec4 outFragColor;
layout (location = 1) out uint outEntityId;

void main() {
    outFragColor = vec4(inColor.rgb * inColor.a, inColor.a);
    outEntityId = inEntityId;
}

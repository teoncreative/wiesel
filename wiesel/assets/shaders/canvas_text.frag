#version 450

layout(set = 0, binding = 1) uniform sampler2D fontAtlas;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outFragColor;

void main() {
    float a = inColor.a * texture(fontAtlas, inUV).r;
    outFragColor = vec4(inColor.rgb * a, a);
}

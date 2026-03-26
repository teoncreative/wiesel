#version 450

layout (set = 0, binding = 0) uniform sampler2D iconTexture;

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main() {
    vec4 texel = texture(iconTexture, inUV);
    outFragColor = texel * inColor;
}
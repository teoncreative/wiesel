#version 450

layout(set = 0, binding = 0) uniform sampler2D spriteTexture;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inTint;

layout(location = 0) out vec4 outFragColor;

void main() {
    vec4 texel = texture(spriteTexture, inUV);
    outFragColor = texel * inTint;
    outFragColor.rgb *= outFragColor.a;
}

#version 450

uint kVertexFlagHasTexture = 1 << 0;
uint kVertexFlagHasNormalMap = 1 << 1;
uint kVertexFlagHasSpecularMap = 1 << 2;
uint kVertexFlagHasHeightMap = 1 << 3;
uint kVertexFlagHasAlbedoMap = 1 << 4;
uint kVertexFlagHasRoughnessMap = 1 << 5;
uint kVertexFlagHasMetallicMap = 1 << 6;

layout(set = 0, binding = 1) uniform sampler2D baseTexture;

layout(location = 0) in vec2 inUV;
layout(location = 1) in flat uint inFlags;

void main()
{
    vec4 baseColor;
    if ((inFlags & kVertexFlagHasTexture) > 0) {
        baseColor = texture(baseTexture, inUV);
    } else {
        baseColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    if (baseColor.a < 0.5) {
        discard;
    }
}
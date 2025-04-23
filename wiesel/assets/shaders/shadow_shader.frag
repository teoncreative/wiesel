#version 450

uint VertexFlagHasTexture = 1 << 0;
uint VertexFlagHasNormalMap = 1 << 1;
uint VertexFlagHasSpecularMap = 1 << 2;
uint VertexFlagHasHeightMap = 1 << 3;
uint VertexFlagHasAlbedoMap = 1 << 4;
uint VertexFlagHasRoughnessMap = 1 << 5;
uint VertexFlagHasMetallicMap = 1 << 6;

layout(set = 0, binding = 1) uniform sampler2D baseTexture;

layout(location = 0) in vec2 inUV;
layout(location = 1) in flat uint inFlags;

void main()
{
    vec4 baseColor;
    if ((inFlags & VertexFlagHasTexture) > 0) {
        baseColor = texture(baseTexture, inUV);
    } else {
        baseColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    if (baseColor.a < 0.5) {
        discard;
    }
}
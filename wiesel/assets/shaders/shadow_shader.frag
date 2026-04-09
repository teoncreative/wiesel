#version 450

uint kVertexFlagHasTexture = 1 << 0;
uint kVertexFlagHasNormalMap = 1 << 1;
uint kVertexFlagHasSpecularMap = 1 << 2;
uint kVertexFlagHasHeightMap = 1 << 3;
uint kVertexFlagHasAlbedoMap = 1 << 4;
uint kVertexFlagHasRoughnessMap = 1 << 5;
uint kVertexFlagHasMetallicMap = 1 << 6;
uint kVertexFlagHasOpacityMap = 1 << 7;

layout(set = 0, binding = 1) uniform sampler2D baseTexture;
layout(set = 0, binding = 5) uniform sampler2D albedoMap;
layout (set = 0, binding = 8) uniform sampler2D opacityMap;

layout(location = 0) in vec2 inUV;
layout(location = 1) in flat uint inFlags;

void main()
{
    float alpha = 1.0;
    if ((inFlags & kVertexFlagHasTexture) > 0) {
        alpha = texture(baseTexture, inUV).a;
    } else if ((inFlags & kVertexFlagHasAlbedoMap) > 0) {
        alpha = texture(albedoMap, inUV).a;
    }
    if ((inFlags & kVertexFlagHasOpacityMap) > 0) {
        alpha *= texture(opacityMap, inUV).a;
    }
    if (alpha < 0.5) {
        discard;
    }
}
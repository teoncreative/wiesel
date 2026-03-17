#version 450

layout(set = 0, binding = 0) uniform sampler2D labelTexture;

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inWorldPos;

layout(location = 0) out vec4 outFragColor;

void main() {
    vec3 normal = normalize(cross(dFdx(inWorldPos), dFdy(inWorldPos)));
    vec3 absN = abs(normal);
    vec2 tileUV;

    if (absN.x >= absN.y && absN.x >= absN.z) {
        // Left/Right faces: use Z and Y
        tileUV = vec2(inWorldPos.z * sign(normal.x), -inWorldPos.y);
    } else if (absN.y >= absN.x && absN.y >= absN.z) {
        // Top/Bottom faces: use X and Z
        tileUV = vec2(inWorldPos.x, -inWorldPos.z * sign(normal.y));
    } else {
        // Front/Back faces: use X and Y
        tileUV = vec2(-inWorldPos.x * sign(normal.z), -inWorldPos.y);
    }

    vec4 texel = texture(labelTexture, tileUV);
    outFragColor = texel * inColor;
}
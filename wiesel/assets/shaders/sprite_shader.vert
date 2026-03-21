#version 450

layout(location = 0) in vec2 inUV;

layout (set = 0, binding = 1, std140) uniform Matrices {
    mat4 modelMatrix;
    vec4 tint;
    int flipX;
    int flipY;
};

layout(set = 1, binding = 1, std140) uniform Camera {
    mat4 viewMatrix;
    mat4 projection;
    mat4 invProjection;
    vec3 position;
    float _pad0;
    float near;
    float far;
    vec4 cascadeSplits;
    int enableSSAO;
    int debugCascades;
    mat4 prevViewProjection;
    vec2 taaJitterOffset;
} cam;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outTint;

const vec2 quadPos[6] = vec2[6](
    vec2(-.5,-.5),
    vec2( .5,-.5),
    vec2( .5, .5),
    vec2(-.5,-.5),
    vec2( .5, .5),
    vec2(-.5, .5)
);

void main() {
    vec2 localPos = quadPos[gl_VertexIndex];
    vec2 uv = -inUV;

    // Apply flip
    if (flipX != 0) {
        uv.x = 1.0 - uv.x;
    }
    if (flipY != 0) {
        uv.y = 1.0 - uv.y;
    }

    outUV = uv;
    outTint = tint;
    gl_Position = cam.projection * cam.viewMatrix * modelMatrix * vec4(localPos, 0, 1);
}

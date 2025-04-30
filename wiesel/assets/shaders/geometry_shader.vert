#version 450

layout(set = 0, binding = 0, std140) uniform Matrices {
    mat4 modelMatrix;
    mat3 normalMatrix;
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
} cam;

layout(location = 0) in vec3 inVertexPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inVertexNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBiTangent;
layout(location = 6) in uint inFlags;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 outUV;
layout(location = 3) out vec3 outNormal;
layout(location = 4) out vec3 outTangent;
layout(location = 5) out vec3 outBiTangent;
layout(location = 6) out uint outFlags;
layout(location = 7) out vec3 outViewDir;
layout(location = 8) out vec3 outViewPos; // view-space pos
layout(location = 9) out mat3 outTBN;

void main() {
    // world‐space
    vec4 worldPos4   = modelMatrix * vec4(inVertexPosition, 1.0);
    outWorldPos      = worldPos4.xyz;

    // view‐space
    vec4 viewPos4   = cam.viewMatrix * worldPos4;
    outViewPos      = viewPos4.xyz;

    outNormal       = mat3(modelMatrix) * inVertexNormal;
    outTangent      = mat3(modelMatrix) * inTangent;
    outBiTangent    = mat3(modelMatrix) * inBiTangent;
    outTBN          = mat3(outTangent, outBiTangent, outNormal);
    outColor = inColor;
    outUV = inUV;
    outFlags = inFlags;
    outViewDir = normalize(cam.position - outWorldPos);

    gl_Position    = cam.projection * viewPos4;
}
#version 450

#define MAX_BONES 128

layout(set = 0, binding = 0, std140) uniform Matrices {
    mat4 modelMatrix;
    mat3 normalMatrix;
    uint entityId;
    // implicit 12 bytes padding to vec4 boundary
    vec4 colorTint;
    vec4 materialParams; // x=roughness, y=metallic, z=specular
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
    vec4 ambient; // xyz=color, w=intensity
} cam;

layout(set = 2, binding = 0, std140) uniform BoneMatrices {
    mat4 bones[MAX_BONES];
} boneData;

layout(location = 0) in vec3 inVertexPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inVertexNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBiTangent;
layout(location = 6) in uint inFlags;
layout(location = 7) in ivec4 inBoneIndices;
layout(location = 8) in vec4 inBoneWeights;

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
layout (location = 12) out flat uint outEntityId;

void main() {
    vec4 localPos = vec4(inVertexPosition, 1.0);
    vec3 localNormal = inVertexNormal;
    vec3 localTangent = inTangent;
    vec3 localBiTangent = inBiTangent;

    // Skeletal animation skinning
    float totalWeight = inBoneWeights.x + inBoneWeights.y
                      + inBoneWeights.z + inBoneWeights.w;
    if (totalWeight > 0.0) {
        mat4 skin = inBoneWeights.x * boneData.bones[inBoneIndices.x]
                  + inBoneWeights.y * boneData.bones[inBoneIndices.y]
                  + inBoneWeights.z * boneData.bones[inBoneIndices.z]
                  + inBoneWeights.w * boneData.bones[inBoneIndices.w];
        localPos = skin * localPos;
        mat3 skin3 = mat3(skin);
        localNormal = skin3 * localNormal;
        localTangent = skin3 * localTangent;
        localBiTangent = skin3 * localBiTangent;
    }

    // world-space
    vec4 worldPos4   = modelMatrix * localPos;
    outWorldPos      = worldPos4.xyz;

    // view-space
    vec4 viewPos4   = cam.viewMatrix * worldPos4;
    outViewPos      = viewPos4.xyz;

    outNormal       = mat3(modelMatrix) * localNormal;
    outTangent      = mat3(modelMatrix) * localTangent;
    outBiTangent    = mat3(modelMatrix) * localBiTangent;
    outTBN          = mat3(outTangent, outBiTangent, outNormal);
    outColor = inColor;
    outUV = inUV;
    outFlags = inFlags;
    outViewDir = normalize(cam.position - outWorldPos);
    outEntityId = entityId;

    gl_Position    = cam.projection * viewPos4;
}

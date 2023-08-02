#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 modelMatrix;
    vec3 scale;
    mat3 normalMatrix;
    mat4 rotationMatrix;
    mat4 cameraViewMatrix;
    mat4 cameraProjection;
    vec3 cameraPosition;
} ubo;

layout(location = 0) in vec3 inVertexPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inVertexNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBiTangent;
layout(location = 6) in uint inFlags;

layout(location = 0) out vec3 outFragPosition;
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 outUV;
layout(location = 3) out vec3 outVertexNormal;
layout(location = 4) out vec3 outTangent;
layout(location = 5) out vec3 outBiTangent;
layout(location = 6) out uint outFlags;
layout(location = 7) out vec3 outViewDir;
layout(location = 8) out mat3 outTBN;

void main() {
    outColor = inColor;
    outUV = inUV;
    outFlags = inFlags;
    mat4 modelView = ubo.cameraViewMatrix * ubo.modelMatrix;
    gl_Position = ubo.cameraProjection * modelView * vec4(inVertexPosition, 1.0f);
    vec4 worldPosition = ubo.modelMatrix * vec4(inVertexPosition, 1.0);
    outFragPosition = worldPosition.xyz;
    outViewDir = normalize(ubo.cameraPosition - outFragPosition);

    mat3 m3_model = mat3(ubo.modelMatrix);
    outVertexNormal = normalize(m3_model * inVertexNormal);
    outTangent = normalize(m3_model * inTangent);
    outBiTangent = normalize(m3_model * inTangent);
    outTBN = mat3(outTangent, -outBiTangent, outVertexNormal);
}
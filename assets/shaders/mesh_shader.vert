#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat3 normalMatrix;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in uint inHasTexture;
layout(location = 4) in vec3 inNormal;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outUV;
layout(location = 2) out uint outHasTexture;
layout(location = 3) out vec3 outEyePos;
layout(location = 4) out vec3 outNormal;

const vec3 DIRECTION_TO_LIGHT = normalize(vec3(1.0, 6.0, 0.0));
const float AMBIENT = 0.05f;
const float DENSITY = 1.0f;

void main() {
    vec3 normalWorldSpace = normalize(ubo.normalMatrix * inNormal);
    float light = AMBIENT + max(dot(normalWorldSpace, DIRECTION_TO_LIGHT * DENSITY), 0.0);

    mat4 modelView = ubo.view * ubo.model;
    gl_Position = ubo.proj * modelView * vec4(inPosition, 1.0);
    outEyePos = vec3(-modelView);
    outColor = inColor * light;
    outUV = inUV;
    outHasTexture = inHasTexture;
    outNormal = inNormal;
}
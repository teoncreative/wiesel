#version 450

layout(set = 0, binding = 0, std140) uniform ElementData {
    vec2 position;
    vec2 size;
    vec4 color;
    vec4 uvRect;
    float entityId;
};

layout(set = 1, binding = 1, std140) uniform Camera {
    mat4 viewMatrix;
    mat4 projection;
    mat4 invProjection;
    vec3 camPosition;
    float _pad0;
    float near;
    float far;
    vec4 cascadeSplits;
    int enableSSAO;
    int debugCascades;
    mat4 prevViewProjection;
    vec2 taaJitterOffset;
    vec3 ambientColor;
    float ambientIntensity;
} cam;

layout(push_constant) uniform PushConstants {
    mat4 modelMatrix;
    vec2 canvasSize;
    vec2 worldSize;
};

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outColor;
layout(location = 2) out float outEntityId;

const vec2 quadPos[6] = vec2[6](
    vec2(0, 0),
    vec2(1, 0),
    vec2(1, 1),
    vec2(0, 0),
    vec2(1, 1),
    vec2(0, 1)
);

void main() {
    vec2 localPos = quadPos[gl_VertexIndex];

    outUV = mix(uvRect.xy, uvRect.zw, localPos);
    outColor = color;
    outEntityId = entityId;

    // Map element pixel position onto the canvas quad
    vec2 pixelPos = position + localPos * size;
    // Normalize to -0.5..+0.5 range across the canvas
    vec2 normalizedPos = (pixelPos / canvasSize) - 0.5;
    // Flip Y: canvas Y goes down, world Y goes up
    normalizedPos.y = -normalizedPos.y;
    // Scale to world units
    vec3 localWorld = vec3(normalizedPos * worldSize, 0.0);
    // Transform to world space and project
    vec4 worldPos = modelMatrix * vec4(localWorld, 1.0);
    gl_Position = cam.projection * cam.viewMatrix * worldPos;
}

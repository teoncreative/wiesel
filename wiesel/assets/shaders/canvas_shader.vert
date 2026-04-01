#version 450

layout(set = 0, binding = 0, std140) uniform ElementData {
    vec2 position;
    vec2 size;
    vec4 color;
    vec4 uvRect;
    float entityId;
    float premultiplied;
};

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
};

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outColor;
layout(location = 2) out float outEntityId;
layout (location = 3) out float outPremultiplied;

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
    outPremultiplied = premultiplied;

    vec2 pixelPos = position + localPos * size;
    vec2 ndc = (pixelPos / screenSize) * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
}

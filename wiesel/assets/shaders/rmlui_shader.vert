#version 450

layout (push_constant) uniform PushConstants {
    vec2 translation;
    vec2 screenSize;
} pc;

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 inTexCoord;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outTexCoord;

void main() {
    vec2 pos = inPosition + pc.translation;
    // Convert pixel coordinates to NDC [-1, 1]
    vec2 ndc = (pos / pc.screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    outColor = inColor;
    outTexCoord = inTexCoord;
}

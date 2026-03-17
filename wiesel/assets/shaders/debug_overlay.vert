#version 450

layout(push_constant) uniform PushData {
    mat4 mvp;
    mat4 model;
    vec4 color;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec3 outWorldPos;

void main() {
    gl_Position = mvp * vec4(inPosition, 1.0);
    outColor = color;
    outUV = inUV;
    outWorldPos = (model * vec4(inPosition, 1.0)).xyz;
}
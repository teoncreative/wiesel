#version 450

layout (push_constant) uniform PushData {
    mat4 mvp;
    vec4 color;
    uint entityId;
};

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outUV;
layout (location = 2) out flat uint outEntityId;

void main() {
    gl_Position = mvp * vec4(inPosition, 1.0);
    outColor = color;
    outUV = inUV;
    outEntityId = entityId;
}

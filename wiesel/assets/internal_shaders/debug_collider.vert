#version 450

layout(push_constant) uniform PushData {
    mat4 mvp;
    vec4 color;
};

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 outColor;

void main() {
    gl_Position = mvp * vec4(inPosition, 1.0);
    outColor = color;
}

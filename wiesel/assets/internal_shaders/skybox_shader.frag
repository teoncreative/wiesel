#version 450

layout(binding = 0) uniform samplerCube skyboxTex;
layout(location = 0) in vec3 vDirection;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(skyboxTex, vDirection);
}

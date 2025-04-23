#version 450

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = texture(texSampler, fragUV);
}

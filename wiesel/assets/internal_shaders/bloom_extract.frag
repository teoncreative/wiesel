#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputImage;

layout(push_constant) uniform BloomParams {
    float threshold;
    float intensity;
};

void main() {
    vec3 color = texture(inputImage, inUV).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float soft = clamp((brightness - threshold) / max(threshold, 0.001), 0.0, 1.0);
    soft = soft * soft;  // smooth falloff
    outColor = vec4(color * soft, 1.0);
}

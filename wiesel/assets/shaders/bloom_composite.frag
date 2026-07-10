#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneImage;
layout(set = 0, binding = 1) uniform sampler2D bloomImage;

layout(push_constant) uniform BloomParams {
    vec4 tintIntensity; // rgb=tint, a=intensity
};

void main() {
    vec3 scene = texture(sceneImage, inUV).rgb;
    vec3 bloom = texture(bloomImage, inUV).rgb;
    outColor = vec4(scene + bloom * tintIntensity.a * tintIntensity.rgb, 1.0);
}

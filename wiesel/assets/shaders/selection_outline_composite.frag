#version 450

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D sceneImage;
layout (set = 0, binding = 1) uniform sampler2D blurredMask;
layout (set = 0, binding = 2) uniform sampler2D originalMask;

layout (push_constant) uniform OutlineParams {
    vec4 outlineColor;
};

void main() {
    vec3 scene = texture(sceneImage, inUV).rgb;
    float blur = texture(blurredMask, inUV).r;
    float mask = texture(originalMask, inUV).r;

    // Outline = blurred mask minus original mask
    float raw = clamp(blur - mask, 0.0, 1.0);

    // Remap: solid for the inner portion, fade for the outer
    // smoothstep(low, high, raw) - values above 'high' become 1.0 (solid)
    // values between 'low' and 'high' fade smoothly
    float outline = raw < 0.3 ? smoothstep(0.0, 0.3, raw) : 1.0;

    outColor = vec4(mix(scene, outlineColor.rgb, outline * outlineColor.a), 1.0);
}

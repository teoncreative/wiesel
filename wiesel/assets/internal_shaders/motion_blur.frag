#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneImage;
layout(set = 0, binding = 1) uniform sampler2D worldPosImage;

layout(set = 1, binding = 1, std140) uniform Camera {
    mat4 viewMatrix;
    mat4 projection;
    mat4 invProjection;
    vec3 position;
    float _pad0;
    float near;
    float far;
    vec4 cascadeSplits;
    int enableSSAO;
    int debugCascades;
    mat4 prevViewProjection;
    vec2 taaJitterOffset;
} cam;

layout(push_constant) uniform MotionBlurParams {
    float strength;
    int numSamples;
};

void main() {
    vec4 worldPos = texture(worldPosImage, inUV);
    // Skip background pixels (no geometry)
    if (worldPos.w == 0.0) {
        outColor = texture(sceneImage, inUV);
        return;
    }

    // Project world position to previous frame's clip space
    vec4 prevClip = cam.prevViewProjection * vec4(worldPos.xyz, 1.0);
    vec2 prevNDC = prevClip.xy / prevClip.w;
    vec2 prevUV = prevNDC * 0.5 + 0.5;

    // Compute screen-space velocity
    vec2 velocity = (inUV - prevUV) * strength;

    // Clamp velocity to avoid excessive blur
    float maxVel = 0.05;
    float velLen = length(velocity);
    if (velLen > maxVel) {
        velocity = velocity * (maxVel / velLen);
    }

    // Sample along the motion vector
    vec3 result = vec3(0.0);
    int samples = max(numSamples, 2);
    for (int i = 0; i < samples; i++) {
        float t = float(i) / float(samples - 1) - 0.5;
        vec2 sampleUV = clamp(inUV + velocity * t, 0.0, 1.0);
        result += texture(sceneImage, sampleUV).rgb;
    }
    result /= float(samples);
    outColor = vec4(result, 1.0);
}

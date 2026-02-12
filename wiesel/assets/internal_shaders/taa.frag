#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D currentImage;
layout(set = 0, binding = 1) uniform sampler2D historyImage;
layout(set = 0, binding = 2) uniform sampler2D depthImage;

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

const float BLEND_FACTOR = 0.9;

vec3 RGB2YCoCg(vec3 rgb) {
    return vec3(
         0.25 * rgb.r + 0.5 * rgb.g + 0.25 * rgb.b,
         0.5  * rgb.r                - 0.5  * rgb.b,
        -0.25 * rgb.r + 0.5 * rgb.g - 0.25 * rgb.b
    );
}

vec3 YCoCg2RGB(vec3 ycocg) {
    return vec3(
        ycocg.x + ycocg.y - ycocg.z,
        ycocg.x            + ycocg.z,
        ycocg.x - ycocg.y - ycocg.z
    );
}

vec3 clipAABB(vec3 aabbMin, vec3 aabbMax, vec3 history) {
    vec3 center = 0.5 * (aabbMax + aabbMin);
    vec3 extent = 0.5 * (aabbMax - aabbMin) + 0.001;
    vec3 offset = history - center;
    vec3 ts = abs(offset / extent);
    float t = max(ts.x, max(ts.y, ts.z));
    if (t > 1.0)
        return center + offset / t;
    return history;
}

void main() {
    // Sample current frame at jittered position
    vec3 currentColor = texture(currentImage, inUV).rgb;
    float depth = texture(depthImage, inUV).r;

    // Reconstruct world position from jittered frame
    vec2 ndc = inUV * 2.0 - 1.0;

    // If your invProjection doesn't include jitter, subtract it from NDC:
    ndc -= cam.taaJitterOffset * 2.0;

    vec4 clipPos = vec4(ndc, depth, 1.0);
    vec4 viewPos = cam.invProjection * clipPos;
    viewPos /= viewPos.w;
    vec4 worldPos = inverse(cam.viewMatrix) * viewPos;

    // Reproject to previous frame
    vec4 prevClip = cam.prevViewProjection * vec4(worldPos.xyz, 1.0);
    vec2 prevUV = (prevClip.xy / prevClip.w) * 0.5 + 0.5;

    // Compute 3x3 neighborhood AABB around current (jittered) position
    vec2 texelSize = 1.0 / textureSize(currentImage, 0);
    vec3 neighborMin = vec3(999.0);
    vec3 neighborMax = vec3(-999.0);

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 sampleUV = inUV + vec2(x, y) * texelSize;  // Use inUV!
            vec3 sample_ycocg = RGB2YCoCg(texture(currentImage, sampleUV).rgb);
            neighborMin = min(neighborMin, sample_ycocg);
            neighborMax = max(neighborMax, sample_ycocg);
        }
    }

    // Sample and clamp history
    vec3 historyColor = texture(historyImage, prevUV).rgb;
    vec3 history_ycocg = RGB2YCoCg(historyColor);
    vec3 clipped_ycocg = clipAABB(neighborMin, neighborMax, history_ycocg);
    vec3 clippedHistory = YCoCg2RGB(clipped_ycocg);

    // Reject history if outside screen or on skybox
    float blend = BLEND_FACTOR;
    if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
        blend = 0.0;
    }

    // Reject on skybox (adjust threshold for your depth range)
    if (depth >= 1.0) {  // or depth == 0.0 for reverse-Z
                         blend = 0.0;
    }

    vec3 result = mix(currentColor, clippedHistory, blend);
    outColor = vec4(result, 1.0);
}
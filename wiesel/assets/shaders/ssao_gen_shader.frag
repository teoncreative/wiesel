#version 450

layout(constant_id = 0) const int SSAO_KERNEL_SIZE = 32;
layout(constant_id = 1) const float SSAO_RADIUS = 1.5;

layout(set = 0, binding = 0) uniform sampler2D samplerViewPos;
layout(set = 0, binding = 1) uniform sampler2D samplerNormal;
layout(set = 0, binding = 2) uniform sampler2D samplerDepth;
layout(set = 0, binding = 3) uniform sampler2D ssaoNoise;

layout(set = 0, binding = 4, std140) uniform SSAOKernel {
    vec4 samples[SSAO_KERNEL_SIZE];
} ssaoKernel;

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
    vec4 ambient; // xyz=color, w=intensity
} cam;

layout (location = 0) in vec2 inUV;
layout (location = 0) out float outFragColor;

vec3 getNoise(vec2 uv) {
    // Tile the noise texture across the screen
    ivec2 texDim = textureSize(samplerViewPos, 0);
    ivec2 noiseDim = textureSize(ssaoNoise, 0);
    vec2 noiseUV = vec2(float(texDim.x)/float(noiseDim.x), float(texDim.y)/float(noiseDim.y)) * uv;
    // ssaoNoise is R32G32B32A32_SFLOAT, values already in [-1,1]
    vec3 n = texture(ssaoNoise, noiseUV).xyz;
    n.z = 0.0; // keep rotation in tangent plane
    return normalize(n);
}

void main() {
    float linearDepth = texture(samplerDepth, inUV).r;

    // Reconstruct view-space position (camera looks down -Z)
    vec2 ndc = inUV * 2.0 - 1.0;
    vec4 clip = vec4(ndc, 1.0, 1.0);
    vec4 vd   = cam.invProjection * clip;
    vec3 viewRay = vd.xyz / vd.w;
    vec3 viewPos = viewRay * (-linearDepth / viewRay.z);
    // viewPos.z is now negative (standard view space)

    // Decode world-space normal → view space
    vec3 worldNormal = normalize(texture(samplerNormal, inUV).rgb * 2.0 - 1.0);
    vec3 normal = normalize(mat3(cam.viewMatrix) * worldNormal);

    vec3 randomVec = getNoise(inUV);

    // Create TBN matrix (view space)
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(tangent, normal);
    mat3 TBN = mat3(tangent, bitangent, normal);

    // Calculate occlusion
    float occlusion = 0.0;
    int validSamples = 0;
    const float bias = 0.01 + 0.005 * linearDepth;
    for(int i = 0; i < SSAO_KERNEL_SIZE; i++) {
        vec3 samplePos = viewPos + TBN * ssaoKernel.samples[i].xyz * SSAO_RADIUS;

        // Project sample to screen UV
        vec4 offset = cam.projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;

        // Skip samples that fall outside the screen
        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0) {
            continue;
        }
        validSamples++;

        // Compare positive linear depths
        float sampleLinearDepth = -samplePos.z;
        float actualDepth = texture(samplerDepth, offset.xy).r;

        float rangeCheck = 1.0 - smoothstep(0.0, SSAO_RADIUS,
                                           abs(linearDepth - actualDepth));
        if (actualDepth + bias < sampleLinearDepth) {
            occlusion += rangeCheck;
        }
    }
    float strength = 1.5;
    occlusion = 1.0 - (occlusion / max(float(validSamples), 1.0));
    outFragColor = pow(occlusion, strength);
}
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
} cam;

layout (location = 0) in vec2 inUV;
layout (location = 0) out float outFragColor;

float rand(vec2 co) {
    return fract(sin(dot(co,vec2(12.9898,78.233)))*43758.5453);
}

vec3 getNoise(vec2 uv){
    float r1 = rand(uv * 0.5);
    float r2 = rand(uv * 1.3);
    float r3 = rand(uv * 2.7);
    return normalize(vec3(r1, r2, r3) * 2.0 - 1.0);
}

vec3 getNoiseLookup(vec2 uv) {
    // Get a random vector using a noise lookup
    ivec2 texDim = textureSize(samplerViewPos, 0);
    ivec2 noiseDim = textureSize(ssaoNoise, 0);
    const vec2 noiseUV = vec2(float(texDim.x)/float(noiseDim.x), float(texDim.y)/(noiseDim.y)) * uv;
    return texture(ssaoNoise, noiseUV).xyz * 2.0 - 1.0;
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
    const float bias = 0.005 * linearDepth;
    for(int i = 0; i < SSAO_KERNEL_SIZE; i++) {
        vec3 samplePos = viewPos + TBN * ssaoKernel.samples[i].xyz * SSAO_RADIUS;

        // Project sample to screen UV
        vec4 offset = cam.projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;

        // Compare positive linear depths
        float sampleLinearDepth = -samplePos.z;
        float actualDepth = texture(samplerDepth, offset.xy).r;

        float rangeCheck = smoothstep(0.0, 1.0,
                                       SSAO_RADIUS / abs(linearDepth - actualDepth));
        if (actualDepth + bias < sampleLinearDepth) {
            occlusion += rangeCheck;
        }
        if (occlusion > float(SSAO_KERNEL_SIZE) * 0.9) {
            break;
        }
    }
    float strength = 2.5;
    occlusion = 1.0 - (occlusion / float(SSAO_KERNEL_SIZE));
    outFragColor = pow(occlusion, strength);
}
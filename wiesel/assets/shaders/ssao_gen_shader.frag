#version 450

layout(constant_id = 0) const int SSAO_KERNEL_SIZE = 64;
layout(constant_id = 1) const float SSAO_RADIUS = 0.5;

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

void main() {
    //vec3 viewPos = texture(samplerViewPos, inUV).rgb;
    float linearDepth = texture(samplerDepth, inUV).r;
    //float linearDepth = texture(samplerViewPos, inUV).w;
    vec2 ndc = inUV * 2.0 - 1.0;
    vec4 clip = vec4(ndc, 1.0, 1.0);
    vec4 vd   = cam.invProjection * clip;
    vec3 viewDir = normalize(vd.xyz / vd.w);
    vec3 viewPos = viewDir * linearDepth;

    // Get G-Buffer values
    vec3 normal = normalize(texture(samplerNormal, inUV).rgb * 2.0 - 1.0);

    // Get a random vector using a noise lookup
    ivec2 texDim = textureSize(samplerViewPos, 0);
    ivec2 noiseDim = textureSize(ssaoNoise, 0);
    const vec2 noiseUV = vec2(float(texDim.x)/float(noiseDim.x), float(texDim.y)/(noiseDim.y)) * inUV;
    vec3 randomVec = texture(ssaoNoise, noiseUV).xyz * 2.0 - 1.0;

    // Create TBN matrix
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(tangent, normal);
    mat3 TBN = mat3(tangent, bitangent, normal);

    // Calculate occlusion value
    float occlusion = 0.0f;
    // remove banding
    const float bias = 0.025f * linearDepth;
    for(int i = 0; i < SSAO_KERNEL_SIZE; i++) {
        vec3 sampleOffset = TBN * ssaoKernel.samples[i].xyz * SSAO_RADIUS;
        vec3 samplePos    = viewPos + sampleOffset;

        // project
        vec4 offset = vec4(samplePos, 1.0f);
        offset = cam.projection * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5f + 0.5f;

        float sampleDist  = length(samplePos);                   // distance from camera
        float actualDepth = texture(samplerDepth, offset.xy).r;  // positive linear
        float rangeCheck  = smoothstep(0.0, 1.0,
                                       SSAO_RADIUS / abs(linearDepth - sampleDist));
        if (actualDepth + bias < sampleDist) {
            occlusion += rangeCheck;
        }
    }
    float strength = 1.0f;
    occlusion = 1.0 - (occlusion / float(SSAO_KERNEL_SIZE));
    outFragColor = pow(occlusion, strength);
}
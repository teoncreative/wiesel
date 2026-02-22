#version 450

#define SHADOW_MAP_CASCADE_COUNT 4

layout(set = 0, binding = 0) uniform sampler2D samplerViewPos;
layout(set = 0, binding = 1) uniform sampler2D samplerWorldPos;
layout(set = 0, binding = 2) uniform sampler2D samplerDepth;
layout(set = 0, binding = 3) uniform sampler2D samplerNormal;
layout(set = 0, binding = 4) uniform sampler2D samplerAlbedo;
layout(set = 0, binding = 5) uniform sampler2D samplerMaterial;
layout(set = 1, binding = 0) uniform sampler2D samplerSSAO;

struct LightBase {
    vec3 position;
    float _pad0;
    vec3 color;
    float _pad1;
    float ambient;
    float diffuse;
    float specular;
    float density;
};

struct LightDirect {
    vec3 direction;
    float _pad;
    LightBase base;
};

struct LightPoint {
    LightBase base;

    float constant;
    float linear;
    float exp;
};

const int MAX_LIGHTS = 16;
layout(set = 2, binding = 0) uniform LightsBufferObject {
    int directLightCount;
    int pointLightCount;
    LightDirect directLights[MAX_LIGHTS];
    LightPoint pointLights[MAX_LIGHTS];
} lights;

layout(set = 2, binding = 1, std140) uniform Camera {
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

layout(set = 2, binding = 2) uniform ShadowMapMatrices {
    mat4 viewProjectionMatrix[SHADOW_MAP_CASCADE_COUNT];
    int enableShadows;
} shadowMatrices;


layout(set = 2, binding = 3) uniform sampler2DArray shadowMap;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outFragColor;


const mat4 biasMat = mat4(
0.5, 0.0, 0.0, 0.0,
0.0, 0.5, 0.0, 0.0,
0.0, 0.0, 1.0, 0.0,
0.5, 0.5, 0.0, 1.0
);

// Returns 0.0 = fully shadowed, 1.0 = fully lit
float calculateShadow(vec4 shadowCoord, uint cascadeIndex) {
    shadowCoord /= shadowCoord.w;

    if (shadowCoord.z < 0.0 || shadowCoord.z > 1.0 ||
    shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
    shadowCoord.y < 0.0 || shadowCoord.y > 1.0) {
        return 1.0;
    }

    // Minimal depth bias: the shadow pass uses front-face culling which
    // already provides natural self-shadowing prevention.
    float bias = 0.00005;

    ivec2 smSize = textureSize(shadowMap, 0).xy;
    vec2 texelSize = 1.0 / vec2(smSize);
    #ifdef USE_GATHER
    vec4 depths = textureGather(shadowMap, vec3(shadowCoord.xy, cascadeIndex));
    float sum = 0.0;
    sum += (shadowCoord.z - bias > depths.x) ? 1.0 : 0.0;
    sum += (shadowCoord.z - bias > depths.y) ? 1.0 : 0.0;
    sum += (shadowCoord.z - bias > depths.z) ? 1.0 : 0.0;
    sum += (shadowCoord.z - bias > depths.w) ? 1.0 : 0.0;
    return 1.0 - sum * 0.25;
    #else
    float shadow = 0.0;
    int count  = 0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 off = shadowCoord.xy + vec2(x, y) * texelSize;
            float d = texture(shadowMap, vec3(off, cascadeIndex)).r;
            shadow += (shadowCoord.z - bias > d) ? 1.0 : 0.0;
            count++;
        }
    }
    return 1.0 - (shadow / float(count));
    #endif
}

void main() {
    vec4 viewData = texture(samplerViewPos, inUV);
    vec3 viewPos = viewData.rgb;
    float linearDepth = viewData.w;

    vec3 worldPos = texture(samplerWorldPos, inUV).rgb;
    vec3 normal = normalize(texture(samplerNormal, inUV).rgb * 2.0 - 1.0);
    vec4 albedo = texture(samplerAlbedo, inUV);
    if (albedo.a < 0.5) {
        discard;
    }
    vec3 material = texture(samplerMaterial, inUV).rgb; // specular, roughnes, metallic
    float ambientOcclusion;
    if (cam.enableSSAO != 0) {
        ambientOcclusion = texture(samplerSSAO, inUV).r;
    } else {
        ambientOcclusion = 1.0f;
    }
    vec3 viewDir = normalize(cam.position - worldPos);

    // Directional light (shadow-receiving)
    vec3 sunAmbientContrib = vec3(0.0);
    vec3 sunDiffSpecContrib = vec3(0.0);
    float sunAmbient = 0.0;
    vec3 sunDir = vec3(0.0);
    for (int i = 0; i < lights.directLightCount; i++) {
        LightDirect light = lights.directLights[i];
        sunDir = normalize(light.direction);
        sunAmbient = light.base.ambient;

        float diff = light.base.diffuse * max(dot(normal, sunDir), 0.0);

        float spec = 0.0;
        if (diff > 0.0) {
            vec3 halfwayDir = normalize(sunDir + viewDir);
            spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0) * light.base.specular;
        }

        // Separate ambient (not shadowed) from diffuse+specular (shadowed)
        sunAmbientContrib = sunAmbient * albedo.rgb * ambientOcclusion * light.base.color * light.base.density;
        sunDiffSpecContrib = (diff * albedo.rgb + spec * vec3(1.0)) * light.base.color * light.base.density;
        break;
    }

    // Point lights (not affected by directional shadow)
    vec3 pointResult = vec3(0.0);
    for (int i = 0; i < lights.pointLightCount; i++) {
        LightPoint light = lights.pointLights[i];
        vec3 pDir = normalize(light.base.position - worldPos);
        float dist = length(light.base.position - worldPos);
        float atten = 1.0 / (light.constant + light.linear * dist + light.exp * dist * dist);

        float pAmbient = light.base.ambient * atten;
        float pDiff = light.base.diffuse * max(dot(normal, pDir), 0.0) * atten;

        float pSpec = 0.0;
        if (pDiff > 0.0) {
            vec3 halfwayDir = normalize(pDir + viewDir);
            pSpec = pow(max(dot(normal, halfwayDir), 0.0), 32.0) * light.base.specular * atten;
        }

        pointResult += (pAmbient * albedo.rgb * ambientOcclusion + pDiff * albedo.rgb + pSpec * vec3(1.0)) * light.base.color * light.base.density;
    }

    // Shadow (only affects directional diffuse+specular)
    float shadow = 1.0;
    uint cascadeIndex = 0;
    if (shadowMatrices.enableShadows != 0 && sunAmbient > 0.0) {
        for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; ++i) {
            if (viewPos.z < cam.cascadeSplits[i]) {
                cascadeIndex = i + 1;
            }
        }
        // Normal offset: shift the shadow lookup position along the surface normal.
        // This prevents contact shadow loss without depth-bias artifacts.
        // Scale offset by sin(theta) so grazing surfaces get more offset.
        float nCos = clamp(dot(normal, sunDir), 0.0, 1.0);
        float nSin = sqrt(1.0 - nCos * nCos);
        vec3 shadowWorldPos = worldPos + normal * (0.03 * nSin + 0.005);
        vec4 shadowCoord = biasMat * shadowMatrices.viewProjectionMatrix[cascadeIndex] * vec4(shadowWorldPos, 1.0);
        shadow = calculateShadow(shadowCoord, cascadeIndex);
    }
    vec3 finalColor = clamp(sunAmbientContrib + sunDiffSpecContrib * shadow + pointResult, 0.0, 1.0);

    if (cam.debugCascades != 0) {
        const vec3 cascadeColors[4] = vec3[](
            vec3(1.0, 0.2, 0.2),  // red = cascade 0 (nearest)
            vec3(0.2, 1.0, 0.2),  // green = cascade 1
            vec3(0.2, 0.2, 1.0),  // blue = cascade 2
            vec3(1.0, 1.0, 0.2)   // yellow = cascade 3 (farthest)
        );
        finalColor = mix(finalColor, cascadeColors[cascadeIndex], 0.35);
    }

    outFragColor = vec4(finalColor, albedo.a);
}

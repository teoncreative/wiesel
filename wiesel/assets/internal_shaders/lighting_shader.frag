#version 450

#define SHADOW_MAP_CASCADE_COUNT 4

layout(set = 0, binding = 0) uniform sampler2D samplerViewPos;
layout(set = 0, binding = 1) uniform sampler2D samplerWorldPos;
layout(set = 0, binding = 2) uniform sampler2D samplerDepth;
layout(set = 0, binding = 3) uniform sampler2D samplerNormal;
layout(set = 0, binding = 4) uniform sampler2D samplerAlbedo;
layout(set = 0, binding = 5) uniform sampler2D samplerMaterial;
layout(set = 1, binding = 0) uniform sampler2D samplerSSAO;

#ifdef USE_RT_SHADOWS
layout(set = 3, binding = 0, r32ui) readonly uniform uimage2D rtShadowMask;
#endif

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


layout(set = 2, binding = 3) uniform sampler2DArrayShadow shadowMap;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outFragColor;


const mat4 biasMat = mat4(
0.5, 0.0, 0.0, 0.0,
0.0, 0.5, 0.0, 0.0,
0.0, 0.0, 1.0, 0.0,
0.5, 0.5, 0.0, 1.0
);

// 16-sample Poisson disk for shadow filtering
const vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870),
    vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845),
    vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554),
    vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507),
    vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367),
    vec2( 0.14383161, -0.14100790)
);

// Per-pixel pseudo-random rotation to break banding
float interleavedGradientNoise(vec2 screenPos) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(screenPos, magic.xy)));
}

// Sample shadow for a single cascade using Poisson disk PCF
float sampleShadowCascade(vec4 shadowCoord, uint cascadeIndex, float filterRadius, float slopeBias) {
    shadowCoord /= shadowCoord.w;

    if (shadowCoord.z < 0.0 || shadowCoord.z > 1.0 ||
        shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0) {
        return 1.0;
    }

    // Depth bias: pull the receiver slightly toward the light to prevent acne
    // without the large spatial offset that causes contact-edge leaking
    float refZ = shadowCoord.z - slopeBias;

    vec2 screenPos = gl_FragCoord.xy;
    float angle = interleavedGradientNoise(screenPos) * 6.283185;
    float s = sin(angle);
    float c = cos(angle);
    mat2 rotation = mat2(c, s, -s, c);

    ivec2 smSize = textureSize(shadowMap, 0).xy;
    vec2 texelSize = 1.0 / vec2(smSize);

    float shadow = 0.0;
    for (int i = 0; i < 16; ++i) {
        vec2 offset = rotation * poissonDisk[i] * filterRadius * texelSize;
        vec2 sampleUV = shadowCoord.xy + offset;
        shadow += texture(shadowMap, vec4(sampleUV, float(cascadeIndex), refZ));
    }
    return shadow / 16.0;
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
    vec3 material = texture(samplerMaterial, inUV).rgb; // specular, roughness, metallic
    float matSpecular = material.r;
    float matRoughness = material.g;
    float matMetallic = material.b;

    // Derive shininess from roughness: rough surfaces have broad, dim highlights
    float shininess = max(2.0, pow(2.0, 10.0 * (1.0 - matRoughness)));

    // Metallic surfaces tint specular with albedo color and reduce diffuse
    float ambientOcclusion;
    if (cam.enableSSAO != 0) {
        ambientOcclusion = texture(samplerSSAO, inUV).r;
    } else {
        ambientOcclusion = 1.0f;
    }
    vec3 viewDir = normalize(cam.position - worldPos);

#ifdef USE_RT_SHADOWS
    // RT shadows: per-light shadow bitmask in r32ui image.
    // Bit i = 1 means light i is visible (lit), 0 means shadowed.
    // Order matches the RT shadow UBO: directional lights first, then point lights.
    uint rtMask = imageLoad(rtShadowMask, ivec2(gl_FragCoord.xy)).r;
    int rtLightIndex = 0;

    vec3 diffuseColor = albedo.rgb * (1.0 - matMetallic);
    vec3 specularColor = mix(vec3(matSpecular * 0.5), albedo.rgb, matMetallic);

    // Directional lights with per-light RT shadow
    vec3 dirResult = vec3(0.0);
    for (int i = 0; i < lights.directLightCount; i++) {
        LightDirect light = lights.directLights[i];
        vec3 lDir = normalize(light.direction);

        float diff = light.base.diffuse * max(dot(normal, lDir), 0.0);
        float spec = 0.0;
        if (diff > 0.0) {
            vec3 halfwayDir = normalize(lDir + viewDir);
            spec = pow(max(dot(normal, halfwayDir), 0.0), shininess) * light.base.specular;
        }

        vec3 ambient = light.base.ambient * diffuseColor * ambientOcclusion * light.base.color * light.base.density;
        vec3 diffSpec = (diff * diffuseColor + spec * specularColor) * light.base.color * light.base.density;

        float shadow = float((rtMask >> rtLightIndex) & 1u);
        rtLightIndex++;

        dirResult += ambient + diffSpec * shadow;
    }

    // Point lights with per-light RT shadow
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
            pSpec = pow(max(dot(normal, halfwayDir), 0.0), shininess) * light.base.specular * atten;
        }

        float shadow = float((rtMask >> rtLightIndex) & 1u);
        rtLightIndex++;

        vec3 ambient = pAmbient * diffuseColor * ambientOcclusion;
        vec3 diffSpec = pDiff * diffuseColor + pSpec * specularColor;
        pointResult += (ambient + diffSpec * shadow) * light.base.color * light.base.density;
    }

    vec3 finalColor = clamp(dirResult + pointResult, 0.0, 1.0);

    if (cam.debugCascades == 2) {
        finalColor = vec3(matRoughness, matMetallic, matSpecular);
    }

    outFragColor = vec4(finalColor, albedo.a);
#else
    // Cascaded shadow map path (shadow only on first directional light)
    vec3 diffuseColor = albedo.rgb * (1.0 - matMetallic);
    vec3 specularColor = mix(vec3(matSpecular * 0.5), albedo.rgb, matMetallic);

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
            spec = pow(max(dot(normal, halfwayDir), 0.0), shininess) * light.base.specular;
        }

        sunAmbientContrib = sunAmbient * diffuseColor * ambientOcclusion * light.base.color * light.base.density;
        sunDiffSpecContrib = (diff * diffuseColor + spec * specularColor) * light.base.color * light.base.density;
        break;
    }

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
            pSpec = pow(max(dot(normal, halfwayDir), 0.0), shininess) * light.base.specular * atten;
        }

        pointResult += (pAmbient * diffuseColor * ambientOcclusion + pDiff * diffuseColor + pSpec * specularColor) * light.base.color * light.base.density;
    }

    float shadow = 1.0;
    uint cascadeIndex = 0;
    if (shadowMatrices.enableShadows != 0 && sunAmbient > 0.0) {
        for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; ++i) {
            if (viewPos.z < cam.cascadeSplits[i]) {
                cascadeIndex = i + 1;
            }
        }

        float nCos = clamp(dot(normal, sunDir), 0.0, 1.0);
        float nSin = sqrt(1.0 - nCos * nCos);
        vec3 shadowWorldPos = worldPos + normal * (0.008 * nSin + 0.001);

        float filterRadius = 1.5 + float(cascadeIndex) * 0.5;

        vec4 shadowCoord = biasMat * shadowMatrices.viewProjectionMatrix[cascadeIndex] * vec4(shadowWorldPos, 1.0);
        shadow = sampleShadowCascade(shadowCoord, cascadeIndex, filterRadius, 0.0);

        if (cascadeIndex < SHADOW_MAP_CASCADE_COUNT - 1) {
            float splitDepth = cam.cascadeSplits[cascadeIndex];
            float prevSplit = (cascadeIndex == 0) ? 0.0 : cam.cascadeSplits[cascadeIndex - 1];
            float cascadeRange = splitDepth - prevSplit;
            float blendZone = cascadeRange * 0.15;
            float distToEdge = viewPos.z - splitDepth;

            if (distToEdge > -blendZone && distToEdge < 0.0) {
                float blendFactor = smoothstep(0.0, 1.0, (distToEdge + blendZone) / blendZone);
                uint nextCascade = cascadeIndex + 1;
                float nextFilterRadius = 1.5 + float(nextCascade) * 0.5;
                vec4 nextShadowCoord = biasMat * shadowMatrices.viewProjectionMatrix[nextCascade] * vec4(shadowWorldPos, 1.0);
                float nextShadow = sampleShadowCascade(nextShadowCoord, nextCascade, nextFilterRadius, 0.0);
                shadow = mix(shadow, nextShadow, blendFactor);
            }
        }
    }

    vec3 finalColor = clamp(sunAmbientContrib + sunDiffSpecContrib * shadow + pointResult, 0.0, 1.0);

    if (cam.debugCascades == 1) {
        const vec3 cascadeColors[4] = vec3[](
            vec3(1.0, 0.2, 0.2),  // red = cascade 0 (nearest)
            vec3(0.2, 1.0, 0.2),  // green = cascade 1
            vec3(0.2, 0.2, 1.0),  // blue = cascade 2
            vec3(1.0, 1.0, 0.2)   // yellow = cascade 3 (farthest)
        );
        finalColor = mix(finalColor, cascadeColors[cascadeIndex], 0.35);
    } else if (cam.debugCascades == 2) {
        // Debug material G-buffer: R=roughness G=metallic B=specular
        finalColor = vec3(matRoughness, matMetallic, matSpecular);
    }

    outFragColor = vec4(finalColor, albedo.a);
#endif
}
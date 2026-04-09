#version 450

uint kVertexFlagHasTexture = 1 << 0;
uint kVertexFlagHasNormalMap = 1 << 1;
uint kVertexFlagHasSpecularMap = 1 << 2;
uint kVertexFlagHasHeightMap = 1 << 3;
uint kVertexFlagHasAlbedoMap = 1 << 4;
uint kVertexFlagHasRoughnessMap = 1 << 5;
uint kVertexFlagHasMetallicMap = 1 << 6;
uint kVertexFlagHasOpacityMap = 1 << 7;

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

// Set 0: per-mesh data (same as geometry shader)
layout (set = 0, binding = 0, std140) uniform Matrices {
    mat4 modelMatrix;
    mat3 normalMatrix;
    uint entityId;
    vec4 colorTint;
    vec4 materialParams; // x=roughness, y=metallic, z=specular
};

#ifdef USE_IBL
layout (set = 3, binding = 0) uniform samplerCube irradianceMap;
layout (set = 3, binding = 1) uniform samplerCube prefilterMap;
layout (set = 3, binding = 2) uniform sampler2D brdfLUT;
#endif

layout(set = 0, binding = 1) uniform sampler2D baseTexture; // diffuse
layout(set = 0, binding = 2) uniform sampler2D normalMap;
layout(set = 0, binding = 3) uniform sampler2D specularMap;
layout(set = 0, binding = 4) uniform sampler2D heightMap;
layout(set = 0, binding = 5) uniform sampler2D albedoMap;
layout(set = 0, binding = 6) uniform sampler2D roughnessMap;
layout(set = 0, binding = 7) uniform sampler2D metallicMap;
layout (set = 0, binding = 8) uniform sampler2D opacityMap;

// Set 1: global data (lights + camera)
const int MAX_LIGHTS = 16;
layout(set = 1, binding = 0) uniform LightsBufferObject {
    int directLightCount;
    int pointLightCount;
    LightDirect directLights[MAX_LIGHTS];
    LightPoint pointLights[MAX_LIGHTS];
} lights;

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

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBiTangent;
layout(location = 6) in flat uint inFlags;
layout(location = 7) in vec3 inViewDir;
layout(location = 8) in vec3 inViewPos;
layout(location = 9) in mat3 inTBN;
layout (location = 12) in flat uint inEntityId;

layout(location = 0) out vec4 outFragColor;

vec3 getSurfaceNormal() {
    if ((inFlags & kVertexFlagHasNormalMap) > 0) {
        vec3 localNormal = 2.0 * texture(normalMap, inUV).rgb - 1.0;
        return normalize(inTBN * localNormal);
    }
    return normalize(inNormal);
}

void main() {
    // Sample base color with alpha
    vec4 baseColor;
    if ((inFlags & kVertexFlagHasTexture) > 0) {
        baseColor = texture(baseTexture, inUV);
    } else if ((inFlags & kVertexFlagHasAlbedoMap) > 0) {
        baseColor = texture(albedoMap, inUV);
    } else if ((inFlags & kVertexFlagHasOpacityMap) > 0) {
        baseColor = texture(opacityMap, inUV);
    } else {
        baseColor = vec4(1.0, 1.0, 1.0, 1.0);
    }
    baseColor *= colorTint;

    if (baseColor.a < 0.01) {
        discard;
    }

    // Material properties
    float matSpecular;
    if ((inFlags & kVertexFlagHasSpecularMap) > 0) {
        matSpecular = texture(specularMap, inUV).r * materialParams.z;
    } else {
        matSpecular = materialParams.z;
    }
    float matRoughness;
    if ((inFlags & kVertexFlagHasRoughnessMap) > 0) {
        matRoughness = texture(roughnessMap, inUV).r * materialParams.x;
    } else {
        matRoughness = materialParams.x;
    }
    float matMetallic;
    if ((inFlags & kVertexFlagHasMetallicMap) > 0) {
        matMetallic = texture(metallicMap, inUV).r * materialParams.y;
    } else {
        matMetallic = materialParams.y;
    }

    vec3 normal = getSurfaceNormal();
    vec3 albedo = inColor * baseColor.rgb;
    vec3 viewDir = normalize(cam.position - inWorldPos);

    float shininess = max(2.0, pow(2.0, 10.0 * (1.0 - matRoughness)));

    // PBR Fresnel
    vec3 F0 = mix(vec3(0.04 * matSpecular), albedo, matMetallic);
    vec3 diffuseColor = albedo * (1.0 - matMetallic);

    // Inline forward lighting (no shadows, no SSAO)
    vec3 result = vec3(0.0);

    // Directional lights
    for (int i = 0; i < lights.directLightCount; i++) {
        LightDirect light = lights.directLights[i];
        vec3 lDir = -normalize(light.direction);
        float NdotL = max(dot(normal, lDir), 0.0);

        vec3 specContrib = vec3(0.0);
        vec3 kD = vec3(1.0);
        if (NdotL > 0.0) {
            vec3 halfwayDir = normalize(lDir + viewDir);
            float NdotH = max(dot(normal, halfwayDir), 0.0);
            float HdotV = max(dot(halfwayDir, viewDir), 0.0);
            vec3 F = F0 + (1.0 - F0) * pow(1.0 - HdotV, 5.0);
            kD = (1.0 - F) * (1.0 - matMetallic);
            float specPower = pow(NdotH, shininess) * (shininess + 2.0) / 8.0;
            specContrib = F * specPower * light.base.specular;
        }

        vec3 ambient = light.base.ambient * diffuseColor * light.base.color * light.base.density;
        vec3 lit = (kD * diffuseColor * light.base.diffuse * NdotL + specContrib) * light.base.color * light.base.density;
        result += ambient + lit;
    }

    // Point lights
    for (int i = 0; i < lights.pointLightCount; i++) {
        LightPoint light = lights.pointLights[i];
        vec3 pDir = normalize(light.base.position - inWorldPos);
        float dist = length(light.base.position - inWorldPos);
        float atten = 1.0 / (light.constant + light.linear * dist + light.exp * dist * dist);
        float NdotL = max(dot(normal, pDir), 0.0);

        vec3 specContrib = vec3(0.0);
        vec3 kD = vec3(1.0);
        if (NdotL > 0.0) {
            vec3 halfwayDir = normalize(pDir + viewDir);
            float NdotH = max(dot(normal, halfwayDir), 0.0);
            float HdotV = max(dot(halfwayDir, viewDir), 0.0);
            vec3 F = F0 + (1.0 - F0) * pow(1.0 - HdotV, 5.0);
            kD = (1.0 - F) * (1.0 - matMetallic);
            float specPower = pow(NdotH, shininess) * (shininess + 2.0) / 8.0;
            specContrib = F * specPower * light.base.specular * atten;
        }

        float pAmbient = light.base.ambient * atten;
        vec3 ambient = pAmbient * diffuseColor;
        vec3 lit = kD * diffuseColor * light.base.diffuse * NdotL * atten + specContrib;
        result += (ambient + lit) * light.base.color * light.base.density;
    }

    // Scene ambient
    #ifdef USE_IBL
    // Diffuse irradiance only - no specular IBL for transparent surfaces.
    // Specular environment reflections require per-room probes or SSR to
    // look correct indoors; the global skybox cubemap produces artifacts.
    vec3 irradiance = texture(irradianceMap, normal).rgb;
    vec3 sceneAmbient = diffuseColor * (1.0 - matMetallic) * irradiance * cam.ambient.w;
    #else
    vec3 sceneAmbient = diffuseColor * (1.0 - matMetallic) * cam.ambient.rgb * cam.ambient.w;
    #endif

    // Premultiplied alpha: composite blend is src*1 + dst*(1-srcAlpha),
    // so RGB must be pre-multiplied by alpha. Pixels with alpha=0 on the
    // cleared (0,0,0,0) background contribute nothing.
    vec3 finalColor = clamp(result + sceneAmbient, 0.0, 1.0);
    outFragColor = vec4(finalColor * baseColor.a, baseColor.a);
}
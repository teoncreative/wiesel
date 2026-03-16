#version 450

uint kVertexFlagHasTexture = 1 << 0;
uint kVertexFlagHasNormalMap = 1 << 1;
uint kVertexFlagHasSpecularMap = 1 << 2;
uint kVertexFlagHasHeightMap = 1 << 3;
uint kVertexFlagHasAlbedoMap = 1 << 4;
uint kVertexFlagHasRoughnessMap = 1 << 5;
uint kVertexFlagHasMetallicMap = 1 << 6;

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
    float entityId;
    vec4 colorTint;
    vec4 materialParams; // x=roughness, y=metallic, z=specular
};

layout(set = 0, binding = 1) uniform sampler2D baseTexture; // diffuse
layout(set = 0, binding = 2) uniform sampler2D normalMap;
layout(set = 0, binding = 3) uniform sampler2D specularMap;
layout(set = 0, binding = 4) uniform sampler2D heightMap;
layout(set = 0, binding = 5) uniform sampler2D albedoMap;
layout(set = 0, binding = 6) uniform sampler2D roughnessMap;
layout(set = 0, binding = 7) uniform sampler2D metallicMap;

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
layout(location = 12) in flat float inEntityId;

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
    } else {
        baseColor = vec4(1.0, 1.0, 1.0, 1.0);
    }
    baseColor *= colorTint;

    // Discard fully transparent pixels
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
    vec3 diffuseColor = albedo * (1.0 - matMetallic);
    vec3 specularColor = mix(vec3(matSpecular * 0.5), albedo, matMetallic);

    // Inline forward lighting (no shadows, no SSAO)
    vec3 result = vec3(0.0);

    // Directional lights
    for (int i = 0; i < lights.directLightCount; i++) {
        LightDirect light = lights.directLights[i];
        vec3 lDir = normalize(light.direction);

        float diff = light.base.diffuse * max(dot(normal, lDir), 0.0);
        float spec = 0.0;
        if (diff > 0.0) {
            vec3 halfwayDir = normalize(lDir + viewDir);
            spec = pow(max(dot(normal, halfwayDir), 0.0), shininess) * light.base.specular;
        }

        vec3 ambient = light.base.ambient * diffuseColor * light.base.color * light.base.density;
        vec3 diffSpec = (diff * diffuseColor + spec * specularColor) * light.base.color * light.base.density;
        result += ambient + diffSpec;
    }

    // Point lights
    for (int i = 0; i < lights.pointLightCount; i++) {
        LightPoint light = lights.pointLights[i];
        vec3 pDir = normalize(light.base.position - inWorldPos);
        float dist = length(light.base.position - inWorldPos);
        float atten = 1.0 / (light.constant + light.linear * dist + light.exp * dist * dist);

        float pDiff = light.base.diffuse * max(dot(normal, pDir), 0.0) * atten;
        float pSpec = 0.0;
        if (pDiff > 0.0) {
            vec3 halfwayDir = normalize(pDir + viewDir);
            pSpec = pow(max(dot(normal, halfwayDir), 0.0), shininess) * light.base.specular * atten;
        }

        float pAmbient = light.base.ambient * atten;
        result += (pAmbient * diffuseColor + pDiff * diffuseColor + pSpec * specularColor) * light.base.color * light.base.density;
    }

    outFragColor = vec4(clamp(result, 0.0, 1.0), baseColor.a);
}

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

layout (set = 0, binding = 0, std140) uniform Matrices {
    mat4 modelMatrix;
    mat3 normalMatrix;
    float entityId;
    // implicit 12 bytes padding to vec4 boundary
    vec4 colorTint;
    vec4 materialParams; // x=roughness, y=metallic, z=specular
};

layout (set = 1, binding = 1, std140) uniform Camera {
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
    vec3 ambientColor;
    float ambientIntensity;
} cam;

layout(set = 0, binding = 1) uniform sampler2D baseTexture; // diffuse
layout(set = 0, binding = 2) uniform sampler2D normalMap;
layout(set = 0, binding = 3) uniform sampler2D specularMap;
layout(set = 0, binding = 4) uniform sampler2D heightMap;
layout(set = 0, binding = 5) uniform sampler2D albedoMap;
layout(set = 0, binding = 6) uniform sampler2D roughnessMap;
layout(set = 0, binding = 7) uniform sampler2D metallicMap;

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

layout(location = 0) out vec4 outViewPos;
layout(location = 1) out vec4 outWorldPos;
layout(location = 2) out float outDepth;
layout(location = 3) out vec4 outNormal;
layout(location = 4) out vec4 outAlbedo;
layout(location = 5) out vec4 outMaterial;
layout(location = 6) out float outEntityId;


vec3 getSurfaceNormal() {
    vec3 normal;
    if ((inFlags & kVertexFlagHasNormalMap) > 0) {
        vec3 localNormal = 2.0 * texture(normalMap, inUV).rgb - 1.0;
        normal = normalize(inTBN * localNormal);
    } else {
        normal = normalize(inNormal);
    }
    return normal;
}

float linearDepth(float depth) {
    float z = depth * 2.0f - 1.0f;
    return (2.0f * cam.near * cam.far) / (cam.far + cam.near - z * (cam.far - cam.near));
}

void main() {
    vec4 baseColor;
    if ((inFlags & kVertexFlagHasTexture) > 0) {
        baseColor = texture(baseTexture, inUV);
    } else if ((inFlags & kVertexFlagHasAlbedoMap) > 0) {
        baseColor = texture(albedoMap, inUV);
    } else {
        baseColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    baseColor *= colorTint;

    if (baseColor.a < 0.5) {
        discard;
    }

    // materialParams: x=roughness, y=metallic, z=specular
    // When a map exists: texture * param (param acts as multiplier)
    // When no map exists: param used directly (acts as the value itself)
    float specular;
    if ((inFlags & kVertexFlagHasSpecularMap) > 0) {
        specular = texture(specularMap, inUV).r * materialParams.z;
    } else {
        specular = materialParams.z;
    }
    float roughness;
    if ((inFlags & kVertexFlagHasRoughnessMap) > 0) {
        roughness = texture(roughnessMap, inUV).r * materialParams.x;
    } else {
        roughness = materialParams.x;
    }
    float metallic;
    if ((inFlags & kVertexFlagHasMetallicMap) > 0) {
        metallic = texture(metallicMap, inUV).r * materialParams.y;
    } else {
        metallic = materialParams.y;
    }
    vec3 normal = getSurfaceNormal();

    // Debug: force vertex normal only (skip normal map)
    if (cam.debugCascades == 8) normal = normalize(inNormal);

    outViewPos = vec4(inViewPos, 1.0);
    outDepth = linearDepth(gl_FragCoord.z);
    outWorldPos = vec4(inWorldPos, 1.0);
    outNormal = vec4(normal * 0.5 + 0.5, 1.0);
    outAlbedo = vec4(inColor, 1.0f) * baseColor;
    outMaterial = vec4(specular, roughness, metallic, 0);
    outEntityId = inEntityId;
    /*switch(cascadeIndex) {
        case 0 :
            outColor.rgb *= vec3(1.0f, 0.25f, 0.25f);
            break;
        case 1 :
            outColor.rgb *= vec3(0.25f, 1.0f, 0.25f);
            break;
        case 2 :
            outColor.rgb *= vec3(0.25f, 0.25f, 1.0f);
            break;
        case 3 :
            outColor.rgb *= vec3(1.0f, 1.0f, 0.25f);
            break;
    }*/
}
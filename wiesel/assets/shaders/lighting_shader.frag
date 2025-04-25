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

float calculateShadow(vec4 shadowCoord, uint cascadeIndex, float ambient, vec3 normal, vec3 lightDir) {
    shadowCoord /= shadowCoord.w;

    if (shadowCoord.z < 0.0 || shadowCoord.z > 1.0 ||
    shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
    shadowCoord.y < 0.0 || shadowCoord.y > 1.0) {
        return 1.0;
    }

    float bias = max(0.005 * dot(normal, -lightDir), 0.0005);
    float shadow = 0.0;
    int count = 0;

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 pcfOffset = vec2(x, y) * texelSize;
            float pcfDepth = texture(shadowMap, vec3(shadowCoord.xy + pcfOffset, cascadeIndex)).r;
            shadow += (shadowCoord.z - bias > pcfDepth) ? 1.0 - ambient : 0.0;
            count++;
        }
    }

    shadow /= count;
    return 1.0 - shadow;
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
    vec3 viewDir = normalize(cam.position - viewPos);

    vec3 result = vec3(0.0f, 0.0f, 0.0f);

    float lightAmbient = 1.0f;
    vec3 lightDir = vec3(-worldPos);
    for (int i = 0; i < lights.directLightCount; i++) {
        LightDirect light = lights.directLights[i];
        lightAmbient = light.base.ambient;
        lightDir = light.direction;

        // Direction of the light (already normalized and transformed)
        vec3 lightDir = normalize(light.direction);

        float lightAmbient = light.base.ambient;

        // Calculate diffuse and specular components
        float lightDiffuse = light.base.diffuse * max(dot(normal, lightDir), 0.0);

        float lightSpecular = 0.0;
        if (lightDiffuse > 0.0) {
            // Calculate halfway vector for specular reflection
            //         vec3 viewDir = normalize(-inFragPosition);
            vec3 halfwayDir = normalize(lightDir + viewDir);
            float shininess = 32.0;
            lightSpecular = pow(max(dot(normal, halfwayDir), 0.0), shininess) * light.base.specular;
        }

        // Final color = diffuse color * light color + specular color * light color
        vec3 specularColor = vec3(1.0); // White specular color
        result += (lightAmbient * albedo.rgb * ambientOcclusion + lightDiffuse * albedo.rgb + lightSpecular * specularColor) * light.base.color * light.base.density;
        break;
    }
    for (int i = 0; i < lights.pointLightCount; i++) {
        LightPoint light = lights.pointLights[i];
        // Calculate light direction and distance
        vec3 lightDir = normalize(light.base.position - worldPos);
        float distance = length(light.base.position - worldPos);
        float attenuation = 1.0 / (light.constant + light.linear * distance + light.exp * distance * distance);

        float lightAmbient = light.base.ambient;
        lightAmbient *= attenuation;

        // Calculate diffuse and specular components
        float lightDiffuse = light.base.diffuse * max(dot(normal, lightDir), 0.0);
        lightDiffuse *= attenuation;

        float lightSpecular = 0.0;
        if (lightDiffuse > 0.0) {
            // Calculate halfway vector for specular reflection
            //         vec3 viewDir = normalize(-inFragPosition);
            vec3 halfwayDir = normalize(lightDir + viewDir);
            float shininess = 32.0;
            lightSpecular = pow(max(dot(normal, halfwayDir), 0.0), shininess) * light.base.specular;

            // Attenuate light intensity based on distance
            lightSpecular *= attenuation;
        }

        // Final color = diffuse color * light color + specular color * light color
        vec3 specularColor = vec3(1.0); // White specular color
        result += (lightAmbient * albedo.rgb * ambientOcclusion + lightDiffuse * albedo.rgb + lightSpecular * specularColor) * light.base.color * light.base.density;
    }
    float shadow = 1.0f;
    uint cascadeIndex = 0;
    if (shadowMatrices.enableShadows != 0 && lightAmbient > 0) {
        // Get cascade index for the current fragment's view position
        for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; ++i) {
            if (viewPos.z < cam.cascadeSplits[i]) {
                cascadeIndex = i + 1;
            }
        }
        vec4 shadowCoord = biasMat * shadowMatrices.viewProjectionMatrix[cascadeIndex] * vec4(worldPos, 1.0);
        shadow = calculateShadow(shadowCoord, cascadeIndex, lightAmbient, normal, lightDir);
    }
    outFragColor = vec4(clamp(result * shadow, 0.0, 1.0), albedo.a);
}

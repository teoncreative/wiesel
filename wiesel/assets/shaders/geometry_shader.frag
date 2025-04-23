#version 450
#define SHADOW_MAP_CASCADE_COUNT 4

uint VertexFlagHasTexture = 1 << 0;
uint VertexFlagHasNormalMap = 1 << 1;
uint VertexFlagHasSpecularMap = 1 << 2;
uint VertexFlagHasHeightMap = 1 << 3;
uint VertexFlagHasAlbedoMap = 1 << 4;
uint VertexFlagHasRoughnessMap = 1 << 5;
uint VertexFlagHasMetallicMap = 1 << 6;

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
    vec3 scale;
    mat3 normalMatrix;
    mat4 rotationMatrix;
};

const int MAX_LIGHTS = 16;
layout (set = 1, binding = 0) uniform LightsBufferObject {
    int directLightCount;
    int pointLightCount;
    LightDirect directLights[MAX_LIGHTS];
    LightPoint pointLights[MAX_LIGHTS];
} lights;

layout (set = 1, binding = 1, std140) uniform Camera {
    mat4 viewMatrix;
    mat4 projection;
    mat4 invProjection;
    vec3 position;
    float _pad0;
    float near;
    float far;
    vec4 cascadeSplits;
} cam;

layout(set = 1, binding = 2) uniform CVPM {
    mat4 lightViewProj[SHADOW_MAP_CASCADE_COUNT];
    int enableShadows;
} cvpm;

layout(set = 1, binding = 3) uniform sampler2DArray shadowMap;

layout(set = 0, binding = 1) uniform sampler2D baseTexture; // diffuse
layout(set = 0, binding = 2) uniform sampler2D normalMap;
layout(set = 0, binding = 3) uniform sampler2D specularMap;
layout(set = 0, binding = 4) uniform sampler2D heightMap;
layout(set = 0, binding = 5) uniform sampler2D albedoMap;
layout(set = 0, binding = 6) uniform sampler2D roughnessMap;
layout(set = 0, binding = 7) uniform sampler2D metalicMap;

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

layout(location = 0) out vec4 outColor;

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
    shadowCoord.y < 0.0 || shadowCoord.y > 1.0)
    return 1.0;

    float bias = max(0.002 * dot(normal, -lightDir), 0.0005);
    float shadow = 0.0;
    int count = 0;

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 pcfOffset = vec2(x, y) * texelSize;
            float pcfDepth = texture(shadowMap, vec3(shadowCoord.xy + pcfOffset, cascadeIndex)).r;
            shadow += (shadowCoord.z - bias > pcfDepth) ? ambient : 0.0;
            count++;
        }
    }

    shadow /= count;
    return 1.0 - shadow;
}

/*float filterPCF(vec4 sc, uint cascadeIndex, float ambient)  {
    ivec2 texDim = textureSize(shadowMap, 0).xy;
    float scale = 0.75;
    float dx = scale * 1.0 / float(texDim.x);
    float dy = scale * 1.0 / float(texDim.y);

    float shadowFactor = 0.0;
    int count = 0;
    int range = 1;

    for (int x = -range; x <= range; x++) {
        for (int y = -range; y <= range; y++) {
            shadowFactor += textureProj(sc, vec2(dx*x, dy*y), cascadeIndex, ambient);
            count++;
        }
    }
    return shadowFactor / count;
}*/

vec3 getSurfaceNormal() {
    vec3 normal;
    if ((inFlags & VertexFlagHasNormalMap) > 0) {
        vec3 localNormal = 2.0 * texture(normalMap, inUV).rgb - 1.0;
        normal = normalize(inTBN * localNormal);
    } else {
        normal = inNormal;
    }
    //   vec3 normal = inVertexNormal;
    return normal;
}

void main() {
    vec4 baseColor;
    if ((inFlags & VertexFlagHasTexture) > 0) {
        baseColor = texture(baseTexture, inUV);
    } else {
        baseColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    if (baseColor.a < 0.5) {
        discard;
    }

    vec4 specular;
    if ((inFlags & VertexFlagHasSpecularMap) > 0) {
        specular = texture(specularMap, inUV);
    } else {
        specular = vec4(0.0f);
    }
    vec3 normal = getSurfaceNormal();

    vec3 result = vec3(0.0f, 0.0f, 0.0f);
    float ambient = 0.0f;
    vec3 lightDir;
    for (int i = 0; i < lights.directLightCount; i++) {
        LightDirect light = lights.directLights[i];
        if (i == 0) {
            ambient = light.base.ambient;
            lightDir = light.direction;
        }

        // Direction of the light (already normalized and transformed)
        vec3 lightDir = normalize(light.direction);

        float lightAmbient = light.base.ambient;

        // Calculate diffuse and specular components
        float lightDiffuse = max(dot(normal, lightDir), 0.0);

        float lightSpecular = 0.0;
        if (lightDiffuse > 0.0) {
            // Calculate halfway vector for specular reflection
            //         vec3 viewDir = normalize(-inFragPosition);
            vec3 halfwayDir = normalize(lightDir + inViewDir);
            float shininess = 32.0;
            lightSpecular = pow(max(dot(normal, halfwayDir), 0.0), shininess) * light.base.specular;
        }

        // Final color = diffuse color * light color + specular color * light color
        vec3 specularColor = vec3(1.0); // White specular color
        result += (lightAmbient * baseColor.rgb + lightDiffuse * baseColor.rgb + lightSpecular * specularColor) * light.base.color * light.base.density;
    }

    for (int i = 0; i < lights.pointLightCount; i++) {
        LightPoint light = lights.pointLights[i];
        // Calculate light direction and distance
        vec3 lightDir = normalize(light.base.position - inWorldPos);
        float distance = length(light.base.position - inWorldPos);
        float attenuation = 1.0 / (light.constant + light.linear * distance + light.exp * distance * distance);

        float lightAmbient = light.base.ambient;
        lightAmbient *= attenuation;

        // Calculate diffuse and specular components
        float lightDiffuse = max(dot(normal, lightDir), 0.0);
        lightDiffuse *= attenuation;

        float lightSpecular = 0.0;
        if (lightDiffuse > 0.0) {
            // Calculate halfway vector for specular reflection
            //         vec3 viewDir = normalize(-inFragPosition);
            vec3 halfwayDir = normalize(lightDir + inViewDir);
            float shininess = 32.0;
            lightSpecular = pow(max(dot(normal, halfwayDir), 0.0), shininess) * light.base.specular;

            // Attenuate light intensity based on distance
            lightSpecular *= attenuation;
        }

        // Final color = diffuse color * light color + specular color * light color
        vec3 specularColor = vec3(1.0); // White specular color
        result += (lightAmbient * baseColor.rgb + lightDiffuse * baseColor.rgb + lightSpecular * specularColor) * light.base.color * light.base.density;
    }

    outColor = vec4(inColor, 1.0f) * vec4(clamp(result, 0.0, 1.0), baseColor.a);
    if (cvpm.enableShadows != 0) {
        // Get cascade index for the current fragment's view position
        uint cascadeIndex = 0;
        for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; ++i) {
            if (inViewPos.z < cam.cascadeSplits[i]) {
                cascadeIndex = i + 1;
            }
        }
        vec4 shadowCoord = biasMat * cvpm.lightViewProj[cascadeIndex] * vec4(inWorldPos, 1.0);
        float shadow = calculateShadow(shadowCoord, cascadeIndex, ambient, normal, lightDir);
        outColor.rgb *= shadow;
    }
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
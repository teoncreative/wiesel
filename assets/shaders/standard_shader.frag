#version 450

uint VertexFlagHasTexture = 1 << 0;
uint VertexFlagHasNormalMap = 1 << 1;
uint VertexFlagHasSpecularMap = 1 << 2;
uint VertexFlagHasHeightMap = 1 << 3;
uint VertexFlagHasAlbedoMap = 1 << 4;
uint VertexFlagHasRoughnessMap = 1 << 5;
uint VertexFlagHasMetallicMap = 1 << 6;

struct LightBase {
   vec3 color;
   float ambient;
   float diffuse;
   float specular;
   float density;
};

struct LightDirect {
   vec3 direction;
   LightBase base;
};

struct LightPoint {
   vec3 position;
   LightBase base;

   float constant;
   float linear;
   float exp;
};

layout(binding = 0) uniform UniformBufferObject {
   mat4 modelMatrix;
   vec3 scale;
   mat3 normalMatrix;
   mat4 cameraViewMatrix;
   mat4 cameraProjection;
   vec3 cameraPosition;
} ubo;

const int MAX_LIGHTS = 16;
layout(binding = 1) uniform LightsBufferObject {
   int directLightCount;
   int pointLightCount;
   LightDirect directLights[MAX_LIGHTS];
   LightPoint pointLights[MAX_LIGHTS];
} lights;

layout(binding = 2) uniform sampler2D baseTexture; // diffuse
layout(binding = 3) uniform sampler2D normalMap;
layout(binding = 4) uniform sampler2D specularMap;
layout(binding = 5) uniform sampler2D heightMap;
layout(binding = 6) uniform sampler2D albedoMap;
layout(binding = 7) uniform sampler2D roughnessMap;
layout(binding = 8) uniform sampler2D metalicMap;

layout(location = 0) in vec3 inFragPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inVertexNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBiTangent;
layout(location = 6) in flat uint inFlags;
layout(location = 7) in vec3 inViewDir;

layout(location = 0) out vec4 outColor;

float saturate(float x) {
   return max(0.0f, min(1.0f, x));
}

vec3 getSurfaceNormal() {
   // todo use normals from normal map
/*   vec3 normal;
   if ((inFlags & VertexFlagHasNormalMap) > 0) {
      normal = normalize(texture(normalMap, inUV).rgb * 2.0 - 1.0);
   } else {
      normal = normalize(inNormal);
   }*/
   vec3 normal = inVertexNormal;
   return normal;
}
void main() {
   vec4 baseColor;
   if ((inFlags & VertexFlagHasTexture) > 0) {
      baseColor = texture(baseTexture, inUV);
   } else {
      baseColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
   }

   vec4 specular;
   if ((inFlags & VertexFlagHasSpecularMap) > 0) {
      specular = texture(specularMap, inUV);
   } else {
      specular = vec4(0.0f);
   }
   vec3 normal = getSurfaceNormal();

   vec3 result = vec3(0.0f, 0.0f, 0.0f);
   for (int i = 0; i < lights.directLightCount; i++) {
      LightDirect light = lights.directLights[i];
      float lightAmbient = light.base.ambient;

      float lightDiffuse = light.base.diffuse * max(dot(normal, light.direction), 0.0);
      float lightSpecular = 0.0;
     // if (lightDiffuse > 0) {
      // Calculate halfway vector for specular reflection
      //         vec3 viewDir = normalize(-inFragPosition);
      vec3 halfwayDir = normalize(light.direction + inViewDir);
      lightSpecular = pow(max(dot(normal, halfwayDir), 0.0), 100.0) * light.base.specular; // 32 = Shininess factor (100)
      //}

      vec3 specularColor = vec3(1.0f);
      result = max(result, (lightAmbient * baseColor.rgb + lightDiffuse * baseColor.rgb + lightSpecular * specularColor) * light.base.color);
   }

   for (int i = 0; i < lights.pointLightCount; i++) {
      LightPoint light = lights.pointLights[i];
      // Calculate light direction and distance
      vec3 lightDir = normalize(light.position - inFragPosition);
      float distance = length(light.position - inFragPosition);
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
         lightSpecular = pow(max(dot(normal, halfwayDir), 0.0), 32.0) * light.base.specular; // 32 = Shininess factor

         // Attenuate light intensity based on distance
         lightSpecular *= attenuation;
      }

      // Final color = diffuse color * light color + specular color * light color
      vec3 specularColor = vec3(0.0); // White specular color
      result = max(result, (lightAmbient * baseColor.rgb + lightDiffuse * baseColor.rgb + lightSpecular * specularColor) * light.base.color);
   }

   // Use the transformed normal as the final surface normal
   //outColor = vec4(normal, 1.0);
   outColor = vec4(inColor, 1.0f) * vec4(result, 1.0f);
}
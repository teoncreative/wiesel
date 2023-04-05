#version 450

layout(binding = 1) uniform sampler2D textureSampler;

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inUV;
layout(location = 2) flat in uint inHasTexture;
layout(location = 3) in vec3 inEyePos;
layout(location = 4) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

void main() {
   if (inHasTexture > 0) {
      outColor = texture(textureSampler, inUV) * vec4(inColor, 1.0f);
   } else {
      outColor = vec4(inColor, 1.0f);
   }
}
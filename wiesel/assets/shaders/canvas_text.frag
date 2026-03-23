#version 450

layout(set = 0, binding = 1) uniform sampler2D fontAtlas;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inEntityId;

layout(location = 0) out vec4 outFragColor;
layout(location = 1) out float outEntityId;

void main() {
    float a = inColor.a * texture(fontAtlas, inUV).r;
    outFragColor = vec4(inColor.rgb * a, a);
    outEntityId = a > 0.01 ? inEntityId : 0.0;
}

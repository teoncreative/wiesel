#version 450

layout (set = 0, binding = 0) uniform sampler2D fontAtlas;

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in flat uint inEntityId;

layout (location = 0) out vec4 outFragColor;
layout (location = 1) out uint outEntityId;

void main() {
    // Font atlas is grayscale (R8): use R as alpha, tint provides RGB.
    float alpha = texture(fontAtlas, inUV).r * inColor.a;
    if (alpha > 0.0) {
        outEntityId = inEntityId;
    } else {
        outEntityId = 0;
    }
    outFragColor = vec4(inColor.rgb * alpha, alpha);
}

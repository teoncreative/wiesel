#version 450

layout (set = 0, binding = 0) uniform sampler2D iconTexture;

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in flat uint inEntityId;

layout (location = 0) out vec4 outFragColor;
layout (location = 1) out uint outEntityId;

void main() {
    vec4 texel = texture(iconTexture, inUV);
    vec4 color = texel * inColor;

    if (color.a > 0.0) {
        outEntityId = inEntityId;
    } else {
        outEntityId = 0;
    }

    outFragColor = vec4(color.rgb * color.a, color.a);
}

#version 450

layout(set = 0, binding = 1) uniform sampler2D elementTexture;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inEntityId;
layout (location = 3) in float inPremultiplied;

layout(location = 0) out vec4 outFragColor;
layout(location = 1) out float outEntityId;

void main() {
    vec4 c = texture(elementTexture, inUV) * inColor;
    // Skip premultiply if source is already premultiplied (e.g. RmlUi offscreen)
    if (inPremultiplied > 0.5) {
        outFragColor = c;
    } else {
        outFragColor = vec4(c.rgb * c.a, c.a);
    }
    outEntityId = inEntityId;
}

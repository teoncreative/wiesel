#version 450

layout (push_constant) uniform PushData {
    mat4 mvp;
    vec4 color;
    vec4 uv_rect;  // (u_min, v_min, u_max, v_max) - atlas sub-region
    uint entityId;
};

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outUV;
layout (location = 2) out flat uint outEntityId;

void main() {
    gl_Position = mvp * vec4(inPosition, 1.0);
    outColor = color;
    // Quad UV (0..1) remapped into the glyph's atlas region
    outUV = mix(uv_rect.xy, uv_rect.zw, inUV);
    outEntityId = entityId;
}

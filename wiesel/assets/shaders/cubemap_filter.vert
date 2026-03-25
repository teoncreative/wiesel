#version 450

layout (push_constant) uniform PushConstants {
    mat4 viewProjection;
} pc;

layout (location = 0) out vec3 localPos;

// Fullscreen triangle that covers the clip space
void main() {
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec4 clipPos = vec4(uv * 2.0 - 1.0, 0.0, 1.0);

    // Unproject to world direction
    vec4 worldPos = inverse(pc.viewProjection) * clipPos;
    localPos = worldPos.xyz / worldPos.w;

    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
#version 450

layout(set = 1, binding = 1, std140) uniform Camera {
    mat4 viewMatrix;
    mat4 projection;
    mat4 invProjection;
    vec3 position;
    float _pad0;
    float near;
    float far;
} cam;

layout(location = 0) out vec3 vDirection;

vec3 cubeVerts[36] = vec3[](
    // back face
    vec3(-1, -1, -1),
    vec3( 1,  1, -1),
    vec3( 1, -1, -1),
    vec3(-1, -1, -1),
    vec3(-1,  1, -1),
    vec3( 1,  1, -1),

    // front face
    vec3(-1, -1,  1),
    vec3( 1, -1,  1),
    vec3( 1,  1,  1),
    vec3(-1, -1,  1),
    vec3( 1,  1,  1),
    vec3(-1,  1,  1),

    // left face
    vec3(-1, -1, -1),
    vec3(-1, -1,  1),
    vec3(-1,  1,  1),
    vec3(-1, -1, -1),
    vec3(-1,  1,  1),
    vec3(-1,  1, -1),

    // right face
    vec3( 1, -1, -1),
    vec3( 1,  1,  1),
    vec3( 1, -1,  1),
    vec3( 1, -1, -1),
    vec3( 1,  1, -1),
    vec3( 1,  1,  1),

    // bottom face
    vec3(-1, -1, -1),
    vec3( 1, -1, -1),
    vec3( 1, -1,  1),
    vec3(-1, -1, -1),
    vec3( 1, -1,  1),
    vec3(-1, -1,  1),

    // top face
    vec3(-1,  1, -1),
    vec3(-1,  1,  1),
    vec3( 1,  1,  1),
    vec3(-1,  1, -1),
    vec3( 1,  1,  1),
    vec3( 1,  1, -1)
);

void main() {
    vec3 pos = cubeVerts[gl_VertexIndex];
    vDirection = normalize(pos);
    gl_Position = cam.projection * mat4(mat3(cam.viewMatrix)) * vec4(pos, 1.0);
    gl_Position.z = gl_Position.w; // push to depth = 1.0
}

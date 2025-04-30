#version 450

layout(location = 0) in vec2 inUV;

layout (set = 0, binding = 1, std140) uniform Matrices {
    mat4 modelMatrix;
};

layout(set = 1, binding = 1, std140) uniform Camera {
    mat4 viewMatrix;
    mat4 projection;
    mat4 invProjection;
    vec3 position;
    float _pad0;
    float near;
    float far;
    vec4 cascadeSplits;
} cam;

layout(location = 0) out vec2 outUV;

const vec2 quadPos[6] = vec2[6](
vec2(-.5,-.5),  // 0: bottom-left
vec2( .5,-.5),  // 1: bottom-right
vec2( .5, .5),  // 2: top-right

vec2(-.5,-.5),  // 3: bottom-left
vec2( .5, .5),  // 4: top-right
vec2(-.5, .5)   // 5: top-left
);

void main() {
    vec2 localPos = quadPos[gl_VertexIndex];
    outUV = -inUV;
    gl_Position = cam.projection * cam.viewMatrix
    * modelMatrix * vec4(localPos,0,1);
}

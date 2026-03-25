#version 450

layout (location = 0) in vec3 localPos;
layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform samplerCube environmentMap;

const float PI = 3.14159265359;

void main() {
    vec3 normal = normalize(localPos);

    vec3 irradiance = vec3(0.0);

    // Tangent-space basis from normal
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float sampleDelta = 0.025;
    float nrSamples = 0.0;

    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            // Spherical to cartesian in tangent space
            vec3 tangentSample = vec3(
            sin(theta) * cos(phi),
            sin(theta) * sin(phi),
            cos(theta)
            );
            // Tangent space to world
            vec3 sampleVec = tangentSample.x * right +
            tangentSample.y * up +
            tangentSample.z * normal;

            irradiance += texture(environmentMap, sampleVec).rgb *
            cos(theta) * sin(theta);
            nrSamples++;
        }
    }

    irradiance = PI * irradiance * (1.0 / nrSamples);
    outColor = vec4(irradiance, 1.0);
}
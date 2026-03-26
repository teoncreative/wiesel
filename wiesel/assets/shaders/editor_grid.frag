#version 450

layout(set = 0, binding = 0, std140) uniform GridUBO {
    mat4 invViewProjection;
    mat4 viewProjection;
    vec4 cameraPos;    // w unused
    float gridScale;
    float fadeDistance;
};

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

float gridLine(vec2 coord, float scale) {
    vec2 grid = abs(fract(coord / scale - 0.5) - 0.5);
    vec2 line = fwidth(coord / scale);
    vec2 g = smoothstep(line * 0.3, line * 0.8, grid);
    return 1.0 - min(g.x, g.y);
}

void main() {
    vec2 ndc = inUV * 2.0 - 1.0;

    // Unproject near/far to world space (GLM_FORCE_DEPTH_ZERO_TO_ONE: Z range [0, 1])
    vec4 nearPoint = invViewProjection * vec4(ndc, 0.0, 1.0);
    vec4 farPoint  = invViewProjection * vec4(ndc, 1.0, 1.0);
    nearPoint.xyz /= nearPoint.w;
    farPoint.xyz  /= farPoint.w;

    vec3 rayOrigin = nearPoint.xyz;
    vec3 rayDir = normalize(farPoint.xyz - nearPoint.xyz);

    // Intersect with Y=0 plane
    if (abs(rayDir.y) < 0.0001) {
        discard;
    }

    float t = -rayOrigin.y / rayDir.y;
    if (t < 0.0) {
        discard;
    }

    vec3 worldPos = rayOrigin + rayDir * t;

    // Compute depth for this world position (GLM_FORCE_DEPTH_ZERO_TO_ONE: already [0,1])
    vec4 clipPos = viewProjection * vec4(worldPos, 1.0);
    gl_FragDepth = clipPos.z / clipPos.w;

    // Distance fade (XZ distance from camera)
    float dist = length(worldPos.xz - cameraPos.xz);
    float fade = 1.0 - smoothstep(fadeDistance * 0.3, fadeDistance, dist);
    if (fade < 0.001) {
        discard;
    }

    float camHeight = max(abs(cameraPos.y), 0.1);

    float fade0 = 1.0 - smoothstep(gridScale * 15.0, gridScale * 50.0, camHeight);
    float fade1 = 1.0 - smoothstep(gridScale * 150.0, gridScale * 500.0, camHeight);
    float fade2 = 1.0 - smoothstep(gridScale * 1500.0, gridScale * 5000.0, camHeight);

    float l0 = gridLine(worldPos.xz, gridScale) * 0.25 * fade0;
    float l1 = gridLine(worldPos.xz, gridScale * 10.0) * 0.35 * fade1;
    float l2 = gridLine(worldPos.xz, gridScale * 100.0) * 0.45 * fade2;
    float l3 = gridLine(worldPos.xz, gridScale * 1000.0) * 0.55;
    float intensity = max(max(l0, l1), max(l2, l3));

    // Axis highlights - wider than grid lines (3x), clamped to prevent grazing expansion
    vec2 fw = min(fwidth(worldPos.xz), vec2(gridScale * 0.05));
    float xAxisDist = abs(worldPos.z);
    float zAxisDist = abs(worldPos.x);
    float xAxis = 1.0 - smoothstep(fw.x * 0.5, fw.x * 3.5, xAxisDist);
    float zAxis = 1.0 - smoothstep(fw.y * 0.5, fw.y * 3.5, zAxisDist);

    // Axes fully replace grid - no blending
    float axisMask = max(xAxis, zAxis);

    if (axisMask > 0.01) {
        vec3 axisColor = vec3(0.0);
        if (xAxis > zAxis) {
            axisColor = vec3(0.85, 0.15, 0.15);
        } else {
            axisColor = vec3(0.15, 0.15, 0.85);
        }
        float a = axisMask * 0.9 * fade;
        // Premultiplied alpha (blend mode uses ONE, ONE_MINUS_SRC_ALPHA)
        outFragColor = vec4(axisColor * a, a);
        return;
    }

    if (intensity < 0.001) {
        discard;
    }

    // Premultiplied alpha
    float a = intensity * fade;
    outFragColor = vec4(vec3(0.55) * a, a);
}
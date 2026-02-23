#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneColor;
layout(set = 0, binding = 1) uniform sampler2D normalBuffer;
layout(set = 0, binding = 2) uniform sampler2D depthBuffer;

layout(push_constant) uniform ToonParams {
    int bands;
    float edgeThreshold;
    float edgeStrength;
};

// Sobel edge detection on a single-channel value across a 3x3 kernel
float sobelEdge(sampler2D tex, vec2 uv, vec2 texelSize, int channel) {
    float tl = texelFetch(tex, ivec2(gl_FragCoord.xy) + ivec2(-1, -1), 0)[channel];
    float t  = texelFetch(tex, ivec2(gl_FragCoord.xy) + ivec2( 0, -1), 0)[channel];
    float tr = texelFetch(tex, ivec2(gl_FragCoord.xy) + ivec2( 1, -1), 0)[channel];
    float l  = texelFetch(tex, ivec2(gl_FragCoord.xy) + ivec2(-1,  0), 0)[channel];
    float r  = texelFetch(tex, ivec2(gl_FragCoord.xy) + ivec2( 1,  0), 0)[channel];
    float bl = texelFetch(tex, ivec2(gl_FragCoord.xy) + ivec2(-1,  1), 0)[channel];
    float b  = texelFetch(tex, ivec2(gl_FragCoord.xy) + ivec2( 0,  1), 0)[channel];
    float br = texelFetch(tex, ivec2(gl_FragCoord.xy) + ivec2( 1,  1), 0)[channel];

    float gx = -tl - 2.0 * l - bl + tr + 2.0 * r + br;
    float gy = -tl - 2.0 * t - tr + bl + 2.0 * b + br;
    return sqrt(gx * gx + gy * gy);
}

void main() {
    vec3 color = texture(sceneColor, inUV).rgb;
    vec2 texelSize = 1.0 / textureSize(sceneColor, 0);

    // Color posterization
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float quantized = floor(lum * float(bands) + 0.5) / float(bands);
    // Preserve hue by scaling original color
    vec3 toonColor = color * (quantized / max(lum, 0.001));
    // Clamp to avoid fireflies from bright specular
    toonColor = clamp(toonColor, 0.0, 1.0);

    // Edge detection
    // Depth edges - normalize by center depth so distant surfaces don't
    // produce artificially large edge magnitudes (which cause fog-like darkening)
    float centerDepth = texelFetch(depthBuffer, ivec2(gl_FragCoord.xy), 0).r;
    float depthEdge = sobelEdge(depthBuffer, inUV, texelSize, 0) / max(centerDepth, 0.001);

    // Normal edges (check all 3 channels, take max)
    float normalEdgeR = sobelEdge(normalBuffer, inUV, texelSize, 0);
    float normalEdgeG = sobelEdge(normalBuffer, inUV, texelSize, 1);
    float normalEdgeB = sobelEdge(normalBuffer, inUV, texelSize, 2);
    float normalEdge = max(normalEdgeR, max(normalEdgeG, normalEdgeB));

    // Combine edges
    float edge = max(depthEdge, normalEdge);
    float edgeMask = smoothstep(edgeThreshold * 0.5, edgeThreshold, edge);

    // Apply dark outlines
    vec3 edgeColor = vec3(0.0);
    vec3 finalColor = mix(toonColor, edgeColor, edgeStrength * edgeMask);

    outColor = vec4(finalColor, 1.0);
}

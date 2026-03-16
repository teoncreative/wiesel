#version 450

layout(set = 0, binding = 0) uniform sampler2D sceneImage;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform FxaaParams {
    vec2 inverseScreenSize;
};

// FXAA 3.11 Quality
// Based on NVIDIA FXAA 3.11 by Timothy Lottes

const float FXAA_EDGE_THRESHOLD = 0.0625;      // 1/16
const float FXAA_EDGE_THRESHOLD_MIN = 0.03125;  // 1/32
const float FXAA_SUBPIX_QUALITY = 0.75;
const int FXAA_SEARCH_STEPS = 12;

float luminance(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 uv = inUV;
    vec2 texel = inverseScreenSize;

    // Sample center and 4 neighbors
    vec3 rgbM  = texture(sceneImage, uv).rgb;
    vec3 rgbN  = texture(sceneImage, uv + vec2( 0.0, -texel.y)).rgb;
    vec3 rgbS  = texture(sceneImage, uv + vec2( 0.0,  texel.y)).rgb;
    vec3 rgbW  = texture(sceneImage, uv + vec2(-texel.x,  0.0)).rgb;
    vec3 rgbE  = texture(sceneImage, uv + vec2( texel.x,  0.0)).rgb;

    float lumM = luminance(rgbM);
    float lumN = luminance(rgbN);
    float lumS = luminance(rgbS);
    float lumW = luminance(rgbW);
    float lumE = luminance(rgbE);

    float lumMin = min(lumM, min(min(lumN, lumS), min(lumW, lumE)));
    float lumMax = max(lumM, max(max(lumN, lumS), max(lumW, lumE)));
    float lumRange = lumMax - lumMin;

    // Early exit if contrast is too low
    if (lumRange < max(FXAA_EDGE_THRESHOLD_MIN, lumMax * FXAA_EDGE_THRESHOLD)) {
        fragColor = vec4(rgbM, 1.0);
        return;
    }

    // Sample 4 diagonal neighbors
    vec3 rgbNW = texture(sceneImage, uv + vec2(-texel.x, -texel.y)).rgb;
    vec3 rgbNE = texture(sceneImage, uv + vec2( texel.x, -texel.y)).rgb;
    vec3 rgbSW = texture(sceneImage, uv + vec2(-texel.x,  texel.y)).rgb;
    vec3 rgbSE = texture(sceneImage, uv + vec2( texel.x,  texel.y)).rgb;

    float lumNW = luminance(rgbNW);
    float lumNE = luminance(rgbNE);
    float lumSW = luminance(rgbSW);
    float lumSE = luminance(rgbSE);

    // Subpixel aliasing detection
    float lumNS = lumN + lumS;
    float lumWE = lumW + lumE;
    float lumCorners = lumNW + lumNE + lumSW + lumSE;
    float subpixA = (2.0 * lumNS + 2.0 * lumWE + lumCorners) / 12.0;
    float subpixB = abs(subpixA - lumM);
    float subpixC = clamp(subpixB / lumRange, 0.0, 1.0);
    float subpixD = (-2.0 * subpixC + 3.0) * subpixC * subpixC;
    float subpixFinal = subpixD * subpixD * FXAA_SUBPIX_QUALITY;

    // Edge detection - horizontal vs vertical
    float edgeH = abs(-2.0 * lumN + lumNW + lumNE) +
                  abs(-2.0 * lumM + lumW  + lumE ) * 2.0 +
                  abs(-2.0 * lumS + lumSW + lumSE);
    float edgeV = abs(-2.0 * lumW + lumNW + lumSW) +
                  abs(-2.0 * lumM + lumN  + lumS ) * 2.0 +
                  abs(-2.0 * lumE + lumNE + lumSE);
    bool isHorizontal = edgeH >= edgeV;

    // Select edge endpoints
    float lum1 = isHorizontal ? lumN : lumW;
    float lum2 = isHorizontal ? lumS : lumE;
    float grad1 = abs(lum1 - lumM);
    float grad2 = abs(lum2 - lumM);
    bool is1Steepest = grad1 >= grad2;

    float gradScaled = 0.25 * max(grad1, grad2);

    float stepLength = isHorizontal ? texel.y : texel.x;
    float lumLocalAvg;

    if (is1Steepest) {
        stepLength = -stepLength;
        lumLocalAvg = 0.5 * (lum1 + lumM);
    } else {
        lumLocalAvg = 0.5 * (lum2 + lumM);
    }

    // Shift UV to edge
    vec2 currentUV = uv;
    if (isHorizontal) {
        currentUV.y += stepLength * 0.5;
    } else {
        currentUV.x += stepLength * 0.5;
    }

    // Search along edge in both directions
    vec2 offset = isHorizontal ? vec2(texel.x, 0.0) : vec2(0.0, texel.y);

    vec2 uv1 = currentUV - offset;
    vec2 uv2 = currentUV + offset;

    float lumEnd1 = luminance(texture(sceneImage, uv1).rgb) - lumLocalAvg;
    float lumEnd2 = luminance(texture(sceneImage, uv2).rgb) - lumLocalAvg;

    bool reached1 = abs(lumEnd1) >= gradScaled;
    bool reached2 = abs(lumEnd2) >= gradScaled;
    bool reachedBoth = reached1 && reached2;

    if (!reached1) uv1 -= offset;
    if (!reached2) uv2 += offset;

    if (!reachedBoth) {
        for (int i = 2; i < FXAA_SEARCH_STEPS; i++) {
            if (!reached1) {
                lumEnd1 = luminance(texture(sceneImage, uv1).rgb) - lumLocalAvg;
            }
            if (!reached2) {
                lumEnd2 = luminance(texture(sceneImage, uv2).rgb) - lumLocalAvg;
            }

            reached1 = abs(lumEnd1) >= gradScaled;
            reached2 = abs(lumEnd2) >= gradScaled;
            reachedBoth = reached1 && reached2;

            if (!reached1) uv1 -= offset;
            if (!reached2) uv2 += offset;

            if (reachedBoth) break;
        }
    }

    // Compute edge blend factor
    float dist1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
    float dist2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);

    bool isDir1 = dist1 < dist2;
    float distFinal = min(dist1, dist2);
    float edgeLength = dist1 + dist2;
    float pixelOffset = -distFinal / edgeLength + 0.5;

    bool isLumCenterSmaller = lumM < lumLocalAvg;
    bool correctVariation = ((isDir1 ? lumEnd1 : lumEnd2) < 0.0) != isLumCenterSmaller;

    float finalOffset = correctVariation ? pixelOffset : 0.0;
    finalOffset = max(finalOffset, subpixFinal);

    // Apply offset
    vec2 finalUV = uv;
    if (isHorizontal) {
        finalUV.y += finalOffset * stepLength;
    } else {
        finalUV.x += finalOffset * stepLength;
    }

    fragColor = vec4(texture(sceneImage, finalUV).rgb, 1.0);
}

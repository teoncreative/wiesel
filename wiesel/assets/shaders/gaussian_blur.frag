#version 450

layout (location = 0) in vec2 inUV;

layout (set = 0, binding = 0) uniform sampler2D inputImage;

#ifdef USE_PUSH_RADIUS
layout (push_constant) uniform BlurParams {
    float radius;
};
#endif

#ifdef BLUR_SINGLE_CHANNEL
layout (location = 0) out float outResult;
#else
layout (location = 0) out vec4 outResult;
#endif

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(inputImage, 0));

    #ifdef BLUR_VERTICAL
    vec2 dir = vec2(0.0, texelSize.y);
    #else
    vec2 dir = vec2(texelSize.x, 0.0);
    #endif

    #ifdef USE_PUSH_RADIUS
    int r = int(radius);
    float sigma = radius * 0.33;
    #else
    const int r = 6;
    const float sigma = 4.0;
    #endif

    vec4 result = texture(inputImage, inUV) * 1.0;
    float total = 1.0;

    for (int i = 1; i <= r; i++) {
        float weight = exp(- float(i * i) / (2.0 * sigma * sigma));
        vec2 uvP = inUV + dir * float(i);
        vec2 uvN = inUV - dir * float(i);
        if (uvP.x >= 0.0 && uvP.x <= 1.0 && uvP.y >= 0.0 && uvP.y <= 1.0) {
            result += texture(inputImage, uvP) * weight;
        }
        if (uvN.x >= 0.0 && uvN.x <= 1.0 && uvN.y >= 0.0 && uvN.y <= 1.0) {
            result += texture(inputImage, uvN) * weight;
        }
        total += weight * 2.0;
    }

    result /= total;

    #ifdef BLUR_SINGLE_CHANNEL
    outResult = result.r;
    #else
    outResult = vec4(result.rgb, 1.0);
    #endif
}

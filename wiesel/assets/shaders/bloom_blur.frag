#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputImage;

// 13-tap Gaussian weights (sigma ~4)
const float weights[7] = float[](
    0.1976,
    0.1746,
    0.1210,
    0.0656,
    0.0278,
    0.0092,
    0.0024
);

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(inputImage, 0));

#ifdef BLUR_VERTICAL
    vec2 dir = vec2(0.0, texelSize.y);
#else
    vec2 dir = vec2(texelSize.x, 0.0);
#endif

    vec3 result = texture(inputImage, inUV).rgb * weights[0];
    for (int i = 1; i < 7; i++) {
        result += texture(inputImage, inUV + dir * float(i)).rgb * weights[i];
        result += texture(inputImage, inUV - dir * float(i)).rgb * weights[i];
    }
    outColor = vec4(result, 1.0);
}

#version 450

layout (set = 0, binding = 0) uniform sampler2D samplerSSAO;
layout (set = 0, binding = 1) uniform sampler2D samplerDepth;

layout (location = 0) in vec2 inUV;

layout (location = 0) out float outFragColor;

void main() {
    // Uniform box-blur
    vec2 texelSize = 1.0 / vec2(textureSize(samplerSSAO, 0));
    /*const int blurRange = 2;
    int n = 0;
    float result = 0.0;
    for (int x = -blurRange; x <= blurRange; x++) {
        for (int y = -blurRange; y <= blurRange; y++) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(samplerSSAO, inUV + offset).r;
            n++;
        }
    }
    outFragColor = result / (float(n));*/

    // Edge aware Bilateral blur
    float centerDepth = texture(samplerDepth, inUV).r;
    float sum = 0, wsum = 0;
    for (int i = -4; i <= 4; i++) {
        #ifdef BLUR_VERTICAL
        vec2 off = vec2(0, texelSize.y * float(i));
        #else
        vec2 off = vec2(texelSize.x * float(i), 0);
        #endif
        float s = texture(samplerSSAO, inUV + off).r;
        float d = texture(samplerDepth, inUV + off).r;
        float w_spatial = exp(-0.5 * (i * i) / 9.0);
        float rangeSigma = 0.1;
        float diff = d - centerDepth;
        float w_range = exp(-0.5 * (diff*diff)/(rangeSigma*rangeSigma));
        //float w_range = exp(-0.5 * ((d - centerDepth) * 100.0) * ((d - centerDepth) * 100.0));
        float w = w_spatial * w_range;
        sum += s * w;
        wsum += w;
    }
    outFragColor = sum / wsum;
}
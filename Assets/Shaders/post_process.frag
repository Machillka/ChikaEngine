#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D HDRSceneColor;
layout(set = 0, binding = 1) uniform PostProcessData {
    vec4 toneMapping;
    vec4 imageOptions;
} postProcess;

/** @brief 使用 ACES Filmic 近似把 HDR 颜色压缩到显示范围。 */
vec3 ToneMapACES(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

/** @brief 对 HDR 高光执行轻量邻域扩散，作为正式多级 Bloom 前的可配置基线。 */
vec3 SampleBloom(vec2 uv)
{
    vec2 texel = postProcess.imageOptions.xy * 2.0;
    vec3 bloom = vec3(0.0);
    float weight = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec3 sampleColor = texture(HDRSceneColor, uv + vec2(x, y) * texel).rgb;
            bloom += max(sampleColor - postProcess.toneMapping.y, vec3(0.0));
            weight += 1.0;
        }
    }
    return bloom / weight * postProcess.toneMapping.z;
}

/** @brief 使用亮度边缘检测执行轻量 FXAA 近似。 */
vec3 SampleFXAA(vec2 uv)
{
    vec2 texel = postProcess.imageOptions.xy;
    vec3 center = texture(HDRSceneColor, uv).rgb;
    vec3 north = texture(HDRSceneColor, uv + vec2(0.0, -texel.y)).rgb;
    vec3 south = texture(HDRSceneColor, uv + vec2(0.0, texel.y)).rgb;
    vec3 east = texture(HDRSceneColor, uv + vec2(texel.x, 0.0)).rgb;
    vec3 west = texture(HDRSceneColor, uv + vec2(-texel.x, 0.0)).rgb;
    vec3 luma = vec3(0.299, 0.587, 0.114);
    float edge = abs(dot(north + south - east - west, luma));
    return edge > 0.08 ? (center * 2.0 + north + south + east + west) / 6.0 : center;
}

void main()
{
    vec3 hdr = postProcess.imageOptions.w > 0.5 ? SampleFXAA(inUV) : texture(HDRSceneColor, inUV).rgb;
    if (postProcess.imageOptions.z > 0.5)
        hdr += SampleBloom(inUV);
    hdr *= postProcess.toneMapping.x;
    vec3 mapped = postProcess.toneMapping.w > 0.5 ? ToneMapACES(hdr) : clamp(hdr, 0.0, 1.0);
    outColor = vec4(pow(mapped, vec3(1.0 / 2.2)), 1.0);
}

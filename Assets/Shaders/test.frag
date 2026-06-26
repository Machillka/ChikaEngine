#version 450

// Import-validated descriptor frequency convention: set 0 = frame, set 1 = material, set 2 = object.
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform MaterialData {
    vec4 BaseColor;
    vec4 Emissive;
    float Metallic;
    float Roughness;
    float OcclusionStrength;
    float NormalScale;
} material;

layout(set = 0, binding = 0) uniform SceneData {
    mat4 cameraVP;
    mat4 lightVP;
    vec4 viewPos;
    vec4 frameOptions;
    vec4 shadowOptions;
} scene;

layout(set = 0, binding = 1) uniform sampler2D shadowMap;
layout(set = 1, binding = 1) uniform sampler2D Albedo;

struct LightData {
    vec4 positionRange;
    vec4 directionType;
    vec4 colorIntensity;
    vec4 spotAngles;
};

layout(set = 0, binding = 2, std430) readonly buffer LightBuffer {
    LightData values[];
} lights;

layout(push_constant) uniform PushConstants {
    mat4 model;
    int isShadowPass;
    int isSkinned;
    int renderMode;
    int useInstancing;
    int useGpuDriven;
} pc;

const float PI = 3.14159265359;

/**
 * 计算 GGX 法线分布函数。
 */
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float nDotH = max(dot(N, H), 0.0);
    float denominator = nDotH * nDotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denominator * denominator, 0.0001);
}

/**
 * 计算 Schlick-GGX 单方向几何遮蔽。
 */
float GeometrySchlickGGX(float nDotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return nDotV / max(nDotV * (1.0 - k) + k, 0.0001);
}

/**
 * 合并观察方向和光照方向的 Smith 几何项。
 */
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

/**
 * 计算 Schlick Fresnel 近似。
 */
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

/**
 * 对主方向光执行可配置 PCF 阴影采样。
 */
float ShadowCalculation(vec3 worldPos, vec3 normal, vec3 lightDirection)
{
    vec4 fragPosLightSpace = scene.lightVP * vec4(worldPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    if (projCoords.z > 1.0 || projCoords.z < 0.0 ||
        any(lessThan(projCoords.xy, vec2(0.0))) || any(greaterThan(projCoords.xy, vec2(1.0))))
        return 0.0;

    float nDotL = max(dot(normal, lightDirection), 0.0);
    float bias = max(scene.frameOptions.z, scene.frameOptions.w * (1.0 - nDotL));
    int radius = int(scene.shadowOptions.z + 0.5);
    float shadow = 0.0;
    float samples = 0.0;
    for (int x = -radius; x <= radius; ++x) {
        for (int y = -radius; y <= radius; ++y) {
            float closestDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * scene.shadowOptions.xy).r;
            shadow += projCoords.z - bias > closestDepth ? 1.0 : 0.0;
            samples += 1.0;
        }
    }
    return shadow / max(samples, 1.0);
}

/**
 * 将一个 RenderWorld Light Proxy 累积为 Metallic-Roughness 直接光。
 */
vec3 EvaluateLight(LightData light, vec3 worldPos, vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0, int lightIndex)
{
    int type = int(light.directionType.w + 0.5);
    vec3 L = normalize(light.directionType.xyz);
    float attenuation = 1.0;
    if (type != 0) {
        vec3 toLight = light.positionRange.xyz - worldPos;
        float distanceToLight = length(toLight);
        L = toLight / max(distanceToLight, 0.0001);
        float range = max(light.positionRange.w, 0.0001);
        float rangeAttenuation = clamp(1.0 - distanceToLight / range, 0.0, 1.0);
        attenuation = rangeAttenuation * rangeAttenuation;
        if (type == 2) {
            float cone = dot(-L, normalize(light.directionType.xyz));
            attenuation *= smoothstep(light.spotAngles.y, light.spotAngles.x, cone);
        }
    }

    vec3 H = normalize(V + L);
    vec3 radiance = light.colorIntensity.rgb * light.colorIntensity.a * attenuation;
    float nDotL = max(dot(N, L), 0.0);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 specular = (D * G * F) / max(4.0 * max(dot(N, V), 0.0) * nDotL, 0.0001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    float shadow = lightIndex == 0 && type == 0 && light.spotAngles.z > 0.5
        ? ShadowCalculation(worldPos, N, L)
        : 0.0;
    return (kD * albedo / PI + specular) * radiance * nDotL * (1.0 - shadow);
}

void main()
{
    if (pc.isShadowPass == 1) {
        outColor = vec4(1.0);
        return;
    }

    vec4 sampledBaseColor = texture(Albedo, inUV) * material.BaseColor;
    vec3 albedo = sampledBaseColor.rgb;
    float metallic = clamp(material.Metallic, 0.0, 1.0);
    float roughness = clamp(material.Roughness, 0.045, 1.0);
    vec3 N = normalize(inNormal);
    vec3 V = normalize(scene.viewPos.xyz - inWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 color = vec3(0.0);
    int lightCount = min(int(scene.frameOptions.y + 0.5), 128);
    for (int index = 0; index < lightCount; ++index)
        color += EvaluateLight(lights.values[index], inWorldPos, N, V, albedo, metallic, roughness, F0, index);

    // Environment fallback keeps the IBL boundary explicit until cubemap prefilter assets are available.
    vec3 diffuseEnvironment = albedo * (1.0 - metallic) * scene.frameOptions.x;
    vec3 specularEnvironment = F0 * (1.0 - roughness) * scene.frameOptions.x;
    color += (diffuseEnvironment + specularEnvironment) * material.OcclusionStrength;
    color += material.Emissive.rgb;
    outColor = vec4(color, sampledBaseColor.a);
}

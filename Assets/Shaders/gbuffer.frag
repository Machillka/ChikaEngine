#version 450

// Import-validated descriptor frequency convention: set 0 = frame, set 1 = material, set 2 = object.
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outAlbedoMetallic;
layout(location = 1) out vec4 outNormalRoughness;
layout(location = 2) out vec4 outEmissiveOcclusion;
layout(location = 3) out vec4 outPosition;

layout(set = 1, binding = 0) uniform MaterialData {
    vec4 BaseColor;
    vec4 Emissive;
    float Metallic;
    float Roughness;
    float OcclusionStrength;
    float NormalScale;
} material;

layout(set = 1, binding = 1) uniform sampler2D Albedo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    int isShadowPass;
    int isSkinned;
    int renderMode;
    int useInstancing;
} pc;

void main()
{
    vec4 baseColor = texture(Albedo, inUV) * material.BaseColor;
    outAlbedoMetallic = vec4(baseColor.rgb, clamp(material.Metallic, 0.0, 1.0));
    outNormalRoughness = vec4(normalize(inNormal) * 0.5 + 0.5, clamp(material.Roughness, 0.045, 1.0));
    outEmissiveOcclusion = vec4(material.Emissive.rgb, clamp(material.OcclusionStrength, 0.0, 1.0));
    outPosition = vec4(inWorldPos, baseColor.a);
}

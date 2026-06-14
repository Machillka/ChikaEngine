#version 450

// Import-validated descriptor frequency convention: set 0 = frame, set 1 = material, set 2 = object.
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outMaterial;

layout(set = 1, binding = 0) uniform MaterialData {
    vec4 BaseColor;
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
    vec3 albedoColor = texture(Albedo, inUV).rgb * material.BaseColor.rgb;
    vec3 normal = normalize(inNormal);

    outAlbedo = vec4(albedoColor, material.BaseColor.a);
    outNormal = vec4(normal * 0.5 + 0.5, 1.0);
    outMaterial = vec4(0.5, 0.0, 0.0, 1.0);
}

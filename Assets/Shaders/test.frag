#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform MaterialData {
    vec4 BaseColor;
} material;

layout(set = 0, binding = 1) uniform SceneData {
    mat4 cameraVP;
    mat4 lightVP;
    vec4 lightDir;
    vec4 viewPos;
} scene;

layout(set = 0, binding = 4) uniform sampler2D shadowMap;
layout(set = 0, binding = 5) uniform sampler2D Albedo;

layout(push_constant) uniform PC {
    mat4 model;
    int isShadowPass;
    int isSkinned;
} pc;

float ShadowCalculation(vec3 worldPos, vec3 normal)
{
    vec4 fragPosLightSpace = scene.lightVP * vec4(worldPos, 1.0);

    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.z < 0.0)
        return 0.0;
    if (projCoords.x > 1.0 || projCoords.x < 0.0 ||
        projCoords.y > 1.0 || projCoords.y < 0.0)
        return 0.0;

    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;

    vec3 L = normalize(scene.lightDir.xyz);
    float ndotl = max(dot(normalize(normal), L), 0.0);

    float bias = max(0.005 * (1.0 - ndotl), 0.001);

    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
    return shadow;
}

void main()
{
    if (pc.isShadowPass == 1) {
        outColor = vec4(1.0);
        return;
    }

    vec3 N = normalize(inNormal);
    vec3 L = normalize(scene.lightDir.xyz);
    vec3 V = normalize(scene.viewPos.xyz - inWorldPos);
    vec3 H = normalize(L + V);

    N = normalize(mix(N, V, 0.2));

    vec3 albedoColor = texture(Albedo, inUV).rgb;
    vec3 objectColor = albedoColor * material.BaseColor.rgb;

    vec3 ambient = 0.4 * objectColor;

    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * objectColor;

    float spec = pow(max(dot(N, H), 0.0), 32.0);
    vec3 specular = spec * vec3(0.3);

    float shadow = ShadowCalculation(inWorldPos, N);

    float rim = 1.0 - max(dot(N, V), 0.0);
    rim = pow(rim, 2.0);             // 柔和一点
    vec3 rimColor = rim * objectColor * 0.4;

    vec3 lighting =
        ambient +
        (1.0 - shadow) * (diffuse + specular) +
        rimColor;

    outColor = vec4(lighting, 1.0);
}
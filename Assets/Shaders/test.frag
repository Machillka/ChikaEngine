#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform SceneData {
    mat4 cameraVP;
    mat4 lightVP;
    vec4 lightDir;
    vec4 viewPos;
} scene;

layout(set = 0, binding = 4) uniform sampler2D shadowMap;

layout(push_constant) uniform PC {
    mat4 model;
    int isShadowPass;
} pc;

// 简单版阴影计算：先保证能看到阴影，再慢慢调优
float ShadowCalculation(vec3 worldPos, vec3 normal)
{
    vec4 fragPosLightSpace = scene.lightVP * vec4(worldPos, 1.0);

    // 透视除法
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // NDC [-1,1] -> [0,1]
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // 超出光源视锥：不投影
    if (projCoords.z > 1.0 || projCoords.z < 0.0)
        return 0.0;
    if (projCoords.x > 1.0 || projCoords.x < 0.0 ||
        projCoords.y > 1.0 || projCoords.y < 0.0)
        return 0.0;

    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;

    vec3 L = normalize(scene.lightDir.xyz);
    float ndotl = max(dot(normalize(normal), L), 0.0);

    // 固定 bias + 角度相关，先保证不自阴影
    float bias = max(0.005 * (1.0 - ndotl), 0.001);

    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
    return shadow;
}

void main()
{
    // 阴影 Pass：只关心深度，颜色随便写
    if (pc.isShadowPass == 1) {
        outColor = vec4(1.0);
        return;
    }

    vec3 N = normalize(inNormal);
    vec3 L = normalize(scene.lightDir.xyz);
    vec3 V = normalize(scene.viewPos.xyz - inWorldPos);
    vec3 H = normalize(L + V);

    vec3 objectColor = vec3(0.8, 0.8, 0.8);

    vec3 ambient = 0.2 * objectColor;
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * objectColor;

    float spec = pow(max(dot(N, H), 0.0), 32.0);
    vec3 specular = spec * vec3(0.3);

    float shadow = ShadowCalculation(inWorldPos, N);

    // 阴影只压 diffuse + specular，保留一点环境光
    vec3 lighting = ambient + (1.0 - shadow) * (diffuse + specular);

    outColor = vec4(lighting, 1.0);
}

#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform SceneData {
    mat4 cameraVP;
    mat4 lightVP;
    vec4 lightDir;
    vec4 viewPos;
} scene;

layout(set = 0, binding = 4) uniform sampler2D GBufferAlbedo;
layout(set = 0, binding = 5) uniform sampler2D GBufferNormal;
layout(set = 0, binding = 6) uniform sampler2D GBufferMaterial;

void main()
{
    vec3 albedo = texture(GBufferAlbedo, inUV).rgb;
    vec3 normal = normalize(texture(GBufferNormal, inUV).xyz * 2.0 - 1.0);
    vec3 material = texture(GBufferMaterial, inUV).rgb;

    vec3 L = normalize(scene.lightDir.xyz);
    float ndotl = max(dot(normal, L), 0.0);

    vec3 ambient = 0.35 * albedo;
    vec3 diffuse = ndotl * albedo;
    vec3 color = ambient + diffuse;

    float roughness = material.r;
    color += albedo * (1.0 - roughness) * 0.05;

    outColor = vec4(color, 1.0);
}

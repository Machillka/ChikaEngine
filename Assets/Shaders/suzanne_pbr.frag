#version 330 core

in vec3 vWorldPos;
in vec2 vUV;
in mat3 vTBN;

out vec4 FragColor;

uniform vec3 u_CameraPos;
uniform vec3 u_LightDir;
uniform vec3 u_AmbientColor;

uniform sampler2D u_AlbedoTex;
uniform sampler2D u_NormalTex;
uniform sampler2D u_RoughnessTex;

void main()
{
    vec3 albedo = texture(u_AlbedoTex, vUV).rgb;
    vec3 normalTex = texture(u_NormalTex, vUV).xyz * 2.0 - 1.0;
    vec3 N = normalize(vTBN * normalTex);
    float roughness = clamp(texture(u_RoughnessTex, vUV).r, 0.04, 1.0);
    float metallic = 0.0;

    vec3 L = normalize(u_LightDir);
    vec3 V = normalize(u_CameraPos - vWorldPos);
    vec3 H = normalize(L + V);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denomD = (NdotH * NdotH * (alpha2 - 1.0) + 1.0);
    float D = alpha2 / (3.14159 * denomD * denomD + 1e-4);

    float k = (roughness + 1.0);
    k = (k * k) / 8.0;
    float Gv = NdotV / (NdotV * (1.0 - k) + k);
    float Gl = NdotL / (NdotL * (1.0 - k) + k);
    float G = Gv * Gl;

    vec3 F0 = vec3(0.1); // 稍微提亮高光
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    vec3 specular = (D * G * F) / max(4.0 * NdotL * NdotV, 1e-4);
    vec3 kd = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kd * albedo / 3.14159;
    vec3 ambient = u_AmbientColor * albedo;

    float rim = pow(1.0 - max(dot(N, V), 0.0), 2.0);
    vec3 rimColor = vec3(1.0) * rim * 0.25;

    vec3 lit = ambient + (diffuse + specular) * NdotL;
    vec3 color = pow(lit, vec3(1.0 / 2.2)) + rimColor;

    FragColor = vec4(color, 1.0);
}

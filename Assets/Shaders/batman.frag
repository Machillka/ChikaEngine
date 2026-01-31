#version 330 core

in vec3 vWorldPos;
in vec2 vUV;
in mat3 vTBN;

out vec4 FragColor;

uniform vec3 u_CameraPos;
uniform vec3 u_LightDir;

uniform sampler2D u_AlbedoTex;
uniform sampler2D u_NormalTex;
uniform sampler2D u_SpecularTex;

uniform vec3 u_AmbientColor;

void main()
{
    vec3 albedo = texture(u_AlbedoTex, vUV).rgb;

    vec3 normalTex = texture(u_NormalTex, vUV).xyz * 2.0 - 1.0;
    vec3 N = normalize(vTBN * normalTex);

    vec3 L = normalize(u_LightDir);
    vec3 V = normalize(u_CameraPos - vWorldPos);
    vec3 R = reflect(-L, N);

    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * albedo;

    float specStrength = texture(u_SpecularTex, vUV).r;
    float spec = pow(max(dot(V, R), 0.0), 32.0);
    vec3 specular = specStrength * spec * vec3(1.0);

    vec3 ambient = u_AmbientColor * albedo;

    vec3 finalColor = ambient + diffuse + specular;
    finalColor = pow(finalColor, vec3(1.0 / 2.2));

    FragColor = vec4(finalColor, 1.0);
}

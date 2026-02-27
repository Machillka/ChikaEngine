#version 330 core
out vec4 FragColor;

in vec3 v_Normal;
in vec3 v_Position;

uniform vec3 u_CameraPos;
uniform samplerCube u_Skybox;

// 可调参数
uniform float u_IOR = 1.52; // 折射率 (Index of Refraction): 玻璃约 1.52, 水 1.33, 钻石 2.42
uniform float u_FresnelBias = 0.1;
uniform float u_FresnelScale = 1.0;
uniform float u_FresnelPower = 2.0;

void main()
{
    vec3 I = normalize(v_Position - u_CameraPos); // 视线向量 (从相机指向片元)
    vec3 N = normalize(v_Normal);                 // 法线

    vec3 R = reflect(I, N);
    float ratio = 1.0 / u_IOR;
    vec3 T = refract(I, N, ratio);

    vec4 reflectionColor = texture(u_Skybox, R);
    vec4 refractionColor = texture(u_Skybox, T);

    float fresnel = u_FresnelBias + u_FresnelScale * pow(1.0 + dot(I, N), u_FresnelPower);
    fresnel = clamp(fresnel, 0.0, 1.0);

    vec4 finalColor = mix(refractionColor, reflectionColor, fresnel);
    
    FragColor = finalColor;
}
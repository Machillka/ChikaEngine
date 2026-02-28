#version 330 core
out vec4 FragColor;

in vec3 v_Normal;
in vec3 v_Position;
in vec3 v_ViewDir;

uniform vec3 u_CameraPos;
uniform samplerCube u_Skybox;

uniform float u_IOR = 1.52;             
uniform float u_Dispersion = 0.02;      
uniform float u_Reflectivity = 0.5;     
uniform float u_Shininess = 64.0;       
uniform vec3 u_LightDir = vec3(0.5, 1.0, 0.5); // 假定光源方向
uniform vec3 u_Tint = vec3(1.0);

void main()
{
    vec3 N = normalize(v_Normal);
    vec3 I = normalize(v_ViewDir);

    float F0 = pow((1.0 - u_IOR) / (1.0 + u_IOR), 2.0);
    float fresnel = F0 + (1.0 - F0) * pow(1.0 + dot(I, N), 5.0);

    vec3 R = reflect(I, N);
    vec3 reflectionColor = texture(u_Skybox, R).rgb;

    float ratioR = 1.0 / u_IOR;
    float ratioG = 1.0 / (u_IOR + u_Dispersion);
    float ratioB = 1.0 / (u_IOR + u_Dispersion * 2.0);

    vec3 TR = refract(I, N, ratioR);
    vec3 TG = refract(I, N, ratioG);
    vec3 TB = refract(I, N, ratioB);

    float r = texture(u_Skybox, TR).r;
    float g = texture(u_Skybox, TG).g;
    float b = texture(u_Skybox, TB).b;
    vec3 refractionColor = vec3(r, g, b) * u_Tint;

    vec3 lightDir = normalize(u_LightDir);
    vec3 halfDir = normalize(lightDir - I);
    float spec = pow(max(dot(N, halfDir), 0.0), u_Shininess);
    vec3 specularColor = vec3(1.0) * spec;

    vec3 color = mix(refractionColor, reflectionColor, fresnel * u_Reflectivity);
    
    color += specularColor;

    FragColor = vec4(color, 1.0);
}
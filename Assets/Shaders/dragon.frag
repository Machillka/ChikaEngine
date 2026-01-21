#version 330 core

in vec3 v_Normal;
in vec3 v_WorldPos;

uniform vec3 u_CameraPos;
uniform vec4 u_Color;
uniform vec4 u_RimColor;
uniform float u_RimPower;
uniform float u_RimStrength;

out vec4 FragColor;

void main()
{
    vec3 N = normalize(v_Normal);
    vec3 V = normalize(u_CameraPos - v_WorldPos);
    float rim = pow(1.0 - max(dot(N, V), 0.0), u_RimPower);
    vec4 rimLight = u_RimColor * rim * u_RimStrength;

    FragColor = u_Color + rimLight;
}
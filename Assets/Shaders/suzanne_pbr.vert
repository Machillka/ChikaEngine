#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

uniform mat4 u_MVP;
uniform mat4 u_Model;

out vec3 vWorldPos;
out vec2 vUV;
out mat3 vTBN;

void main()
{
    vec4 worldPos = u_Model * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vUV = aUV;

    vec3 T = normalize(mat3(u_Model) * aTangent);
    vec3 B = normalize(mat3(u_Model) * aBitangent);
    vec3 N = normalize(mat3(u_Model) * aNormal);
    vTBN = mat3(T, B, N);

    gl_Position = u_MVP * vec4(aPos, 1.0);
}

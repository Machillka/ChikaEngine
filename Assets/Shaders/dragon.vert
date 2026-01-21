#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 u_MVP;
uniform mat4 u_Model;

out vec3 v_Normal;
out vec3 v_WorldPos;

void main()
{
    vec4 worldPos = u_Model * vec4(aPos, 1.0);
    v_WorldPos = worldPos.xyz;

    v_Normal = mat3(transpose(inverse(u_Model))) * aNormal;

    gl_Position = u_MVP * vec4(aPos, 1.0);
}

#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 v_Normal;
out vec3 v_Position;
out vec3 v_ViewDir;

uniform mat4 u_Model;
uniform mat4 u_MVP;
uniform vec3 u_CameraPos;

void main()
{
    vec4 worldPos = u_Model * vec4(aPos, 1.0);
    v_Position = worldPos.xyz;
    v_Normal = mat3(transpose(inverse(u_Model))) * aNormal;
    v_ViewDir = normalize(worldPos.xyz - u_CameraPos);
    
    gl_Position = u_MVP * vec4(aPos, 1.0);
}
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 v_Normal;
out vec3 v_Position;

uniform mat4 u_Model;
uniform mat4 u_MVP;

void main()
{
    v_Normal = mat3(transpose(inverse(u_Model))) * aNormal;
    
    v_Position = vec3(u_Model * vec4(aPos, 1.0));
    
    gl_Position = u_MVP * vec4(aPos, 1.0);
}
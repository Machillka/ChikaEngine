#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

// 只输出最基础、插值最稳定的世界空间属性
layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;

layout(set = 0, binding = 1) uniform SceneData {
    mat4 cameraVP;
    mat4 lightVP;
    vec4 lightDir;
    vec4 viewPos;
} scene;

layout(push_constant) uniform PC {
    mat4 model;
    int isShadowPass;
} pc;

void main()
{
    vec4 worldPos = pc.model * vec4(inPos, 1.0);
    outWorldPos = worldPos.xyz;

    outNormal = mat3(pc.model) * inNormal;
    outUV = inUV;

    if (pc.isShadowPass == 1) {
        gl_Position = scene.lightVP * worldPos;
    } else {
        gl_Position = scene.cameraVP * worldPos;
    }
}
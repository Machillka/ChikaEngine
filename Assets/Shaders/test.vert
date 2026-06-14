#version 450

// Import-validated descriptor frequency convention: set 0 = frame, set 1 = material, set 2 = object.
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;


layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;

layout(set = 0, binding = 0) uniform SceneData {
    mat4 cameraVP;
    mat4 lightVP;
    vec4 viewPos;
    vec4 frameOptions;
    vec4 shadowOptions;
} scene;

layout(set = 2, binding = 1, std430) readonly buffer InstanceData {
    mat4 models[];
} instances;

layout(push_constant) uniform PushConstants {
    mat4 model;
    int isShadowPass;
    int isSkinned;
    int renderMode;
    int useInstancing;
} pc;

void main()
{
    mat4 model = pc.useInstancing == 1 ? instances.models[gl_InstanceIndex] : pc.model;
    vec4 worldPos = model * vec4(inPos, 1.0);
    outWorldPos = worldPos.xyz;

    outNormal = mat3(model) * inNormal;
    outUV = inUV;

    if (pc.isShadowPass == 1) {
        gl_Position = scene.lightVP * worldPos;
    } else {
        gl_Position = scene.cameraVP * worldPos;
    }
}

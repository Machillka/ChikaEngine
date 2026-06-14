#version 450

// Import-validated descriptor frequency convention: set 0 = frame, set 1 = material, set 2 = object.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 3) in ivec4 inBoneIndices;
layout(location = 4) in vec4 inBoneWeights;


layout(set = 0, binding = 0) uniform SceneData {
    mat4 cameraVP;
    mat4 lightVP;
    vec4 viewPos;
    vec4 frameOptions;
    vec4 shadowOptions;
} scene;

layout(set = 2, binding = 0, std430) readonly buffer BoneData {
    mat4 boneMatrices[];
} uboBones;

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

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;

void main() {
    mat4 skinMat = mat4(1.0);

    if (pc.isSkinned == 1) {
        skinMat =
            inBoneWeights.x * uboBones.boneMatrices[inBoneIndices.x] +
            inBoneWeights.y * uboBones.boneMatrices[inBoneIndices.y] +
            inBoneWeights.z * uboBones.boneMatrices[inBoneIndices.z] +
            inBoneWeights.w * uboBones.boneMatrices[inBoneIndices.w];
    }

    vec4 localPos;
    vec3 localNormal;

    if (pc.isSkinned == 1) {
        localPos = skinMat * vec4(inPosition, 1.0);
        localNormal = mat3(skinMat) * inNormal;
    } else {
        localPos = vec4(inPosition, 1.0);
        localNormal = inNormal;
    }

    mat4 model = pc.useInstancing == 1 ? instances.models[gl_InstanceIndex] : pc.model;
    vec4 worldPos = model * localPos;
    vec3 worldNormal = normalize(mat3(model) * localNormal);

    if (pc.isShadowPass == 1) {
        gl_Position = scene.lightVP * worldPos;
    } else {
        gl_Position = scene.cameraVP * worldPos;
    }

    outUV = inUV;
    outWorldPos = worldPos.xyz;
    outNormal = worldNormal;
}

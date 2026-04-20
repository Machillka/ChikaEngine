#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 3) in ivec4 inBoneIndices;
layout(location = 4) in vec4 inBoneWeights;


layout(set = 0, binding = 1) uniform SceneData {
    mat4 cameraVP;
    mat4 lightVP;
    vec4 lightDir;
    vec4 viewPos;
} scene;

layout(set = 0, binding = 2) uniform BoneData {
    mat4 boneMatrices[128];
} uboBones;

layout(push_constant) uniform PushConstants {
    mat4 modelMatrix;
    int isShadowPass;
    int isSkinned;
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

    vec4 worldPos = pc.modelMatrix * localPos;
    vec3 worldNormal = normalize(mat3(pc.modelMatrix) * localNormal);

    if (pc.isShadowPass == 1) {
        gl_Position = scene.lightVP * worldPos;
    } else {
        gl_Position = scene.cameraVP * worldPos;
    }

    outUV = inUV;
    outWorldPos = worldPos.xyz;
    outNormal = worldNormal;
}
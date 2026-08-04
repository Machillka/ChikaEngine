#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec2 inNDC;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform SkyboxData {
    mat4 inverseViewProjection;
    vec4 options;
} skybox;
layout(set = 0, binding = 1) uniform samplerCube EnvironmentSkybox;
layout(set = 0, binding = 2) uniform sampler2D SceneDepth;

void main()
{
    // Deferred runs after opaque geometry and only fills pixels that still contain the clear depth.
    if (skybox.options.y > 0.5) {
        float depth = texture(SceneDepth, inUV).r;
        if (depth < skybox.options.z - skybox.options.w)
            discard;
    }

    vec4 worldPosition = skybox.inverseViewProjection * vec4(inNDC, 1.0, 1.0);
    vec3 direction = normalize(worldPosition.xyz / max(abs(worldPosition.w), 0.00001));
    outColor = vec4(texture(EnvironmentSkybox, direction).rgb * skybox.options.x, 1.0);
}

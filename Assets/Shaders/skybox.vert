#version 450

// Skybox uses a fullscreen triangle and reconstructs a world-space ray in the fragment stage.
layout(location = 0) out vec2 outUV;
layout(location = 1) out vec2 outNDC;

void main()
{
    vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    outUV = pos;
    outNDC = pos * 2.0 - 1.0;
    gl_Position = vec4(outNDC, 0.0, 1.0);
}

#version 450

// Fullscreen passes use gl_VertexIndex and intentionally declare no reflected vertex input layout.
layout(location = 0) out vec2 outUV;

void main()
{
    vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    outUV = pos;
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}

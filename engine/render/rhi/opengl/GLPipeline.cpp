// GLPipeline.cpp
#include "GLPipeline.h"

#include "debug/log_macros.h"

namespace ChikaEngine::Render
{
    GLuint GLPipeline::CompileShader(GLenum type, const char* src)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char info[512];
            glGetShaderInfoLog(shader, 512, nullptr, info);
            // std::cerr << "Shader compile error: " << info << std::endl;
        }

        return shader;
    }

    GLPipeline::GLPipeline(const char* vsSource, const char* fsSource)
    {
        GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSource);
        GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);

        _program = glCreateProgram();
        glAttachShader(_program, vs);
        glAttachShader(_program, fs);
        glLinkProgram(_program);

        glDeleteShader(vs);
        glDeleteShader(fs);

        GLint success;
        glGetProgramiv(_program, GL_LINK_STATUS, &success);
        if (!success)
        {
            char info[512];
            glGetProgramInfoLog(_program, 512, nullptr, info);
            // std::cerr << "Program link error: " << info << std::endl;
            LOG_ERROR("Render", info);
        }
    }

    GLPipeline::~GLPipeline()
    {
        glDeleteProgram(_program);
    }
} // namespace ChikaEngine::Render

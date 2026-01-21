// GLPipeline.cpp
#include "GLPipeline.h"

#include "debug/log_macros.h"
#include "math/mat4.h"
#include "math/vector3.h"

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
    void GLPipeline::Bind()
    {
        glUseProgram(_program);
    }

    GLint GLPipeline::GetLocation(const char* name) const
    {
        GLint loc = glGetUniformLocation(_program, name);
        return loc;
    }

    void GLPipeline::SetUniformFloat(const char* name, float v)
    {
        GLint loc = GetLocation(name);
        if (loc >= 0)
            glUniform1f(loc, v);
    }

    void GLPipeline::SetUniformVec3(const char* name, const std::array<float, 3>& v)
    {
        GLint loc = GetLocation(name);
        if (loc >= 0)
            glUniform3fv(loc, 1, v.data());
    }

    void GLPipeline::SetUniformVec4(const char* name, const std::array<float, 4>& v)
    {
        GLint loc = GetLocation(name);
        if (loc >= 0)
            glUniform4fv(loc, 1, v.data());
    }

    void GLPipeline::SetUniformMat4(const char* name, const ChikaEngine::Math::Mat4& m)
    {
        GLint loc = GetLocation(name);
        if (loc >= 0)
            glUniformMatrix4fv(loc, 1, GL_TRUE, m.m.data());
    }

    void GLPipeline::SetUniformTexture(const char* name, IRHITexture2D* tex, int slot)
    {
        if (!tex)
            return;
        GLint loc = GetLocation(name);
        if (loc < 0)
            return;
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, (GLuint)tex->Handle());
        glUniform1i(loc, slot);
    }

    GLPipeline::~GLPipeline()
    {
        glDeleteProgram(_program);
    }
} // namespace ChikaEngine::Render

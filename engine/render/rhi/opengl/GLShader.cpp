#include "GLShader.h"

#include "glheader.h"
#include "math/mat4.h"
#include "math/vector3.h"

#include <stdexcept>

namespace ChikaEngine::Render
{
    //  内部工具函数封装
    namespace
    {
        unsigned int CompileShader(unsigned int type, const std::string& source)
        {
            const auto shader = glCreateShader(type);
            const char* src = source.c_str();
            glShaderSource(shader, 1, &src, nullptr);
            glCompileShader(shader);

            int success = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (success == GL_FALSE)
            {
                char infoLog[1024];
                glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
                throw std::runtime_error(std::string{"Shader compile error: "} + infoLog);
            }
            return shader;
        }

        unsigned int LinkProgram(unsigned int vs, unsigned int fs)
        {
            const auto program = glCreateProgram();
            glAttachShader(program, vs);
            glAttachShader(program, fs);
            glLinkProgram(program);
            int success = 0;
            glGetProgramiv(program, GL_COMPILE_STATUS, &success);
            if (success == GL_FALSE)
            {
                char infoLog[1024];
                glGetProgramInfoLog(program, 1024, nullptr, infoLog);
                throw std::runtime_error(std::string{"Shader compile error: "} + infoLog);
            }
            glDeleteShader(vs);
            glDeleteShader(fs);
            return program;
        }
    } // namespace

    GLShader::GLShader(const ShaderSource& src)
    {
        const auto vs = CompileShader(GL_VERTEX_SHADER, src.vertex);
        const auto fs = CompileShader(GL_FRAGMENT_SHADER, src.fragment);
        _programID = LinkProgram(vs, fs);
    }

    GLShader::~GLShader()
    {
        if (_programID)
        {
            glDeleteProgram(_programID);
            _programID = 0;
        }
    }

    void GLShader::Bind() const
    {
        glUseProgram(_programID);
    }
    void GLShader::Unbind() const
    {
        glUseProgram(0);
    }
    int GLShader::GetUniformLocation(const char* name) const
    {
        return glGetUniformLocation(_programID, name);
    }
    void GLShader::SetMat4(const char* name, const Math::Mat4& m) const
    {
        const int loc = GetUniformLocation(name);
        if (loc != -1)
        {
            glUniformMatrix4fv(loc, 1, GL_FALSE, m.m.data());
        }
    }
    void GLShader::SetVec3(const char* name, const Math::Vector3& v) const
    {
        const int loc = GetUniformLocation(name);
        if (loc != -1)
        {
            glUniform3f(loc, v.x, v.y, v.z);
        }
    }
} // namespace ChikaEngine::Render
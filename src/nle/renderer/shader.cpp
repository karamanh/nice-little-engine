#include "shader.h"
#include "../core/utils.h"

#include <algorithm>

namespace nle
{
namespace
{
#ifdef NLE_GLES
    /**
     * @brief A desktop shader, made one OpenGL ES 3 will compile.
     *
     * The engine's shaders are written for GLSL 3.30, which ES 3.00 differs
     * from in its first line and in having no precision of its own for a
     * fragment shader: both are said here, once, rather than every shader
     * being written twice.
     */
    std::string for_es(std::string_view source, bool fragment)
    {
        std::string rest(source);

        // After whatever blank lines a source written inline starts with.
        const size_t first = rest.find_first_not_of(" \t\r\n");

        if(first != std::string::npos && rest.compare(first, 8, "#version") == 0)
        {
            const size_t line_end = rest.find('\n', first);
            rest = line_end == std::string::npos ? std::string() : rest.substr(line_end + 1);
        }

        // A uniform with a value of its own is a desktop's: ES gives every
        // one nought to start with and nothing else. The engine's are all
        // either nought or set on every draw (surface_uniforms), so the
        // value is simply dropped.
        size_t line = 0;

        while(line < rest.size())
        {
            const size_t end = std::min(rest.find('\n', line), rest.size());
            const size_t first = rest.find_first_not_of(" \t", line);

            if(first != std::string::npos && first < end && rest.compare(first, 8, "uniform ") == 0)
            {
                const size_t equals = rest.find('=', first);
                const size_t semicolon = rest.find(';', first);

                if(equals < semicolon && semicolon < end)
                {
                    rest.erase(equals, semicolon - equals);
                    continue;
                }
            }

            line = end + 1;
        }

        std::string made = "#version 300 es\n";

        if(fragment)
        {
            made += "precision highp float;\n"
                    "precision highp int;\n"
                    "precision highp sampler2D;\n"
                    "precision highp sampler2DShadow;\n";
        }

        // Kept on the line it was on, so a compiler's line numbers still
        // point into the file as written.
        made += "#line 2\n";
        return made + rest;
    }
#endif
}
    shader::shader(const std::string &vertex_source, const std::string &fragment_source, enum shader_source source)
    {
        if(source == shader_source::file)
        {
            m_vertex_source = utils::load_file(vertex_source);
            m_fragment_source = utils::load_file(fragment_source);
        }
        else if(source == shader_source::memory)
        {
            // we treat params vertex_shader and fragment_shader as sources directly
            m_vertex_source = vertex_source;
            m_fragment_source = fragment_source;
        }
    }

    void shader::load()
    {
        compile_shaders();
    }

    void shader::use()
    {
        glUseProgram(m_program);
    }

    void shader::unuse()
    {
        glUseProgram(0);
    }

    void shader::unload()
    {
        if(m_vertex_shader != 0)
        {
            glDeleteShader(m_vertex_shader);
        }
        else if(m_fragment_shader != 0)
        {
            glDeleteShader(m_fragment_shader);
        }
        else if(m_geometry_shader != 0)
        {
            glDeleteShader(m_geometry_shader);
        }
    }

    unsigned int shader::program() const
    {
        return m_program;
    }

    int shader::uniform_location(const std::string &uniform) const
    {
        auto it = m_uniform_locations.find(uniform);
        if(it != m_uniform_locations.end())
        {
            return it->second;
        }

        int location = glGetUniformLocation(m_program, uniform.c_str());
        m_uniform_locations.emplace(uniform, location);
        return location;
    }

    void shader::compile_shaders()
    {
        /// a relink invalidates every previously resolved location.
        m_uniform_locations.clear();

        m_program = glCreateProgram();

        if(m_program == 0)
        {
            utils::prerror("error creating shader program");
        }

        GLint result = 0;
        GLchar error_log[1024] = {0};

        attach_shaders(shader_type::vertex);
        attach_shaders(shader_type::fragment);

        glLinkProgram(m_program);
        glGetProgramiv(m_program, GL_LINK_STATUS, &result);

        if(result == 0)
        {
            glGetProgramInfoLog(m_program, sizeof(error_log), nullptr, error_log);
            utils::prerror("error linking shader program", error_log);
            return;
        }

        // Every sampler starts on unit nought, and two of different kinds on
        // one unit is an error a phone's driver holds a draw to: nothing is
        // drawn at all until whoever sets the shadow map's unit has. So a
        // shadow sampler starts on a unit of its own, the last there is sure
        // to be.
        {
            constexpr GLint LAST_UNIT = 15;

            GLint uniforms = 0;
            GLint was_using = 0;

            glGetProgramiv(m_program, GL_ACTIVE_UNIFORMS, &uniforms);
            glGetIntegerv(GL_CURRENT_PROGRAM, &was_using);
            glUseProgram(m_program);

            for(GLint i = 0; i < uniforms; ++i)
            {
                GLchar name[128] = {0};
                GLint size = 0;
                GLenum type = 0;

                glGetActiveUniform(m_program, static_cast<GLuint>(i), sizeof(name), nullptr, &size, &type, name);

                if(type == GL_SAMPLER_2D_SHADOW)
                {
                    glUniform1i(glGetUniformLocation(m_program, name), LAST_UNIT);
                }
            }

            glUseProgram(static_cast<GLuint>(was_using));
        }

        glValidateProgram(m_program);
        glGetProgramiv(m_program, GL_VALIDATE_STATUS, &result);

        if(result == 0)
        {
            glGetProgramInfoLog(m_program, sizeof(error_log), nullptr, error_log);
            utils::prerror("error validating shader program", error_log);
            return;
        }
    }

    void shader::attach_shaders(enum shader_type type)
    {
        std::string_view source;
        std::string stype;

        switch (type)
        {
        case shader_type::vertex:
            stype =  "vertex";
            source = m_vertex_source;
            break;
        case shader_type::fragment:
            stype = "fragment";
            source = m_fragment_source;
            break;
        case shader_type::geometry:
            stype = "geometry";
            source = m_geometry_source;
            break;
        }

#ifdef NLE_GLES
        const std::string translated = for_es(source, type == shader_type::fragment);
        source = translated;
#endif

        GLuint shader = glCreateShader(static_cast<GLuint>(type));
        const GLchar *shader_source[1];
        shader_source[0] = source.data();

        GLint source_len[1];
        source_len[0] = source.length();

        glShaderSource(shader, 1, shader_source, source_len);
        glCompileShader(shader);

        GLint result = 0;
        GLchar error_log[1024] = {0};

        glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
        if(result == 0)
        {
            glGetShaderInfoLog(shader, sizeof(error_log), nullptr, error_log);
            utils::prerror("error compiling " + stype, error_log);
        }

        glAttachShader(m_program, shader);
    }
} // namespace nle

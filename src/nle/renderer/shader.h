/**
 * @file shader.h
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief 
 * @version 0.1
 * @date 2024-02-11
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#pragma once

#include "gl.h"

#include <string>
#include <future>
#include <unordered_map>

namespace nle
{

enum class shader_source
{
    file,
    memory
};

enum class shader_type
{
    vertex = GL_VERTEX_SHADER,
    fragment = GL_FRAGMENT_SHADER,
#ifdef NLE_GLES
    // OpenGL ES 3.0 has none, and the engine uses none.
    geometry = 0x8DD9
#else
    geometry = GL_GEOMETRY_SHADER
#endif
};

class shader
{
public:
    shader() = delete;
    shader(const std::string& vertex_source, const std::string& fragment_source, enum shader_source source = shader_source::file);
    
    void load();

    void use();

    void unuse();

    void unload();
    
    unsigned int program() const;

    /// Cached lookup of a uniform's location. Returns -1 when the uniform is
    /// not active in the linked program.
    int uniform_location(const std::string& uniform) const;

private:
    GLuint m_program;

    /// glGetUniformLocation() is a driver round trip, and the renderer asks for
    /// the same handful of names every frame. Locations are stable for the
    /// lifetime of a linked program, so they are memoised here.
    mutable std::unordered_map<std::string, int> m_uniform_locations;

    GLuint m_vertex_shader;

    GLuint m_fragment_shader;

    GLuint m_geometry_shader;

    /// vertex shader source
    std::string m_vertex_source;

    /// fragment shader source
    std::string m_fragment_source;

    /// geometry shader source
    std::string m_geometry_source;

    std::future<bool> m_ready;

    void compile_shaders();

    void attach_shaders(enum shader_type type);

    friend class renderer_3d;
};
} // namespace nle

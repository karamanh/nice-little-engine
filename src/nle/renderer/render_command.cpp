/**
 * @file render_command.cpp
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief Render command system implementation
 * @version 0.1
 * @date 2024-11-07
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "render_command.h"

namespace nle
{

render_command_buffer::render_command_buffer()
{
    m_commands.reserve(100); // Reserve some initial capacity
}

render_command_buffer::~render_command_buffer()
{
}

void render_command_buffer::set_polygon_mode(unsigned int front_and_back, unsigned int mode)
{
    render_command cmd;
    cmd.type = command_type::set_polygon_mode;
    cmd.data = polygon_mode_data{ front_and_back, mode };
    m_commands.push_back(cmd);
}

void render_command_buffer::use_shader(ref<shader> shader)
{
    render_command cmd;
    cmd.type = command_type::use_shader;
    cmd.data = shader;
    m_commands.push_back(cmd);
}

void render_command_buffer::set_uniform(const std::string& name, const glm::mat4& value)
{
    render_command cmd;
    cmd.type = command_type::set_uniform_matrix4;
    cmd.data = uniform_data{name, value};
    m_commands.push_back(cmd);
}

void render_command_buffer::set_uniform(const std::string& name, const std::vector<glm::mat4>& value)
{
    render_command cmd;
    cmd.type = command_type::set_uniform_matrix4_array;
    cmd.data = uniform_data{name, value};
    m_commands.push_back(cmd);
}

void render_command_buffer::set_uniform(const std::string& name, const glm::vec4& value)
{
    render_command cmd;
    cmd.type = command_type::set_uniform_vec4;
    cmd.data = uniform_data{name, value};
    m_commands.push_back(cmd);
}

void render_command_buffer::set_uniform(const std::string& name, const glm::vec3& value)
{
    render_command cmd;
    cmd.type = command_type::set_uniform_vec3;
    cmd.data = uniform_data{name, value};
    m_commands.push_back(cmd);
}

void render_command_buffer::set_uniform(const std::string& name, float value)
{
    render_command cmd;
    cmd.type = command_type::set_uniform_float;
    cmd.data = uniform_data{name, value};
    m_commands.push_back(cmd);
}

void render_command_buffer::set_uniform(const std::string& name, int value)
{
    render_command cmd;
    cmd.type = command_type::set_uniform_int;
    cmd.data = uniform_data{name, value};
    m_commands.push_back(cmd);
}

void render_command_buffer::use_texture(ref<texture> texture, unsigned int unit)
{
    render_command cmd;
    cmd.type = command_type::use_texture;
    cmd.data = texture_bind_data{ texture, unit };
    m_commands.push_back(cmd);
}

void render_command_buffer::draw_elements(unsigned int primitive_type, unsigned int vao, unsigned int ebo, size_t count)
{
    render_command cmd;
    cmd.type = command_type::draw_elements;
    cmd.data = draw_elements_data{ primitive_type, vao, ebo, count, 1 };
    m_commands.push_back(cmd);
}

void render_command_buffer::draw_elements_instanced(unsigned int primitive_type,
                                                    unsigned int vao, unsigned int ebo,
                                                    size_t count, size_t instances)
{
    if(instances == 0)
    {
        return;
    }

    render_command cmd;
    cmd.type = command_type::draw_elements_instanced;
    cmd.data = draw_elements_data{ primitive_type, vao, ebo, count, instances };
    m_commands.push_back(cmd);
}

void render_command_buffer::draw_copies(unsigned int primitive_type, unsigned int vao, unsigned int ebo,
                                        size_t count, std::vector<glm::mat4> transforms)
{
    if(transforms.empty())
    {
        return;
    }

    render_command cmd;
    cmd.type = command_type::draw_copies;
    cmd.data = draw_copies_data{ primitive_type, vao, ebo, count, std::move(transforms) };
    m_commands.push_back(std::move(cmd));
}

void render_command_buffer::set_depth_mask(bool mask)
{
    render_command cmd;
    cmd.type = command_type::set_depth_mask;
    cmd.data = mask;
    m_commands.push_back(cmd);
}

void render_command_buffer::set_blending(blend_mode mode)
{
    render_command cmd;
    cmd.type = command_type::set_blending;
    cmd.data = mode;
    m_commands.push_back(cmd);
}

void render_command_buffer::clear()
{
    m_commands.clear();
}

const std::vector<render_command>& render_command_buffer::commands() const
{
    return m_commands;
}

} // namespace nle

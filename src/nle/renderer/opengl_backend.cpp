/**
 * @file opengl_backend.cpp
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief OpenGL backend implementation
 * @version 0.1
 * @date 2024-11-07
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "opengl_backend.h"
#include "shader.h"
#include "texture.h"
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

namespace nle
{

opengl_backend::opengl_backend()
{
    invalidate_state_cache();
}

opengl_backend::~opengl_backend()
{
}

void opengl_backend::begin_frame(const render_context& context)
{
    m_context = context;
    m_frame_uniform_programs.clear();
    m_statistics = frame_statistics{};
    invalidate_state_cache();

    // The state the cache now believes in, made true. Blending is left on
    // between frames -- the renderer turns it on at the start and bloom puts
    // it back after its passes -- with whatever function was set last. That
    // was a particle's additive one after any frame with a fire in it, and
    // every solid thing drawn before the next change of blending was added
    // onto what was behind it instead of covering it.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    m_current_depth_mask = 1;
}

void opengl_backend::invalidate_state_cache()
{
    m_current_shader.reset();
    m_current_program = 0;
    m_current_vao = 0;
    m_current_ebo = 0;
    m_current_polygon_mode = -1;
    m_current_depth_mask = -1;
    m_current_blend_mode = blend_mode::none;
}

const opengl_backend::frame_statistics& opengl_backend::statistics() const
{
    return m_statistics;
}

int opengl_backend::location(const std::string& name)
{
    if (m_current_shader)
    {
        return m_current_shader->uniform_location(name);
    }

    if (m_current_program == 0)
    {
        return -1;
    }

    return glGetUniformLocation(m_current_program, name.c_str());
}

void opengl_backend::bind_shader(const ref<class shader>& shader)
{
    if (!shader)
    {
        return;
    }

    if (shader->program() != m_current_program)
    {
        shader->use();
        m_current_program = shader->program();
        m_current_shader = shader;
        ++m_statistics.shader_binds;
    }
    else
    {
        m_current_shader = shader;
        ++m_statistics.redundant_commands_dropped;
    }

    // The first time a program is used this frame it gets the frame constants.
    if (m_frame_uniform_programs.insert(m_current_program).second)
    {
        upload_frame_uniforms(shader);
    }
}

void opengl_backend::upload_frame_uniforms(const ref<class shader>& shader)
{
    if (!shader)
    {
        return;
    }

    auto set_mat4 = [&](const char* name, const glm::mat4& value) {
        int loc = shader->uniform_location(name);
        if (loc != -1)
        {
            glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
            ++m_statistics.uniform_uploads;
        }
    };
    auto set_vec3 = [&](const std::string& name, const glm::vec3& value) {
        int loc = shader->uniform_location(name);
        if (loc != -1)
        {
            glUniform3f(loc, value.x, value.y, value.z);
            ++m_statistics.uniform_uploads;
        }
    };
    auto set_float = [&](const std::string& name, float value) {
        int loc = shader->uniform_location(name);
        if (loc != -1)
        {
            glUniform1f(loc, value);
            ++m_statistics.uniform_uploads;
        }
    };
    auto set_int = [&](const std::string& name, int value) {
        int loc = shader->uniform_location(name);
        if (loc != -1)
        {
            glUniform1i(loc, value);
            ++m_statistics.uniform_uploads;
        }
    };

    set_mat4("u_view", m_context.view);
    set_mat4("u_projection", m_context.projection);
    set_vec3("u_eye_position", m_context.eye_position);
    set_float("u_time", m_context.time);
    set_vec3("u_resolution", glm::vec3(m_context.resolution, 0.0f));

    const auto& dl = m_context.directional_light;
    set_int("u_directional_light.enabled", dl.enabled ? 1 : 0);
    set_vec3("u_directional_light.direction", dl.direction);
    set_vec3("u_directional_light.color", dl.color);
    set_vec3("u_directional_light.ambient", dl.ambient);
    set_vec3("u_directional_light.diffuse", dl.diffuse);
    set_vec3("u_directional_light.specular", dl.specular);

    const int count = static_cast<int>(std::min<size_t>(m_context.point_lights.size(), MAX_POINT_LIGHTS));
    set_int("u_point_light_count", count);

    for (int i = 0; i < count; ++i)
    {
        const auto& pl = m_context.point_lights[static_cast<size_t>(i)];
        const std::string base = "u_point_lights[" + std::to_string(i) + "].";

        set_vec3(base + "position", pl.position);
        set_vec3(base + "color", pl.color);
        set_vec3(base + "ambient", pl.ambient);
        set_vec3(base + "diffuse", pl.diffuse);
        set_vec3(base + "specular", pl.specular);
        set_float(base + "constant", pl.constant);
        set_float(base + "linear", pl.linear);
        set_float(base + "quadratic", pl.quadratic);
        set_float(base + "range", pl.range);
    }

    set_int("u_sky.distance_fog_enabled", m_context.fog.enabled ? 1 : 0);
    set_float("u_sky.distance_fog_near", m_context.fog.near_distance);
    set_float("u_sky.distance_fog_far", m_context.fog.far_distance);
    set_vec3("u_sky.distance_fog_color", m_context.fog.color);

    const fog_bank_data& bank = m_context.fog.bank;

    set_int("u_fog_bank.enabled", bank.enabled ? 1 : 0);
    set_vec3("u_fog_bank.centre", bank.centre);
    set_float("u_fog_bank.radius", bank.radius);
    set_float("u_fog_bank.bottom", bank.bottom);
    set_float("u_fog_bank.top", bank.top);
    set_float("u_fog_bank.density", bank.density);
    set_vec3("u_fog_bank.color", bank.color);
}

void opengl_backend::force_shader(ref<class shader> shader)
{
    m_forced_shader = std::move(shader);
}

void opengl_backend::execute_commands(const render_command_buffer& command_buffer)
{
    for (const auto& cmd : command_buffer.commands())
    {
        ++m_statistics.commands;
        execute_command(cmd);
    }
}

void opengl_backend::execute_command(const render_command& cmd)
{
    switch (cmd.type)
    {
        case command_type::set_polygon_mode:
        {
            const auto& data = std::get<polygon_mode_data>(cmd.data);
            if (static_cast<GLint>(data.mode) == m_current_polygon_mode)
            {
                ++m_statistics.redundant_commands_dropped;
                break;
            }
            glPolygonMode(data.front_and_back, data.mode);
            m_current_polygon_mode = static_cast<GLint>(data.mode);
            break;
        }
        case command_type::use_shader:
        {
            // Whatever the thing asked for, unless one shader is drawing
            // everything just now.
            bind_shader(m_forced_shader ? m_forced_shader : std::get<ref<class shader>>(cmd.data));
            break;
        }
        case command_type::set_uniform_matrix4:
        {
            const auto& uniform = std::get<uniform_data>(cmd.data);
            const auto& matrix = std::get<glm::mat4>(uniform.value);
            int loc = location(uniform.name);
            if (loc != -1)
            {
                glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(matrix));
                ++m_statistics.uniform_uploads;
            }
            break;
        }
        case command_type::set_uniform_matrix4_array:
        {
            const auto& uniform = std::get<uniform_data>(cmd.data);
            const auto& matrices = std::get<std::vector<glm::mat4>>(uniform.value);
            int loc = location(uniform.name);
            if (loc != -1 && !matrices.empty())
            {
                glUniformMatrix4fv(loc, static_cast<GLsizei>(matrices.size()), GL_FALSE,
                                   glm::value_ptr(matrices.front()));
                ++m_statistics.uniform_uploads;
            }
            break;
        }
        case command_type::set_uniform_vec3:
        {
            const auto& uniform = std::get<uniform_data>(cmd.data);
            const auto& vec = std::get<glm::vec3>(uniform.value);
            int loc = location(uniform.name);
            if (loc != -1)
            {
                glUniform3f(loc, vec.x, vec.y, vec.z);
                ++m_statistics.uniform_uploads;
            }
            break;
        }
        case command_type::set_uniform_vec4:
        {
            const auto& uniform = std::get<uniform_data>(cmd.data);
            const auto& vec = std::get<glm::vec4>(uniform.value);
            int loc = location(uniform.name);
            if (loc != -1)
            {
                glUniform4f(loc, vec.x, vec.y, vec.z, vec.w);
                ++m_statistics.uniform_uploads;
            }
            break;
        }
        case command_type::set_uniform_float:
        {
            const auto& uniform = std::get<uniform_data>(cmd.data);
            int loc = location(uniform.name);
            if (loc != -1)
            {
                glUniform1f(loc, std::get<float>(uniform.value));
                ++m_statistics.uniform_uploads;
            }
            break;
        }
        case command_type::set_uniform_int:
        {
            const auto& uniform = std::get<uniform_data>(cmd.data);
            int loc = location(uniform.name);
            if (loc != -1)
            {
                glUniform1i(loc, std::get<int>(uniform.value));
                ++m_statistics.uniform_uploads;
            }
            break;
        }
        case command_type::use_texture:
        {
            const auto& data = std::get<texture_bind_data>(cmd.data);
            if (data.handle)
            {
                data.handle->use(static_cast<uint8_t>(data.unit));

                // keep the matching sampler pointing at the unit we just bound.
                int loc = location("u_texture_" + std::to_string(data.unit));
                if (loc != -1)
                {
                    glUniform1i(loc, static_cast<GLint>(data.unit));
                    ++m_statistics.uniform_uploads;
                }
            }
            break;
        }
        case command_type::draw_elements:
        {
            const auto& data = std::get<draw_elements_data>(cmd.data);

            if (data.vao != m_current_vao)
            {
                glBindVertexArray(data.vao);
                m_current_vao = data.vao;
            }
            if (data.ebo != m_current_ebo)
            {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);
                m_current_ebo = data.ebo;
            }

            glDrawElements(data.primitive_type, static_cast<GLsizei>(data.count), GL_UNSIGNED_INT, 0);
            ++m_statistics.draw_calls;
            break;
        }
        case command_type::draw_elements_instanced:
        {
            const auto& data = std::get<draw_elements_data>(cmd.data);

            if (data.vao != m_current_vao)
            {
                glBindVertexArray(data.vao);
                m_current_vao = data.vao;
            }
            if (data.ebo != m_current_ebo)
            {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);
                m_current_ebo = data.ebo;
            }

            glDrawElementsInstanced(data.primitive_type,
                                    static_cast<GLsizei>(data.count),
                                    GL_UNSIGNED_INT, nullptr,
                                    static_cast<GLsizei>(data.instances));

            ++m_statistics.draw_calls;
            break;
        }

        case command_type::set_blending:
        {
            const blend_mode mode = std::get<blend_mode>(cmd.data);

            if (mode == m_current_blend_mode)
            {
                ++m_statistics.redundant_commands_dropped;
                break;
            }

            switch (mode)
            {
                // Back to how every frame starts rather than off: a solid
                // thing is opaque either way, and a texture with holes in
                // it -- leaves -- has always been drawn through this.
                case blend_mode::none:
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    break;

                case blend_mode::additive:
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                    break;

                case blend_mode::alpha:
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    break;
            }

            m_current_blend_mode = mode;
            break;
        }

        case command_type::set_depth_mask:
        {
            const GLint mask = std::get<bool>(cmd.data) ? 1 : 0;
            if (mask == m_current_depth_mask)
            {
                ++m_statistics.redundant_commands_dropped;
                break;
            }
            glDepthMask(mask ? GL_TRUE : GL_FALSE);
            m_current_depth_mask = mask;
            break;
        }
    }
}

} // namespace nle

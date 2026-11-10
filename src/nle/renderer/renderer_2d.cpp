#include "renderer_2d.h"

#include <cmath>

#include "gl.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

namespace nle
{

namespace
{
    /// Quads per buffer growth step. A HUD is a few dozen quads; this is
    /// plenty, and the buffer grows if a frame ever needs more.
    constexpr size_t INITIAL_QUADS = 512;
}

renderer_2d::renderer_2d(const std::string& vertex_shader_path, const std::string& fragment_shader_path)
    : m_shader(make_ref<class shader>(vertex_shader_path, fragment_shader_path, shader_source::file))
{
    m_shader->load();
    create_white_texture();
    create_buffers();
}

renderer_2d::renderer_2d(ref<class shader> shader)
    : m_shader(shader)
{
    create_white_texture();
    create_buffers();
}

renderer_2d::~renderer_2d()
{
    if(m_ebo != 0) glDeleteBuffers(1, &m_ebo);
    if(m_vbo != 0) glDeleteBuffers(1, &m_vbo);
    if(m_vao != 0) glDeleteVertexArrays(1, &m_vao);
}

void renderer_2d::create_white_texture()
{
    // Untextured quads multiply against this, so one batch covers everything.
    const unsigned char white[4] = { 255, 255, 255, 255 };
    m_white_texture = make_ref<class texture>(white, 1, 1, 4, false);
}

void renderer_2d::create_buffers()
{
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    m_vbo_capacity_quads = INITIAL_QUADS;

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_vbo_capacity_quads * 4 * sizeof(vertex_2d)),
                 nullptr, GL_DYNAMIC_DRAW);

    // Quad indices never change, so they are uploaded once for the whole
    // capacity rather than rebuilt per frame.
    std::vector<uint32_t> indices(m_vbo_capacity_quads * 6);
    for(size_t quad = 0; quad < m_vbo_capacity_quads; ++quad)
    {
        const auto base = static_cast<uint32_t>(quad * 4);
        const size_t out = quad * 6;

        indices[out + 0] = base + 0;
        indices[out + 1] = base + 1;
        indices[out + 2] = base + 2;
        indices[out + 3] = base + 0;
        indices[out + 4] = base + 2;
        indices[out + 5] = base + 3;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                 indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_2d), (void*)offsetof(vertex_2d, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_2d), (void*)offsetof(vertex_2d, uv));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(vertex_2d), (void*)offsetof(vertex_2d, color));

    glBindVertexArray(0);
}

void renderer_2d::begin(const glm::vec2& resolution)
{
    m_resolution = glm::max(resolution, glm::vec2(1.0f));
    m_in_frame = true;
    m_frame_draw_calls = 0;
    m_vertices.clear();
    m_current_texture = m_white_texture;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader->use();

    // Top-left origin with y growing downwards, which is how UI is laid out.
    const glm::mat4 projection = glm::ortho(0.0f, m_resolution.x, m_resolution.y, 0.0f, -1.0f, 1.0f);

    const int location = m_shader->uniform_location("u_projection");
    if(location != -1)
    {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(projection));
    }

    const int sampler = m_shader->uniform_location("u_texture_0");
    if(sampler != -1)
    {
        glUniform1i(sampler, 0);
    }
}

void renderer_2d::end()
{
    flush();

    glEnable(GL_DEPTH_TEST);

    m_in_frame = false;
    m_draw_calls = m_frame_draw_calls;
}

void renderer_2d::use_texture(const ref<class texture>& texture)
{
    const auto& wanted = texture ? texture : m_white_texture;

    if(wanted != m_current_texture)
    {
        flush();
        m_current_texture = wanted;
    }
}

void renderer_2d::push_quad(const glm::vec2& position, const glm::vec2& size,
                            const glm::vec2& uv_min, const glm::vec2& uv_max,
                            const glm::vec4& color)
{
    if(!m_in_frame || size.x <= 0.0f || size.y <= 0.0f || color.a <= 0.0f)
    {
        return;
    }

    if(m_vertices.size() / 4 >= m_vbo_capacity_quads)
    {
        flush();
    }

    const glm::vec2 top_left = position;
    const glm::vec2 bottom_right = position + size;

    m_vertices.push_back({ { top_left.x,     top_left.y     }, { uv_min.x, uv_min.y }, color });
    m_vertices.push_back({ { bottom_right.x, top_left.y     }, { uv_max.x, uv_min.y }, color });
    m_vertices.push_back({ { bottom_right.x, bottom_right.y }, { uv_max.x, uv_max.y }, color });
    m_vertices.push_back({ { top_left.x,     bottom_right.y }, { uv_min.x, uv_max.y }, color });
}

void renderer_2d::flush()
{
    if(m_vertices.empty())
    {
        return;
    }

    const size_t quads = m_vertices.size() / 4;

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(m_vertices.size() * sizeof(vertex_2d)),
                    m_vertices.data());

    if(m_current_texture)
    {
        m_current_texture->use(0);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(quads * 6), GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);

    ++m_frame_draw_calls;
    m_vertices.clear();
}

void renderer_2d::draw_rect(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
{
    use_texture(m_white_texture);
    push_quad(position, size, glm::vec2(0.0f), glm::vec2(1.0f), color);
}

namespace
{
    /// How far in from the side a corner of @p radius is, @p from_edge
    /// pixels in from the top or bottom -- at the middle of that pixel row.
    float corner_inset(float radius, float from_edge)
    {
        const float dy = radius - (from_edge + 0.5f);

        return radius - std::sqrt(std::max(0.0f, radius * radius - dy * dy));
    }
}

void renderer_2d::draw_rounded_rect(const glm::vec2& position, const glm::vec2& size,
                                    const glm::vec4& color, float radius)
{
    radius = std::floor(std::clamp(radius, 0.0f, std::min(size.x, size.y) * 0.5f));

    if(radius < 1.0f)
    {
        draw_rect(position, size, color);
        return;
    }

    // The middle, between the two curved bands.
    draw_rect({ position.x, position.y + radius }, { size.x, size.y - 2.0f * radius }, color);

    // A row at a time where the corners curve, top and bottom together.
    for(int row = 0; row < static_cast<int>(radius); ++row)
    {
        const float in = corner_inset(radius, static_cast<float>(row));
        const float wide = size.x - 2.0f * in;

        draw_rect({ position.x + in, position.y + static_cast<float>(row) }, { wide, 1.0f }, color);
        draw_rect({ position.x + in, position.y + size.y - 1.0f - static_cast<float>(row) },
                  { wide, 1.0f }, color);
    }
}

void renderer_2d::draw_rounded_outline(const glm::vec2& position, const glm::vec2& size,
                                       float thickness, const glm::vec4& color, float radius)
{
    radius = std::floor(std::clamp(radius, 0.0f, std::min(size.x, size.y) * 0.5f));

    if(radius < 1.0f || thickness <= 0.0f)
    {
        draw_outline(position, size, thickness, color);
        return;
    }

    thickness = std::min(thickness, radius);

    // The straight sides, between the corners.
    draw_rect({ position.x + radius, position.y }, { size.x - 2.0f * radius, thickness }, color);
    draw_rect({ position.x + radius, position.y + size.y - thickness },
              { size.x - 2.0f * radius, thickness }, color);
    draw_rect({ position.x, position.y + radius }, { thickness, size.y - 2.0f * radius }, color);
    draw_rect({ position.x + size.x - thickness, position.y + radius },
              { thickness, size.y - 2.0f * radius }, color);

    // The corners, a row at a time: from the outer curve in to the inner
    // one, which is the same curve a thickness further in.
    const float inner_radius = radius - thickness;

    for(int row = 0; row < static_cast<int>(radius); ++row)
    {
        const float outer = corner_inset(radius, static_cast<float>(row));

        // Past the inner curve's rows the band is solid out to the straight
        // part; within them it stops where the inner curve starts.
        const float inner = static_cast<float>(row) < thickness
            ? radius
            : thickness + corner_inset(inner_radius, static_cast<float>(row) - thickness);

        const float span = std::max(0.0f, std::min(inner, radius) - outer);

        if(span <= 0.0f)
        {
            continue;
        }

        const float top = position.y + static_cast<float>(row);
        const float bottom = position.y + size.y - 1.0f - static_cast<float>(row);

        for(const float y : { top, bottom })
        {
            draw_rect({ position.x + outer, y }, { span, 1.0f }, color);
            draw_rect({ position.x + size.x - outer - span, y }, { span, 1.0f }, color);
        }
    }
}

void renderer_2d::draw_outline(const glm::vec2& position, const glm::vec2& size, float thickness,
                               const glm::vec4& color)
{
    if(thickness <= 0.0f)
    {
        return;
    }

    thickness = std::min(thickness, std::min(size.x, size.y) * 0.5f);

    draw_rect(position, { size.x, thickness }, color);
    draw_rect({ position.x, position.y + size.y - thickness }, { size.x, thickness }, color);
    draw_rect({ position.x, position.y + thickness }, { thickness, size.y - 2.0f * thickness }, color);
    draw_rect({ position.x + size.x - thickness, position.y + thickness },
              { thickness, size.y - 2.0f * thickness }, color);
}

void renderer_2d::draw_texture(const glm::vec2& position, const glm::vec2& size, ref<class texture> texture,
                               const glm::vec4& tint, const glm::vec2& uv_min, const glm::vec2& uv_max)
{
    use_texture(texture);
    push_quad(position, size, uv_min, uv_max, tint);
}

void renderer_2d::draw_bar(const glm::vec2& position, const glm::vec2& size, float fill,
                           const glm::vec4& fill_color, const glm::vec4& background_color,
                           const glm::vec4& border_color, float border)
{
    fill = std::clamp(fill, 0.0f, 1.0f);

    draw_rect(position, size, background_color);

    const glm::vec2 inner_position = position + glm::vec2(border);
    const glm::vec2 inner_size = size - glm::vec2(border * 2.0f);

    if(inner_size.x > 0.0f && inner_size.y > 0.0f)
    {
        draw_rect(inner_position, { inner_size.x * fill, inner_size.y }, fill_color);
    }

    if(border > 0.0f)
    {
        draw_outline(position, size, border, border_color);
    }
}

void renderer_2d::draw_cooldown_overlay(const glm::vec2& position, const glm::vec2& size, float remaining,
                                        const glm::vec4& color)
{
    remaining = std::clamp(remaining, 0.0f, 1.0f);

    if(remaining <= 0.0f)
    {
        return;
    }

    // Shade from the top down, so the slot "empties" as the cooldown expires.
    draw_rect(position, { size.x, size.y * remaining }, color);
}

glm::vec2 renderer_2d::resolution() const
{
    return m_resolution;
}

size_t renderer_2d::draw_calls() const
{
    return m_draw_calls;
}

} // namespace nle

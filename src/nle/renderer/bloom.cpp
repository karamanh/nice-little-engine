#include "bloom.h"

#include "shader.h"
#include "../core/utils.h"

#include <algorithm>

namespace nle
{

namespace
{

/**
 * A fullscreen triangle, worked out from the vertex index.
 *
 * One triangle rather than two, so there is no seam down the diagonal where
 * neighbouring pixels are rasterised twice, and no vertex buffer at all.
 */
const char* POST_VERTEX = R"(#version 330

out vec2 io_uv;

void main()
{
    vec2 corner = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);

    io_uv = corner;
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
)";

/// Keeps what is brighter than the threshold and throws the rest away.
const char* BRIGHT_FRAGMENT = R"(#version 330

in vec2 io_uv;
out vec4 io_color;

uniform sampler2D u_source;
uniform float u_threshold;
uniform float u_softness;

void main()
{
    vec3 color = texture(u_source, io_uv).rgb;

    // Perceived brightness, so a saturated red glows like a grey of the same
    // apparent lightness rather than like a white.
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));

    // Faded in rather than switched on: a hard threshold makes edges crawl as
    // the camera moves and pixels cross it.
    float amount = smoothstep(u_threshold, u_threshold + max(u_softness, 0.0001), brightness);

    io_color = vec4(color * amount, 1.0);
}
)";

/// One axis of a gaussian blur. Run twice with u_direction swapped.
const char* BLUR_FRAGMENT = R"(#version 330

in vec2 io_uv;
out vec4 io_color;

uniform sampler2D u_source;

/// One texel along the axis being blurred.
uniform vec2 u_direction;

void main()
{
    float weight[5] = float[](0.2270270, 0.1945946, 0.1216216, 0.0540540, 0.0162162);

    vec3 total = texture(u_source, io_uv).rgb * weight[0];

    for(int i = 1; i < 5; ++i)
    {
        vec2 step = u_direction * float(i);

        total += texture(u_source, io_uv + step).rgb * weight[i];
        total += texture(u_source, io_uv - step).rgb * weight[i];
    }

    io_color = vec4(total, 1.0);
}
)";

/// The scene with the glow added back on top.
const char* COMPOSITE_FRAGMENT = R"(#version 330

in vec2 io_uv;
out vec4 io_color;

uniform sampler2D u_scene;
uniform sampler2D u_glow;
uniform float u_intensity;

void main()
{
    vec3 scene = texture(u_scene, io_uv).rgb;
    vec3 glow = texture(u_glow, io_uv).rgb;

    io_color = vec4(scene + glow * u_intensity, 1.0);
}
)";

void set_uniform(const ref<shader>& program, const char* name, float value)
{
    glUniform1f(program->uniform_location(name), value);
}

void set_uniform(const ref<shader>& program, const char* name, const glm::vec2& value)
{
    glUniform2f(program->uniform_location(name), value.x, value.y);
}

void set_sampler(const ref<shader>& program, const char* name, GLuint texture, int unit)
{
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(program->uniform_location(name), unit);
}

} // namespace

bloom::bloom()
{
}

bloom::~bloom()
{
    destroy(m_scene);
    destroy(m_blur[0]);
    destroy(m_blur[1]);

    if(m_empty_vertex_array != 0)
    {
        glDeleteVertexArrays(1, &m_empty_vertex_array);
    }
}

void bloom::set_enabled(bool enabled) { m_enabled = enabled; }
bool bloom::enabled() const { return m_enabled; }

void bloom::set_threshold(float threshold) { m_threshold = std::max(threshold, 0.0f); }
float bloom::threshold() const { return m_threshold; }

void bloom::set_softness(float softness) { m_softness = std::max(softness, 0.0f); }
float bloom::softness() const { return m_softness; }

void bloom::set_intensity(float intensity) { m_intensity = std::max(intensity, 0.0f); }
float bloom::intensity() const { return m_intensity; }

void bloom::set_passes(int passes) { m_passes = std::clamp(passes, 1, 16); }
int bloom::passes() const { return m_passes; }

bool bloom::ensure_shaders()
{
    if(m_bright && m_blur_shader && m_composite)
    {
        return true;
    }

    m_bright = make_ref<shader>(POST_VERTEX, BRIGHT_FRAGMENT, shader_source::memory);
    m_blur_shader = make_ref<shader>(POST_VERTEX, BLUR_FRAGMENT, shader_source::memory);
    m_composite = make_ref<shader>(POST_VERTEX, COMPOSITE_FRAGMENT, shader_source::memory);

    m_bright->load();
    m_blur_shader->load();
    m_composite->load();

    if(m_empty_vertex_array == 0)
    {
        glGenVertexArrays(1, &m_empty_vertex_array);
    }

    return true;
}

void bloom::destroy(target& t)
{
    if(t.color != 0) { glDeleteTextures(1, &t.color); }
    if(t.depth != 0) { glDeleteRenderbuffers(1, &t.depth); }
    if(t.framebuffer != 0) { glDeleteFramebuffers(1, &t.framebuffer); }

    t = {};
}

bool bloom::make_target(target& t, int width, int height, bool with_depth)
{
    destroy(t);

    t.width = std::max(width, 1);
    t.height = std::max(height, 1);

    glGenFramebuffers(1, &t.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, t.framebuffer);

    glGenTextures(1, &t.color);
    glBindTexture(GL_TEXTURE_2D, t.color);

    // Floating point, which is the whole reason this exists: a light at seven
    // times intensity has to survive being written down before anything can
    // notice it is brighter than white.
    // Half floats, which a phone can draw into where it can draw into floats
    // at all; if it cannot, the target does not complete and there is no
    // bloom.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, t.width, t.height, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Clamped, so the blur does not wrap the far edge of the screen into the
    // near one.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.color, 0);

    if(with_depth)
    {
        glGenRenderbuffers(1, &t.depth);
        glBindRenderbuffer(GL_RENDERBUFFER, t.depth);
        // With a stencil, which outlines are masked with.
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, t.width, t.height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, t.depth);
    }

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if(status != GL_FRAMEBUFFER_COMPLETE)
    {
        utils::prerror("bloom: could not make a", t.width, "x", t.height, "target");
        destroy(t);
        return false;
    }

    return true;
}

bool bloom::ensure_targets(int width, int height)
{
    if(m_scene.width == width && m_scene.height == height && m_scene.framebuffer != 0)
    {
        return true;
    }

    // Half resolution for the glow. It is a wide, soft thing, so the detail
    // is not missed and it costs a quarter as much.
    const int half_width = std::max(width / 2, 1);
    const int half_height = std::max(height / 2, 1);

    if(!make_target(m_scene, width, height, true)
    || !make_target(m_blur[0], half_width, half_height, false)
    || !make_target(m_blur[1], half_width, half_height, false))
    {
        return false;
    }

    return true;
}

bool bloom::begin(int width, int height)
{
    m_active = false;

    if(!m_enabled || m_broken || width <= 0 || height <= 0)
    {
        return false;
    }

    if(!ensure_shaders() || !ensure_targets(width, height))
    {
        // Said once. A machine that cannot make a float target now will not
        // be able to next frame either, and saying so sixty times a second
        // helps nobody.
        utils::prerror("bloom: giving up and drawing straight to the window");
        m_broken = true;
        return false;
    }

    m_width = width;
    m_height = height;
    m_active = true;

    glBindFramebuffer(GL_FRAMEBUFFER, m_scene.framebuffer);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    return true;
}

void bloom::draw_fullscreen()
{
    glBindVertexArray(m_empty_vertex_array);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void bloom::end()
{
    if(!m_active)
    {
        return;
    }

    m_active = false;

    // Post passes are flat image work: nothing to depth test against, and
    // nothing to blend with.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    const int half_width = m_blur[0].width;
    const int half_height = m_blur[0].height;

    // ---- take out what is bright ----------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, m_blur[0].framebuffer);
    glViewport(0, 0, half_width, half_height);

    m_bright->use();
    set_uniform(m_bright, "u_threshold", m_threshold);
    set_uniform(m_bright, "u_softness", m_softness);
    set_sampler(m_bright, "u_source", m_scene.color, 0);
    draw_fullscreen();

    // ---- blur it ---------------------------------------------------------
    m_blur_shader->use();

    const glm::vec2 texel(1.0f / static_cast<float>(half_width),
                          1.0f / static_cast<float>(half_height));

    int source = 0;

    for(int pass = 0; pass < m_passes; ++pass)
    {
        // Separable: a horizontal pass then a vertical one gives the same
        // result as a square kernel for a fraction of the taps.
        for(int axis = 0; axis < 2; ++axis)
        {
            const int destination = 1 - source;

            glBindFramebuffer(GL_FRAMEBUFFER, m_blur[destination].framebuffer);
            glViewport(0, 0, half_width, half_height);

            set_uniform(m_blur_shader, "u_direction",
                        axis == 0 ? glm::vec2(texel.x, 0.0f) : glm::vec2(0.0f, texel.y));
            set_sampler(m_blur_shader, "u_source", m_blur[source].color, 0);

            draw_fullscreen();

            source = destination;
        }
    }

    // ---- put it back on the scene ---------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);

    m_composite->use();
    set_uniform(m_composite, "u_intensity", m_intensity);
    set_sampler(m_composite, "u_scene", m_scene.color, 0);
    set_sampler(m_composite, "u_glow", m_blur[source].color, 1);

    draw_fullscreen();

    // Back to how the rest of the engine expects to find things.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
}

} // namespace nle

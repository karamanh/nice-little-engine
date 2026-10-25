#include "particles.h"

#include "render_command.h"
#include "shader.h"

#include <GL/glew.h>

#include <algorithm>

#include <glm/gtc/type_ptr.hpp>

namespace nle
{

namespace
{

/**
 * @brief The whole simulation, as a function of an index and the clock.
 *
 * Every particle works out where it is from gl_InstanceID and u_time and
 * nothing else. There is no buffer of positions to update, no ping-pong, no
 * read-back -- which is what makes an emitter cost one draw call and no CPU
 * time, and is also why a particle cannot be told anything: it has no memory
 * to tell it into.
 *
 * The randomness is a hash of the index, so it is the same every frame and
 * the same on every machine. A particle that jittered between frames because
 * its "random" direction was re-rolled would be a firefly, not a flame.
 */
const char* PARTICLE_VERTEX = R"(
#version 330 core

layout (location = 0) in vec2 a_corner;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

uniform float u_time;
uniform float u_lifetime;
uniform float u_speed;
uniform float u_size;
uniform float u_radius;
uniform vec3  u_span;
uniform int   u_count;
uniform int   u_style;

out float v_age;
out vec2  v_corner;
out float v_tone;

/// Three uncorrelated numbers in [0,1) from one index. Cheap and good enough
/// that no two particles of a few hundred visibly share a path.
vec3 hash3(float n)
{
    return fract(sin(vec3(n, n + 17.13, n + 41.71)) * vec3(43758.5453, 22578.1459, 19642.3491));
}

void main()
{
    float index = float(gl_InstanceID);
    vec3 r = hash3(index);

    // Spread the births evenly through the cycle, so the stream is steady
    // rather than pulsing all together.
    float phase = float(gl_InstanceID) / float(max(u_count, 1));
    float age = fract(u_time / u_lifetime + phase);

    float seconds = age * u_lifetime;

    // A direction that does not change for the life of this particle.
    float angle = r.x * 6.2831853;
    vec2 around = vec2(cos(angle), sin(angle));

    vec3 local = vec3(0.0);
    float size = u_size;

    if(u_style == 0)
    {
        // Flame: up, wavering, narrowing to nothing.
        float wobble = sin(u_time * 3.1 + index) * 0.12 * age;

        local = vec3(around.x * u_radius * (0.35 + r.y * 0.65) + wobble,
                     seconds * u_speed,
                     around.y * u_radius * (0.35 + r.y * 0.65));

        // Wide at the base, pinched at the top, which is what reads as fire
        // rather than as a column of dots.
        size *= (0.45 + r.z * 0.55) * (1.0 - age * 0.75);
    }
    else if(u_style == 1)
    {
        // Cloud: outward and gently down, keeping its size.
        //
        // Born anywhere inside the radius rather than exactly on it. Born on
        // it, a large emitter drew a ring with nothing in the middle -- fine
        // for a puff of frost on a staff, where the radius is a few
        // centimetres and nobody can tell, and obviously wrong the moment
        // one is asked to fill a blizzard eight units across.
        float out_by = u_radius * (0.3 + r.y * 0.7) + seconds * u_speed * 0.5;

        local = vec3(around.x * out_by,
                     (r.y - 0.65) * seconds * u_speed * 0.6,
                     around.y * out_by);

        size *= (0.6 + r.z * 0.8) * (1.0 - age * 0.3);
    }
    else if(u_style == 3)
    {
        // Falling: born high and spread across the whole circle, coming
        // down with a little sway. The square root is what spreads them
        // evenly over a disc -- without it they bunch in the middle, since
        // there is far more circle near the rim than near the centre.
        float spread = u_radius * sqrt(r.y);
        float sway = sin(seconds * 2.3 + r.z * 6.283) * u_radius * 0.06;

        local = vec3(around.x * spread + sway,
                     u_radius * 1.5 - seconds * u_speed,
                     around.y * spread);

        size *= (0.5 + r.z * 0.6);
    }
    else if(u_style == 4)
    {
        // Swirl: out from the middle and turning, arriving at the rim as it
        // dies. An explosion is a front that travels, so every particle
        // starts at the centre rather than somewhere inside the circle.
        float out_by = u_radius * age;
        float turn = age * 2.4 + r.z * 6.283;

        vec2 spun = vec2(around.x * cos(turn) - around.y * sin(turn),
                         around.x * sin(turn) + around.y * cos(turn));

        local = vec3(spun.x * out_by,
                     (0.15 + r.y * 0.85) * u_radius * 0.35 * age,
                     spun.y * out_by);

        size *= (0.5 + r.z * 0.7) * (1.0 - age * 0.6);
    }
    else if(u_style == 5)
    {
        // Bolt: a narrow column falling fast. Almost no spread, so it reads
        // as one thing coming down rather than as weather.
        float spread = u_radius * 0.07 * (r.y - 0.5);

        local = vec3(around.x * u_radius * 0.05 + spread,
                     u_radius * 1.6 - seconds * u_speed,
                     around.y * u_radius * 0.05);

        // Brightest as it arrives, which is where the eye ends up.
        size *= (0.4 + r.z * 0.5) * (0.35 + age * 0.65);
    }
    else if(u_style == 6)
    {
        // Arc: the whole of it strikes at once, so its age is the strike's
        // rather than each particle's, and which strike this is decides its
        // shape and whether it happens at all.
        age = fract(u_time / u_lifetime);
        float strike = floor(u_time / u_lifetime);

        float t = fract(r.z * 7.31 + r.y);

        // Six crooked legs from a radius and a half up to the emitter. The
        // ends wander less than the middle, so it lands on what it hits.
        float legs = 6.0;
        float k = floor(t * legs);
        float along = fract(t * legs);

        vec2 a = (hash3(strike * 13.0 + k).xy - 0.5) * u_radius * 0.9 * min(k, legs - k) / 3.0;
        vec2 b = (hash3(strike * 13.0 + k + 1.0).xy - 0.5) * u_radius * 0.9
               * min(k + 1.0, legs - k - 1.0) / 3.0;

        vec2 sideways = mix(a, b, along);
        float height = u_radius * 1.5 * (1.0 - (k + along) / legs);

        local = vec3(sideways.x, height, sideways.y);

        // Thin, and gone between strikes that do not happen.
        float strikes = step(hash3(strike * 3.7).x, u_speed);
        size *= (0.55 + r.x * 0.45) * strikes;
    }
    else if(u_style == 7)
    {
        // Orbit: evenly round a flat ring, turning, each bobbing a little out
        // of step with the next. Never born and never dying -- its age is a
        // twinkle, kept young so the stars stay bright.
        float slot = index / float(max(u_count, 1)) * 6.2831853;
        float turn = u_time * u_speed + slot;
        float bob = sin(u_time * 3.0 + slot * 2.0) * u_radius * 0.15;

        local = vec3(cos(turn) * u_radius, bob, sin(turn) * u_radius);

        age = 0.1 + 0.1 * sin(u_time * 7.0 + index * 2.3);
        size *= 0.85 + 0.25 * sin(u_time * 6.0 + index * 1.7);
    }
    else if(u_style == 8)
    {
        // Vortex: round the axis and up it at once. Narrow and quick at the
        // foot, wider as it climbs, the way a column of wind opens out --
        // and each particle a little in or out of the line, so it is a
        // funnel rather than a coil of wire.
        float ring = u_radius * (0.45 + 0.55 * age) * (0.8 + r.y * 0.4);
        float turn = angle + u_time * u_speed + age * 3.5;

        local = vec3(cos(turn) * ring,
                     age * u_radius * 1.1 + r.z * 0.15,
                     sin(turn) * ring);

        size *= (0.5 + r.z * 0.6) * (1.0 - age * 0.45);
    }
    else if(u_style == 9)
    {
        // Mist: born anywhere round a short cylinder about the emitter's up,
        // turning slowly round it and drifting up a little, swelling as it
        // goes -- a cold breath hanging about the thing, not a stream.
        float turn = angle + seconds * u_speed;
        float ring = u_radius * (0.8 + r.y * 0.4);

        local = vec3(cos(turn) * ring,
                     (r.z - 0.5) * u_radius * 2.2 + seconds * u_radius * 0.12,
                     sin(turn) * ring);

        size *= (0.7 + r.z * 0.6) * (0.8 + age * 0.5);
    }
    else
    {
        // Spark: straight out, fast, gone. Born inside the radius for the
        // same reason a cloud is -- a ring of sparks around an empty middle
        // is not a field of lightning.
        float reach = seconds * u_speed * 2.2;
        float up = (r.y - 0.4) * 1.6;

        local = normalize(vec3(around.x, up, around.y))
              * (u_radius * (0.25 + r.z * 0.75) + reach);

        size *= (0.25 + r.z * 0.4) * (1.0 - age) * (1.0 - age);
    }

    vec3 centre = (u_model * vec4(local, 1.0)).xyz;

    // Born along a line rather than at a point, for a thing that glows down
    // its length. Added after the model transform because the span is given
    // in world terms: only the birthplace moves, so a flame spread along a
    // blade still rises rather than running along it.
    centre += u_span * fract(r.z + phase);

    // Turned to face the camera by building the quad in view space, which is
    // the cheapest billboard there is: no per-particle matrix, no cross
    // products, just the two axes the view already has.
    vec3 right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 up    = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);

    vec3 world = centre + (right * a_corner.x + up * a_corner.y) * size;

    gl_Position = u_projection * u_view * vec4(world, 1.0);

    v_age = age;
    v_corner = a_corner;
    v_tone = r.y;
}
)";

/**
 * @brief A soft round dot, faded at the edge and over its life.
 *
 * No texture: a circle with a soft edge is two instructions and always the
 * right resolution, and an emitter that needs no files is an emitter that
 * cannot be missing one.
 */
const char* PARTICLE_FRAGMENT = R"(
#version 330 core

in float v_age;
in vec2  v_corner;
in float v_tone;

uniform vec3  u_born;
uniform vec3  u_dying;
uniform float u_intensity;
uniform int   u_style;
uniform int   u_dark;

out vec4 frag_color;

void main()
{
    // Round, and soft at the rim. Squared so the falloff is gentler in the
    // middle than at the edge.
    float r = length(v_corner) * 2.0;
    float fall = clamp(1.0 - r, 0.0, 1.0);
    fall *= fall;

    // A star for one going round a dazed head: four points, the curve whose
    // arms run out along the axes, with a little glow left at its heart.
    if(u_style == 7)
    {
        vec2 c = abs(v_corner * 2.0);
        float star = clamp(1.0 - (pow(c.x, 0.55) + pow(c.y, 0.55)), 0.0, 1.0);
        fall = max(star * 1.6, fall * 0.35);
    }

    if(fall <= 0.001)
    {
        discard;
    }

    // Bright at birth, guttering out. The fade is on alpha rather than on
    // colour, because these are drawn additively and a colour fading to black
    // is already invisible.
    float life = 1.0 - v_age;

    // Mist comes and goes softly at both ends: nothing in it starts bright.
    if(u_style == 9)
    {
        life = sin(v_age * 3.14159265);
    }

    vec3 colour = mix(u_born, u_dying, v_age);

    // Mist is many wisps, not one colour: each its own shade, lighter or
    // darker, some leaning blue and some purple, so it reads as cloud with
    // depth to it rather than a flat smear.
    if(u_style == 9)
    {
        vec3 lean = mix(vec3(0.85, 0.95, 1.25), vec3(1.15, 0.88, 1.2), v_tone);
        colour *= lean * mix(0.6, 1.45, fract(v_tone * 7.31));
    }

    // Dark ones are laid over, as solid as their intensity says, rather than
    // added: added, a dark colour is nothing at all.
    if(u_dark == 1)
    {
        frag_color = vec4(colour, fall * life * u_intensity);
        return;
    }

    frag_color = vec4(colour * u_intensity, fall * life * life);
}
)";

/// One quad, shared by every emitter in the process. Two triangles is all the
/// geometry a particle system needs; everything else is the shader.
GLuint g_vao = 0;
GLuint g_vbo = 0;
GLuint g_ebo = 0;

ref<::nle::shader> g_shader;

} // namespace

void particle_emitter::ensure_resources()
{
    if(g_vao != 0)
    {
        return;
    }

    const float corners[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f,
    };

    const unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    glGenVertexArrays(1, &g_vao);
    glBindVertexArray(g_vao);

    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    glGenBuffers(1, &g_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    g_shader = make_ref<::nle::shader>(PARTICLE_VERTEX, PARTICLE_FRAGMENT,
                                      shader_source::memory);

    // The constructor only keeps the source; nothing is compiled or linked
    // until it is asked for. A shader that was never loaded has program zero
    // and draws nothing at all, perfectly silently.
    g_shader->load();
}

particle_emitter::particle_emitter()
{
}

particle_emitter::~particle_emitter()
{
}

void particle_emitter::set_count(size_t count)
{
    m_count = count;
}

size_t particle_emitter::count() const
{
    return m_count;
}

void particle_emitter::set_style(particle_style style)
{
    m_style = style;
}

particle_style particle_emitter::style() const
{
    return m_style;
}

void particle_emitter::set_colours(const glm::vec3& born, const glm::vec3& dying)
{
    m_born = born;
    m_dying = dying;
}

void particle_emitter::set_lifetime(float seconds)
{
    m_lifetime = seconds > 0.0f ? seconds : 0.001f;
}

void particle_emitter::set_speed(float units_per_second)
{
    m_speed = units_per_second;
}

void particle_emitter::set_size(float size)
{
    m_size = size;
}

float particle_emitter::size() const
{
    return m_size;
}

void particle_emitter::set_radius(float radius)
{
    m_radius = radius;
}

void particle_emitter::set_span(const glm::vec3& span)
{
    m_span = span;
}

const glm::vec3& particle_emitter::span() const
{
    return m_span;
}

void particle_emitter::set_intensity(float intensity)
{
    m_intensity = intensity;
}

float particle_emitter::intensity() const
{
    return m_intensity;
}

void particle_emitter::set_thin_with_distance(bool thin)
{
    m_thin_with_distance = thin;
}

void particle_emitter::set_dark(bool dark)
{
    m_dark = dark;
}

void particle_emitter::render(render_command_buffer& command_buffer,
                              const render_context& context)
{
    if(m_count == 0 || !visible())
    {
        return;
    }

    ensure_resources();

    m_clock += context.delta_time;

    // All of them close by, thinning out from fifteen metres to a fifth of
    // them at sixty, and none past that: a crowd far off costs nothing.
    size_t drawn = m_count;

    if(m_thin_with_distance)
    {
        const float away = glm::length(glm::vec3(transform_matrix()[3]) - context.eye_position);

        if(away > 60.0f)
        {
            return;
        }

        const float share = 1.0f - 0.8f * std::clamp((away - 15.0f) / 45.0f, 0.0f, 1.0f);
        drawn = std::max<size_t>(1, static_cast<size_t>(static_cast<float>(m_count) * share));
    }

    command_buffer.use_shader(g_shader);

    command_buffer.set_uniform("u_model", transform_matrix());
    command_buffer.set_uniform("u_view", context.view);
    command_buffer.set_uniform("u_projection", context.projection);

    command_buffer.set_uniform("u_time", m_clock);
    command_buffer.set_uniform("u_lifetime", m_lifetime);
    command_buffer.set_uniform("u_speed", m_speed);
    command_buffer.set_uniform("u_size", m_size);
    command_buffer.set_uniform("u_radius", m_radius);
    command_buffer.set_uniform("u_span", m_span);
    command_buffer.set_uniform("u_count", static_cast<int>(drawn));
    command_buffer.set_uniform("u_style", static_cast<int>(m_style));

    command_buffer.set_uniform("u_born", m_born);
    command_buffer.set_uniform("u_dying", m_dying);
    command_buffer.set_uniform("u_intensity", m_intensity);
    command_buffer.set_uniform("u_dark", m_dark ? 1 : 0);

    // Added to what is already there, and writing no depth: overlapping
    // particles should brighten each other rather than the nearest one
    // hiding the rest, and a cloud of them should not stop the world behind
    // it being drawn.
    command_buffer.set_blending(m_dark ? blend_mode::alpha : blend_mode::additive);
    command_buffer.set_depth_mask(false);

    command_buffer.draw_elements_instanced(GL_TRIANGLES, g_vao, g_ebo, 6, drawn);

    // Put back, or everything drawn after this is a glow too.
    command_buffer.set_depth_mask(true);
    command_buffer.set_blending(blend_mode::none);
}

} // namespace nle

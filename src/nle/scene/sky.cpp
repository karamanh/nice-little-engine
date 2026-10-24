#include "sky.h"

#include "../renderer/shader.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

#include <cmath>

namespace nle
{

namespace
{

const char* SKY_VERTEX = R"(
#version 330 core

layout (location = 0) in vec3 position;

uniform mat4 u_view_rotation;
uniform mat4 u_projection;

out vec3 v_direction;

void main()
{
    v_direction = position;

    // Drawn first and writing no depth, so everything drawn after it is in
    // front of it wherever it happens to sit.
    gl_Position = u_projection * u_view_rotation * vec4(position, 1.0);
}
)";

const char* SKY_FRAGMENT = R"(
#version 330 core

in vec3 v_direction;
out vec4 frag_color;

uniform vec3 u_zenith;
uniform vec3 u_horizon;
uniform vec3 u_ground;
uniform float u_horizon_falloff;

uniform vec3 u_sun_direction;
uniform vec3 u_sun_color;
uniform float u_sun_cos_outer;
uniform float u_sun_cos_inner;
uniform float u_sun_glow;

uniform float u_stars;
uniform float u_time;

// The fog bank, as default_frag.glsl has it: cloud lying in one place is
// seen against the sky as well as against whatever is behind it.
uniform vec3 u_eye_position;
uniform int u_fog_bank_enabled;
uniform vec3 u_fog_bank_centre;
uniform float u_fog_bank_radius;
uniform float u_fog_bank_bottom;
uniform float u_fog_bank_top;
uniform float u_fog_bank_density;
uniform vec3 u_fog_bank_color;

float fog_hash(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float fog_noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    return mix(mix(fog_hash(i), fog_hash(i + vec2(1.0, 0.0)), f.x),
               mix(fog_hash(i + vec2(0.0, 1.0)), fog_hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

// The same sum as default_frag.glsl's fog_bank_cover, along a line out to
// the far side of the bank rather than to a surface.
float fog_bank_cover(vec3 dir)
{
    vec3 from = u_eye_position;
    float reach = 2.0 * u_fog_bank_radius + length(from.xz - u_fog_bank_centre.xz);
    vec3 ray = dir * reach;
    float floor_y = u_fog_bank_centre.y + u_fog_bank_bottom;
    float ceiling = u_fog_bank_centre.y + u_fog_bank_top;

    float t0 = 0.0;
    float t1 = 1.0;

    if (abs(ray.y) > 0.0001)
    {
        float at_floor = (floor_y - from.y) / ray.y;
        float at_ceiling = (ceiling - from.y) / ray.y;

        t0 = max(t0, min(at_floor, at_ceiling));
        t1 = min(t1, max(at_floor, at_ceiling));
    }
    else if (from.y < floor_y || from.y > ceiling)
    {
        return 0.0;
    }

    vec2 offset = from.xz - u_fog_bank_centre.xz;
    float a = dot(ray.xz, ray.xz);
    float b = 2.0 * dot(offset, ray.xz);
    float c = dot(offset, offset) - u_fog_bank_radius * u_fog_bank_radius;

    if (a > 0.000001)
    {
        float found = b * b - 4.0 * a * c;

        if (found < 0.0)
        {
            return 0.0;
        }

        float root = sqrt(found);
        t0 = max(t0, (-b - root) / (2.0 * a));
        t1 = min(t1, (-b + root) / (2.0 * a));
    }
    else if (c > 0.0)
    {
        return 0.0;
    }

    if (t1 <= t0)
    {
        return 0.0;
    }

    const int STEPS = 10;
    float thickness = max(u_fog_bank_top - u_fog_bank_bottom, 0.001);
    float step_length = (t1 - t0) * reach / float(STEPS);
    vec2 drift = vec2(0.6, 0.25) * u_time;
    float depth = 0.0;

    for (int i = 0; i < STEPS; ++i)
    {
        vec3 p = from + ray * mix(t0, t1, (float(i) + 0.5) / float(STEPS));

        float h = (p.y - floor_y) / thickness;
        float body = smoothstep(0.0, 0.3, h) * (1.0 - smoothstep(0.5, 1.0, h));
        float edge = 1.0 - smoothstep(0.65, 1.0, length(p.xz - u_fog_bank_centre.xz) / u_fog_bank_radius);

        vec2 at = p.xz + vec2(p.y * 0.8, -p.y * 0.5);
        float billows = 0.6 * fog_noise((at + drift) / 38.0) + 0.4 * fog_noise((at - drift * 1.5) / 14.0);

        depth += u_fog_bank_density * body * edge * smoothstep(0.42, 0.72, billows) * 2.4 * step_length;
    }

    return 1.0 - exp(-depth);
}

float hash(vec3 p)
{
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

void main()
{
    vec3 dir = normalize(v_direction);
    float up = dir.y;

    vec3 colour;

    if (up >= 0.0)
    {
        colour = mix(u_horizon, u_zenith, pow(up, u_horizon_falloff));
    }
    else
    {
        // Quickly to the ground colour below the horizon, so the seam where
        // the world ends is soft rather than a line.
        colour = mix(u_horizon, u_ground, smoothstep(0.0, 0.25, -up));
    }

    // Stars, where it is dark enough to see them and above the horizon.
    if (u_stars > 0.0 && up > 0.0)
    {
        vec3 cell = floor(dir * 300.0);
        float h = hash(cell);

        if (h > 0.9975)
        {
            float twinkle = 0.75 + 0.25 * sin(u_time * (1.0 + h * 3.0) + h * 40.0);
            colour += vec3(u_stars * twinkle) * smoothstep(0.0, 0.2, up);
        }
    }

    // The sun: a disc with a soft edge, and a glow round it that warms the
    // sky it is in.
    float toward = dot(dir, u_sun_direction);

    float disc = smoothstep(u_sun_cos_outer, u_sun_cos_inner, toward);
    // More glow is a wider glow as well as a brighter one.
    float exponent = mix(600.0, 18.0, clamp(u_sun_glow, 0.0, 1.0));
    float glow = u_sun_glow * pow(max(toward, 0.0), exponent);
    float halo = u_sun_glow * 0.35 * pow(max(toward, 0.0), 3.0) * (1.0 - clamp(up, 0.0, 1.0));

    colour += u_sun_color * (disc * 2.5 + glow + halo);

    if (u_fog_bank_enabled == 1)
    {
        colour = mix(colour, u_fog_bank_color, fog_bank_cover(dir));
    }

    frag_color = vec4(colour, 1.0);
}
)";

ref<shader> g_sky_shader;

ref<shader> sky_shader()
{
    if(!g_sky_shader)
    {
        g_sky_shader = make_ref<shader>(SKY_VERTEX, SKY_FRAGMENT, shader_source::memory);

        // Kept as source until asked to load: a shader never loaded has
        // program zero and draws nothing, silently.
        g_sky_shader->load();
    }

    return g_sky_shader;
}

} // namespace

sky::sky(ref<mesh_3d> mesh)
    : mesh_instance_3d(mesh)
{
}

sky::~sky()
{
}

void sky::set_settings(const sky_settings& settings)
{
    m_settings = settings;
}

const sky_settings& sky::settings() const
{
    return m_settings;
}

void sky::render(render_command_buffer& command_buffer, const render_context& context)
{
    if(!mesh())
    {
        return;
    }

    command_buffer.use_shader(sky_shader());

    // The camera's turning and nothing of its moving: the sky is infinitely
    // far off, so walking never brings any of it closer.
    command_buffer.set_uniform("u_view_rotation", glm::mat4(glm::mat3(context.view)));
    command_buffer.set_uniform("u_projection", context.projection);

    command_buffer.set_uniform("u_zenith", m_settings.zenith);
    command_buffer.set_uniform("u_horizon", m_settings.horizon);
    command_buffer.set_uniform("u_ground", m_settings.ground);
    command_buffer.set_uniform("u_horizon_falloff", std::max(m_settings.horizon_falloff, 0.01f));

    // Where the light comes from. The light's direction already points at
    // it -- the lit shader dots a surface's normal straight with it -- so the
    // sun sits where the shading says it is.
    const glm::vec3 toward = context.directional_light.direction;
    const glm::vec3 toward_sun = glm::length(toward) > 0.0f ? glm::normalize(toward)
                                                            : glm::vec3(0.0f, 1.0f, 0.0f);

    const float radius = glm::radians(std::max(m_settings.sun_size, 0.05f));

    command_buffer.set_uniform("u_sun_direction", toward_sun);
    command_buffer.set_uniform("u_sun_color", context.directional_light.enabled ? m_settings.sun_color
                                                                              : glm::vec3(0.0f));
    command_buffer.set_uniform("u_sun_cos_outer", std::cos(radius));
    command_buffer.set_uniform("u_sun_cos_inner", std::cos(radius * 0.75f));
    command_buffer.set_uniform("u_sun_glow", std::max(m_settings.sun_glow, 0.0f));

    command_buffer.set_uniform("u_stars", std::max(m_settings.stars, 0.0f));
    command_buffer.set_uniform("u_time", context.time);

    const fog_bank_data& bank = context.fog.bank;

    command_buffer.set_uniform("u_eye_position", context.eye_position);
    command_buffer.set_uniform("u_fog_bank_enabled", bank.enabled ? 1 : 0);
    command_buffer.set_uniform("u_fog_bank_centre", bank.centre);
    command_buffer.set_uniform("u_fog_bank_radius", std::max(bank.radius, 1.0f));
    command_buffer.set_uniform("u_fog_bank_bottom", bank.bottom);
    command_buffer.set_uniform("u_fog_bank_top", bank.top);
    command_buffer.set_uniform("u_fog_bank_density", bank.density);
    command_buffer.set_uniform("u_fog_bank_color", bank.color);

    command_buffer.set_depth_mask(false);
    command_buffer.draw_elements(GL_TRIANGLES, mesh()->vao(), mesh()->ebo(), mesh()->indices().size());
    command_buffer.set_depth_mask(true);
}

} // namespace nle

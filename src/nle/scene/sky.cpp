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

    // At the very back: drawn after everything solid, with its depth the far
    // plane's, so it fills only what nothing else has -- and the fog banks
    // seen against it are worked out only where it shows. Drawn first, every
    // pixel paid for them, ground and all, which standing in a bank was the
    // whole screen.
    vec4 at = u_projection * u_view_rotation * vec4(position, 1.0);
    gl_Position = at.xyww;
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
// Fog or cloud lying in one place -- a bowl, a marsh, a ring of valleys
// round a place -- rather than everywhere at a distance. See
// nle::fog_bank_data.
struct FogBank
{
    int enabled;
    vec3 centre;
    float radius;
    float inner;
    float bottom;
    float top;
    float density;
    float billow;
    vec3 color;

    // Radians a second its billows turn round its centre.
    float spin;
};

uniform FogBank u_fog_banks[16];
uniform int u_fog_bank_count;

// Value noise, read from a picture of it (renderer_3d::make_fog_noise) that
// repeats every 256 cells: one fetch, where working it out from a hash was
// four -- and a look through a fog bank asks for it two dozen times.
uniform sampler2D u_fog_noise;

float fog_noise(vec2 p)
{
    return texture(u_fog_noise, p / 256.0).r;
}

// The same as default_frag.glsl's fog_bank_shade.
vec3 fog_bank_shade(vec3 base, vec3 p, float h, float billows)
{
    float lit = clamp(0.15 + 0.95 * h - 0.9 * (billows - 0.55), 0.0, 1.0);

    return mix(base * vec3(0.36, 0.37, 0.43), base * 1.12, lit);
}

// The slow patches a little bluer or browner, seventy metres across: the
// same anywhere along one look through a bank, so worked out once for it
// rather than at every step.
vec3 fog_bank_tint(vec3 p)
{
    float tint = fog_noise((p.xz + vec2(u_time * 0.4, 0.0)) / 70.0);
    return mix(vec3(0.88, 0.93, 1.06), vec3(1.07, 1.0, 0.9), tint);
}

// The same sum as default_frag.glsl's fog_bank_cover, along a line out to
// the far side of the bank rather than to a surface.
vec4 fog_bank_cover(FogBank bank, vec3 dir)
{
    vec3 from = u_eye_position;
    float reach = 2.0 * bank.radius + length(from.xz - bank.centre.xz);
    vec3 ray = dir * reach;
    float floor_y = bank.centre.y + bank.bottom;
    float ceiling = bank.centre.y + bank.top;

    // Only the part of the line between the bottom and the top.
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
        return vec4(0.0);
    }

    // And inside its edge.
    vec2 offset = from.xz - bank.centre.xz;
    float a = dot(ray.xz, ray.xz);
    float b = 2.0 * dot(offset, ray.xz);
    float c = dot(offset, offset) - bank.radius * bank.radius;

    if (a > 0.000001)
    {
        float found = b * b - 4.0 * a * c;

        if (found < 0.0)
        {
            return vec4(0.0);
        }

        float root = sqrt(found);
        t0 = max(t0, (-b - root) / (2.0 * a));
        t1 = min(t1, (-b + root) / (2.0 * a));

        // A ring with the eye in its hole: start where the line comes out
        // of the hole, so every step is spent where there can be cloud.
        float hole = dot(offset, offset) - bank.inner * bank.inner;

        if (bank.inner > 0.0 && hole < 0.0)
        {
            t0 = max(t0, (-b + sqrt(b * b - 4.0 * a * hole)) / (2.0 * a));
        }
    }
    else if (c > 0.0)
    {
        return vec4(0.0);
    }

    if (t1 <= t0)
    {
        return vec4(0.0);
    }

    // As many steps as the length of the look through it needs: about one
    // every sixth of a billow, so a billow is never stepped over, and no more
    // than ten. Standing in a bank, most of the screen is ground or grass a
    // few metres off, which three steps cover as well as ten did; it was all
    // of those paying for ten that made walking into one slow.
    const int MOST_STEPS = 10;
    float path = (t1 - t0) * length(ray);
    int steps = int(clamp(ceil(path / max(bank.billow * 0.16, 0.05)), 3.0, float(MOST_STEPS)));
    float thickness = max(bank.top - bank.bottom, 0.001);
    float step_length = path / float(steps);
    vec2 drift = vec2(0.6, 0.25) * u_time * (bank.billow / 38.0);

    // Turning: where each point is looked up in the billows is carried back
    // round the centre by as far as the bank has turned, so the billows go
    // round. Kept within a turn, so a clock that has run for days loses
    // nothing to rounding.
    float turned = mod(bank.spin * u_time, 6.2831853);
    mat2 back = mat2(cos(turned), -sin(turned), sin(turned), cos(turned));
    vec3 colour = vec3(0.0);
    float through = 1.0;

    for (int i = 0; i < MOST_STEPS; ++i)
    {
        // Done: the steps are spent, or nothing more behind can show.
        if (i >= steps || through < 0.015)
        {
            break;
        }

        vec3 p = from + ray * mix(t0, t1, (float(i) + 0.5) / float(steps));

        // Up from nothing at the bottom, full through the middle, and away
        // to nothing at the top -- softer above than below, as cloud is.
        float h = (p.y - floor_y) / thickness;
        float body = smoothstep(0.0, 0.3, h) * (1.0 - smoothstep(0.5, 1.0, h));

        // Thinning to its edge, and to its hole if it is a ring.
        float out_from = length(p.xz - bank.centre.xz);
        float edge = (1.0 - smoothstep(0.65, 1.0, out_from / bank.radius))
                   * smoothstep(bank.inner, bank.inner * 1.2 + 0.001, out_from);

        // Billows, leaning with height so they are not columns.
        vec2 at = bank.centre.xz + back * (p.xz - bank.centre.xz) + vec2(p.y * 0.8, -p.y * 0.5);
        float billows = 0.6 * fog_noise((at + drift) / bank.billow)
                      + 0.4 * fog_noise((at - drift * 1.5) / (bank.billow * 0.37));

        float thick = bank.density * body * edge * smoothstep(0.42, 0.72, billows) * 2.4 * step_length;
        float taken = 1.0 - exp(-thick);

        // Front to back, so what is nearer covers what is behind it.
        colour += through * taken * fog_bank_shade(bank.color, p, h, billows);
        through *= 1.0 - taken;
    }

    float hidden = 1.0 - through;

    colour *= fog_bank_tint(from + ray * mix(t0, t1, 0.5));

    return vec4(colour / max(hidden, 0.0001), hidden);
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

    for (int i = 0; i < u_fog_bank_count; ++i)
    {
        if (u_fog_banks[i].enabled == 1)
        {
            vec4 cloud = fog_bank_cover(u_fog_banks[i], dir);
            colour = mix(colour, cloud.rgb, cloud.a);
        }
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

    command_buffer.set_uniform("u_eye_position", context.eye_position);

    const int bank_count = std::min(static_cast<int>(context.fog.banks.size()), MAX_FOG_BANKS);

    command_buffer.set_uniform("u_fog_bank_count", bank_count);
    command_buffer.set_uniform("u_fog_noise", context.fog_noise_texture_unit);

    for(int i = 0; i < bank_count; ++i)
    {
        const fog_bank_data& bank = context.fog.banks[static_cast<size_t>(i)];
        const std::string at = "u_fog_banks[" + std::to_string(i) + "].";

        command_buffer.set_uniform(at + "enabled", bank.enabled ? 1 : 0);
        command_buffer.set_uniform(at + "centre", bank.centre);
        command_buffer.set_uniform(at + "radius", std::max(bank.radius, 1.0f));
        command_buffer.set_uniform(at + "inner", std::max(bank.inner_radius, 0.0f));
        command_buffer.set_uniform(at + "bottom", bank.bottom);
        command_buffer.set_uniform(at + "top", bank.top);
        command_buffer.set_uniform(at + "density", bank.density);
        command_buffer.set_uniform(at + "billow", std::max(bank.billow, 1.0f));
        command_buffer.set_uniform(at + "color", bank.color);
        command_buffer.set_uniform(at + "spin", glm::radians(bank.spin));
    }

    command_buffer.set_depth_mask(false);
    command_buffer.draw_elements(GL_TRIANGLES, mesh()->vao(), mesh()->ebo(), mesh()->indices().size());
    command_buffer.set_depth_mask(true);
}

} // namespace nle

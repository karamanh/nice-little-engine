#version 330

#define NLE_MAX_POINT_LIGHTS 8

in vec4 io_vertex_color;
in vec2 io_texture_coordinates;
in vec3 io_normal;
in vec3 io_frag_position;
in vec4 io_light_space_position;

out vec4 io_color;

struct DirectionalLight
{
    // Direction *towards* the light, i.e. the light object's front vector.
    vec3 direction;
    vec3 color;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    int enabled;
};

struct PointLight
{
    vec3 position;
    vec3 color;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    // attenuation = 1 / (constant + linear * d + quadratic * d * d)
    float constant;
    float linear;
    float quadratic;

    // hard cutoff; the last quarter of the range is faded out so the cutoff
    // itself is not visible as an edge.
    float range;
};

struct Material
{
    float shininess;
    float dissolve;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    int accept_light;

    // The most the distance fog may take of this surface. One lets it take
    // everything; less keeps a faded silhouette -- scenery on the horizon.
    float fog_limit;
};

struct Sky
{
    int distance_fog_enabled;
    float distance_fog_near;
    float distance_fog_far;
    vec3 distance_fog_color;
};

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
};

// The ground, when this surface is one. Off for everything else, which is
// everything: a prop pays one comparison and never samples any of these.
//
// It lives here rather than in a shader of its own because a second copy of
// all the lighting and fog below is a second copy that will disagree with
// this one the first time either is touched.
uniform sampler2D u_terrain_base;
uniform sampler2D u_terrain_layer_0;
uniform sampler2D u_terrain_layer_1;
uniform sampler2D u_terrain_layer_2;
uniform sampler2D u_terrain_layer_3;
uniform sampler2D u_terrain_layer_4;
uniform sampler2D u_terrain_layer_5;
uniform sampler2D u_terrain_layer_6;
uniform sampler2D u_terrain_layer_7;

// One splat map to every four layers, a channel to a layer.
uniform sampler2D u_terrain_splat;
uniform sampler2D u_terrain_splat_b;

// How many uv units the ground spans, which is also how many samples the
// paint has across it. Nought means "this surface is not ground", and every
// other surface is given nought, because a uniform belongs to the program
// and the program is shared: without that, the first field of grass drawn
// would go on to wallpaper every tree behind it.
uniform float u_terrain_extent;

uniform int u_terrain_base_enabled;
uniform int u_terrain_layers_enabled;
uniform int u_terrain_splat_enabled;
uniform int u_terrain_splat_b_enabled;

uniform float u_terrain_base_tiling;
uniform vec4 u_terrain_layer_tiling;
uniform vec4 u_terrain_layer_tiling_b;

/**
 * The ground's own pictures: a base, and up to four painted over it, mixed
 * by a splat map whose four channels are the weights the paint brush has
 * been writing all along.
 *
 * Returns white when there is nothing to sample, so that multiplying by it
 * leaves the vertex colour exactly as it was. Ground painted before there
 * were any pictures is shaded the way it always was.
 */
vec3 ground_colour(vec2 uv)
{
    if (u_terrain_extent <= 0.0)
    {
        return vec3(1.0);
    }

    if (u_terrain_base_enabled != 1
        && u_terrain_splat_enabled != 1
        && u_terrain_splat_b_enabled != 1)
    {
        return vec3(1.0);
    }

    // The ground's own uv counts tiles rather than running nought to one, so
    // tiling is a multiple of the whole field only after this.
    vec2 field = uv / u_terrain_extent;

    vec3 result = vec3(1.0);

    if (u_terrain_base_enabled == 1)
    {
        result = texture(u_terrain_base, field * u_terrain_base_tiling).rgb;
    }

    // One texel to a sample, and there is one more sample than there are
    // tiles, so uv lands on a texel index directly. The half puts the read at
    // the texel's middle: without it the whole painting sits half a texel out.
    vec2 splat_uv = (uv + 0.5) / (u_terrain_extent + 1.0);

    vec4 weights = u_terrain_splat_enabled == 1
        ? texture(u_terrain_splat, splat_uv)
        : vec4(0.0);

    // Each over the last, in the order they are painted -- which is how the
    // brush behaves, and so how a road laid over grass is expected to look.
    if ((u_terrain_layers_enabled & 1) != 0)
    {
        result = mix(result, texture(u_terrain_layer_0, field * u_terrain_layer_tiling.x).rgb,
                     weights.r);
    }

    if ((u_terrain_layers_enabled & 2) != 0)
    {
        result = mix(result, texture(u_terrain_layer_1, field * u_terrain_layer_tiling.y).rgb,
                     weights.g);
    }

    if ((u_terrain_layers_enabled & 4) != 0)
    {
        result = mix(result, texture(u_terrain_layer_2, field * u_terrain_layer_tiling.z).rgb,
                     weights.b);
    }

    if ((u_terrain_layers_enabled & 8) != 0)
    {
        result = mix(result, texture(u_terrain_layer_3, field * u_terrain_layer_tiling.w).rgb,
                     weights.a);
    }

    // The next four, from the second map. Skipped entirely when there is no
    // second map, which is the usual case and costs one comparison.
    if (u_terrain_splat_b_enabled != 1)
    {
        return result;
    }

    vec4 more = texture(u_terrain_splat_b, splat_uv);

    if ((u_terrain_layers_enabled & 16) != 0)
    {
        result = mix(result, texture(u_terrain_layer_4, field * u_terrain_layer_tiling_b.x).rgb,
                     more.r);
    }

    if ((u_terrain_layers_enabled & 32) != 0)
    {
        result = mix(result, texture(u_terrain_layer_5, field * u_terrain_layer_tiling_b.y).rgb,
                     more.g);
    }

    if ((u_terrain_layers_enabled & 64) != 0)
    {
        result = mix(result, texture(u_terrain_layer_6, field * u_terrain_layer_tiling_b.z).rgb,
                     more.b);
    }

    if ((u_terrain_layers_enabled & 128) != 0)
    {
        result = mix(result, texture(u_terrain_layer_7, field * u_terrain_layer_tiling_b.w).rgb,
                     more.a);
    }

    return result;
}

uniform sampler2DShadow u_shadow_map;
uniform int u_shadows_enabled;
uniform float u_shadow_softness;


uniform int u_lighting_enabled = 1;
uniform int u_point_lighting_enabled = 1;
uniform int u_texture_enabled = 0;
uniform sampler2D u_texture_0;

uniform DirectionalLight u_directional_light;
uniform PointLight u_point_lights[NLE_MAX_POINT_LIGHTS];
uniform int u_point_light_count;

uniform Material u_material;
uniform Sky u_sky;
// Every bank in the level, as many as there are: see nle::MAX_FOG_BANKS.
uniform FogBank u_fog_banks[16];
uniform int u_fog_bank_count;
uniform float u_waves;
uniform vec3 u_eye_position;

// A polish laid over one thing: rgb its colour, w how much. Nought for
// nearly everything, and set only round the draw of the thing that has it.
uniform vec4 u_sheen = vec4(0.0);
uniform float u_sheen_tint = 0.0;
uniform float u_time;

/**
 * The sun's contribution, with what a shadow may take away kept apart from
 * what it may not.
 *
 * @param shaded how much of the sun reaches here, from sunlight_reaching.
 *
 * Ambient is the light that gets everywhere by definition, so a shadow does
 * not touch it. Shadowing it as well simply dims the whole world -- which
 * is exactly what happened on a level with its ambient at full white: every
 * surface went down by the same amount and nothing read as a shadow at all.
 */
vec3 directional_light_contribution(vec3 normal, vec3 view_direction, float shaded)
{
    vec3 direction = normalize(u_directional_light.direction);

    // The light's own ambient term belongs here too; leaving it out made
    // light::set_ambient() a no-op for directional lights. It defaults to
    // white, so default lights are unaffected.
    vec3 ambient = u_directional_light.color * u_directional_light.ambient * u_material.ambient;

    float diffuse_factor = max(dot(normal, direction), 0.0);
    vec3 diffuse = u_material.diffuse * u_directional_light.diffuse * diffuse_factor;

    vec3 reflect_direction = reflect(-direction, normal);
    float specular_factor = pow(max(dot(view_direction, reflect_direction), 0.0), max(u_material.shininess, 1.0));
    vec3 specular = u_material.specular * u_directional_light.specular * specular_factor;

    return ambient + (diffuse + specular) * shaded;
}

vec3 point_light_contribution(PointLight light, vec3 normal, vec3 view_direction)
{
    vec3 to_light = light.position - io_frag_position;
    float dist = length(to_light);

    if (dist > light.range)
    {
        return vec3(0.0);
    }

    vec3 direction = dist > 0.0 ? to_light / dist : normal;

    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);
    attenuation *= 1.0 - smoothstep(light.range * 0.75, light.range, dist);

    vec3 ambient = light.ambient * u_material.ambient;

    float diffuse_factor = max(dot(normal, direction), 0.0);
    vec3 diffuse = light.diffuse * u_material.diffuse * diffuse_factor;

    vec3 reflect_direction = reflect(-direction, normal);
    float specular_factor = pow(max(dot(view_direction, reflect_direction), 0.0), max(u_material.shininess, 1.0));
    vec3 specular = light.specular * u_material.specular * specular_factor;

    return light.color * (ambient + diffuse + specular) * attenuation;
}

float fog_factor()
{
    float f = (distance(u_eye_position, io_frag_position) - u_sky.distance_fog_near) /
              max(u_sky.distance_fog_far - u_sky.distance_fog_near, 0.0001);
    return clamp(f, 0.0, 1.0);
}

// Value noise, read from a picture of it (renderer_3d::make_fog_noise) that
// repeats every 256 cells: one fetch, where working it out from a hash was
// four -- and a look through a fog bank asks for it two dozen times.
uniform sampler2D u_fog_noise;

float fog_noise(vec2 p)
{
    return texture(u_fog_noise, p / 256.0).r;
}

/**
 * The colour of a fog bank at one place in it: darker low down and in the
 * thick of a billow, paler towards its top, and over the whole of it slow
 * patches a little bluer or a little browner, the way storm cloud is.
 */
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

/**
 * What a fog bank puts over this fragment: the colour of the cloud in rgb
 * and how much of the fragment it hides in a. The fog along the line from
 * the eye to here, counted only where that line is inside the bank. Thickest
 * between its bottom and top and gone at each, thinning to the edge, in
 * billows that drift, so it reads as cloud lying there rather than as a tint.
 */
vec4 fog_bank_cover(FogBank bank)
{
    vec3 from = u_eye_position;
    vec3 ray = io_frag_position - from;
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
        vec2 at = p.xz + vec2(p.y * 0.8, -p.y * 0.5);
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

/**
 * How much of this fragment the sun can actually reach.
 *
 * One at full daylight, down towards a floor in shadow -- never nought,
 * because a shadow that is pure black hides everything in it and reads as a
 * hole in the world rather than as shade.
 */
float sunlight_reaching(vec3 normal)
{
    if (u_shadows_enabled != 1)
    {
        return 1.0;
    }

    vec3 projected = io_light_space_position.xyz / io_light_space_position.w;

    projected = projected * 0.5 + 0.5;

    // Past the far plane of the light's box: no opinion, so full daylight.
    if (projected.z > 1.0)
    {
        return 1.0;
    }

    // Surfaces edge-on to the light need a larger bias, because one texel
    // of the depth map covers a long way across them. Without the slope
    // term every ground plane gets stripes at dawn and dusk.
    float facing = max(dot(normalize(normal), normalize(-u_directional_light.direction)), 0.0);
    float bias = max(0.0025 * (1.0 - facing), 0.0006);

    vec2 texel = u_shadow_softness / vec2(textureSize(u_shadow_map, 0));

    float lit = 0.0;

    // Nine samples in a ring, which is enough to take the staircase off an
    // edge without costing what a real blur would.
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            vec3 at = vec3(projected.xy + vec2(x, y) * texel, projected.z - bias);
            lit += texture(u_shadow_map, at);
        }
    }

    lit /= 9.0;

    // A floor, so shade is shade rather than a void. It can be lower than
    // it was now that this only takes the sun away and leaves the ambient
    // light alone -- nothing goes black, whatever this says.
    return mix(0.1, 1.0, lit);
}

// The swell on water: three long waves crossing at angles, so it never
// lines up into rows. The same here and in default_frag.glsl, which shades by
// its slopes. Height about one either way; u_waves scales it.
vec3 wave_at(vec2 at, float time)
{
    const vec2 D1 = vec2(0.80, 0.60);
    const vec2 D2 = vec2(-0.45, 0.89);
    const vec2 D3 = vec2(0.97, -0.24);
    const float K1 = 0.21;
    const float K2 = 0.33;
    const float K3 = 0.52;

    float p1 = dot(D1, at) * K1 + time * 0.6;
    float p2 = dot(D2, at) * K2 + time * 0.85;
    float p3 = dot(D3, at) * K3 + time * 1.1;

    float height = 0.5 * sin(p1) + 0.3 * sin(p2) + 0.2 * sin(p3);

    // And how fast that changes along x and along z.
    vec2 slope = 0.5 * K1 * cos(p1) * D1 + 0.3 * K2 * cos(p2) * D2 + 0.2 * K3 * cos(p3) * D3;

    return vec3(height, slope);
}

void main() {
    vec3 normal = normalize(io_normal);
    vec3 view_direction = normalize(u_eye_position - io_frag_position);

    vec4 base_color = u_texture_enabled == 1
        ? texture(u_texture_0, io_texture_coordinates)
        : io_vertex_color;

    // A picture with holes in it -- leaves on a card, a fence, hair -- is cut
    // out rather than blended. Blended, the clear part still wrote depth, so
    // whatever happened to be drawn after it (water, the tree behind) was
    // hidden behind nothing: the same tree drew right or wrong depending on
    // where it fell in the draw order. Cut out, the holes are holes. Only the
    // texture's own alpha: a whole thing made faint is still blended.
    if (u_texture_enabled == 1 && base_color.a < 0.5)
    {
        discard;
    }

    // Ground with pictures on it. White for everything that is not ground,
    // so this multiply changes nothing at all for a prop.
    base_color = vec4(base_color.rgb * ground_colour(io_texture_coordinates),
                      base_color.a);

    // Unlit surfaces (the sky, for one) keep their colour untouched.
    vec3 light_factor = vec3(1.0);

    if (u_lighting_enabled == 1 || u_point_lighting_enabled == 1)
    {
        light_factor = vec3(0.0);

        if (u_lighting_enabled == 1)
        {
            // Only the sun is shadowed. A point light is a lamp in a room
            // and casting shadows from every one of them would mean a depth
            // map each; the sun is the one everybody can see the shadow of.
            light_factor += directional_light_contribution(normal, view_direction,
                                                           sunlight_reaching(normal));
        }

        if (u_point_lighting_enabled == 1)
        {
            for (int i = 0; i < u_point_light_count && i < NLE_MAX_POINT_LIGHTS; ++i)
            {
                light_factor += point_light_contribution(u_point_lights[i], normal, view_direction);
            }
        }
    }

    io_color = vec4(base_color.rgb * light_factor, base_color.a * u_material.dissolve);

    // Water, shaded by its own waves whether or not it takes light: darker
    // in the troughs facing away, a glint where a slope turns the sun to the
    // eye, and the sky's pale towards the horizon, where water is a mirror.
    if (u_waves > 0.0)
    {
        vec3 wave = wave_at(io_frag_position.xz, u_time);

        // Ripples on the swell, too small for the mesh to carry.
        vec2 ripple = vec2(fog_noise(io_frag_position.xz * 0.9 + u_time * 0.8),
                           fog_noise(io_frag_position.zx * 0.9 - u_time * 0.6)) - 0.5;

        // Steeper than the swell really is, for the light only: a slope of a
        // few degrees does not show, and the mesh already carries the height.
        vec3 surface = normalize(vec3(-(4.0 * u_waves * wave.y + 0.3 * ripple.x), 1.0,
                                      -(4.0 * u_waves * wave.z + 0.3 * ripple.y)));

        vec3 towards_sun = normalize(u_directional_light.direction);
        float facing = dot(surface, towards_sun);
        float glint = pow(max(dot(reflect(-towards_sun, surface), view_direction), 0.0), 90.0);
        float mirror = pow(1.0 - max(dot(surface, view_direction), 0.0), 4.0);

        vec3 water = io_color.rgb * (0.75 + 0.55 * facing);
        water = mix(water, u_sky.distance_fog_color, mirror * 0.55);
        water += u_directional_light.color * glint * 0.7;

        io_color = vec4(water, min(io_color.a + mirror * 0.3 + glint, 1.0));
    }

    // A sheen, for something that has been worked on: brighter all over, a
    // rim of light round its edge and a glint where the sun catches it, each
    // more the further it has been taken. u_sheen.w is how far: nought to
    // one, and past one for something taken as far as it goes.
    if (u_sheen.w > 0.0)
    {
        float w = u_sheen.w;

        float rim = pow(1.0 - max(dot(normal, view_direction), 0.0), 2.0);
        vec3 towards_sun = normalize(-u_directional_light.direction);
        vec3 halfway = normalize(towards_sun + view_direction);
        // Broader as it goes, so the brightest catch the sun over more of
        // their surface rather than in a pinprick.
        float glint = pow(max(dot(normal, halfway), 0.0), mix(64.0, 18.0, clamp(w - 0.5, 0.0, 1.0)));

        // Taking on the colour through, as far as asked: the surface's own
        // light carried in the sheen's colour, and a little of it of its own,
        // so even its shaded side reads as frost or fire.
        vec3 base = mix(io_color.rgb,
                        dot(io_color.rgb, vec3(0.3, 0.55, 0.15)) * u_sheen.rgb * 1.6 + u_sheen.rgb * 0.08,
                        min(u_sheen_tint * w, 1.0));

        vec3 polished = base * (1.0 + 0.35 * w)
                      + u_sheen.rgb * w * ((0.55 + 0.5 * u_sheen_tint) * rim + 0.9 * glint + 0.05);

        // Kept under where bloom starts up to one -- polished, not a lamp --
        // and past it for the brightest, whose rim and glint flare.
        float cap = mix(0.97, 1.8, clamp(w - 1.0, 0.0, 1.0));

        io_color.rgb = max(base, min(polished, vec3(cap)));
    }

    if (u_sky.distance_fog_enabled == 1)
    {
        io_color.rgb = mix(io_color.rgb, u_sky.distance_fog_color,
                           min(fog_factor(), u_material.fog_limit));
    }

    // After the haze of distance, not before: a cloud in the bowl is
    // nearer than the hills far off behind it, and hides them as it hides
    // the sky.
    for (int i = 0; i < u_fog_bank_count; ++i)
    {
        if (u_fog_banks[i].enabled == 1)
        {
            vec4 cloud = fog_bank_cover(u_fog_banks[i]);
            io_color.rgb = mix(io_color.rgb, cloud.rgb, cloud.a);
        }
    }
}

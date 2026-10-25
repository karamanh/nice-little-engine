/**
 * @file render_context.h
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief Per-frame state shared by every draw in a frame.
 * @version 0.1
 * @date 2026-09-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "frustum.h"

#include <glm/glm.hpp>

#include <vector>

namespace nle
{

/// Upper bound on the point lights uploaded in a single frame. Must match
/// NLE_MAX_POINT_LIGHTS in the shaders.
constexpr int MAX_POINT_LIGHTS = 8;

/// Upper bound on the joints of a single skin. Must match NLE_MAX_JOINTS in
/// the shaders. Kept conservative: 64 joints is 1024 uniform components, which
/// every GL 3.3 implementation is required to provide.
constexpr int MAX_JOINTS = 64;

struct directional_light_data
{
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 color = glm::vec3(1.0f);
    glm::vec3 ambient = glm::vec3(1.0f);
    glm::vec3 diffuse = glm::vec3(1.0f);
    glm::vec3 specular = glm::vec3(1.0f);
    bool enabled = true;
};

struct point_light_data
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 color = glm::vec3(1.0f);
    glm::vec3 ambient = glm::vec3(0.0f);
    glm::vec3 diffuse = glm::vec3(1.0f);
    glm::vec3 specular = glm::vec3(1.0f);

    /// attenuation = 1 / (constant + linear * d + quadratic * d * d)
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;

    /// hard cutoff, beyond which the light contributes nothing.
    float range = 50.0f;
};

/**
 * @brief Fog or cloud lying in one place: a bowl, a marsh, a valley.
 *
 * Not distance fog, which hides what is far off everywhere alike. This is a
 * body of fog with a bottom, a top and an edge -- a slice of a cylinder round
 * @p centre -- thickest between the two and thinning to nothing at each and
 * towards the edge, drifting in patches. Whatever is looked at through it is
 * hidden by as much of it as the line of sight crosses. With a bottom above
 * the ground it is a cloud: clear air underneath, something huge flying in
 * and out of it overhead.
 */
/// How many fog banks a scene can have at once: what the shaders hold.
constexpr int MAX_FOG_BANKS = 16;

struct fog_bank_data
{
    bool enabled = false;

    /// Where it lies: the middle, and the height of its floor.
    glm::vec3 centre = glm::vec3(0.0f);

    /// How far out it reaches, thinning towards the edge.
    float radius = 100.0f;

    /**
     * @brief Clear inside this far from the middle: a ring rather than a
     *        disc, for cloud lying among the mountains round a place and not
     *        over it. Nought for a disc.
     */
    float inner_radius = 0.0f;

    /// How far above the floor it starts, and how far up it is gone.
    float bottom = 0.0f;
    float top = 10.0f;

    /// How thick it is at its thickest: how much of a metre of it hides.
    float density = 0.02f;

    glm::vec3 color = glm::vec3(0.8f);

    /// How big its billows are across, in metres: small for a bowl, large
    /// for weather lying over a range.
    float billow = 38.0f;
};

struct fog_data
{
    bool enabled = false;
    float near_distance = 0.0f;
    float far_distance = 1000.0f;
    /// colour fragments fade towards at far_distance.
    glm::vec3 color = glm::vec3(1.0f);

    /// Fog and cloud lying in places: a boss's bowl, the hollows among the
    /// hills where things live. At most MAX_FOG_BANKS; past that, ignored.
    std::vector<fog_bank_data> banks;
};

/**
 * @brief Everything a draw needs that is constant for the whole frame.
 *
 * Built once per frame by renderer_3d and uploaded once per shader program by
 * opengl_backend, rather than re-emitted by every render object.
 */
struct render_context
{
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
    glm::vec3 eye_position = glm::vec3(0.0f);
    glm::vec2 resolution = glm::vec2(1.0f);

    /// seconds since the renderer started, and since the previous frame.
    float time = 0.0f;
    float delta_time = 0.0f;

    directional_light_data directional_light;

    /**
     * @brief Where the sun is looking from, for asking what it could see.
     *
     * Only meaningful when shadows are on; otherwise it is the identity and
     * the shader is told not to look.
     */
    glm::mat4 light_space = glm::mat4(1.0f);

    bool shadows_enabled = false;

    /// Which texture unit the depth map is on. Fixed and high, so it cannot
    /// collide with a material's own textures.
    /// Well clear of the ground's own, which run from one up to eleven. It
    /// used to be seven, which is also where the ground's fourth picture
    /// goes -- the two were reading each other's texture, and only having
    /// shadows switched off kept it quiet.
    int shadow_texture_unit = 12;

    /// How soft the edge is, in texels of the depth map.
    float shadow_softness = 1.0f;
    std::vector<point_light_data> point_lights;
    fog_data fog;

    /// What the camera can see, from the two matrices above. Kept here so
    /// that everything asking the question asks it of the same answer.
    struct frustum view_frustum;
};

} // namespace nle

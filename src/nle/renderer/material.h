/**
 * @file material.h
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief 
 * @version 0.1
 * @date 2024-02-12
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#pragma once

#include <glm/glm.hpp>

#include "render_command.h"

#include <string>

namespace nle
{

class material
{
public:
    material();
    virtual ~material();

    void set_ambient(glm::vec3 ambient);
    glm::vec3 ambient() const;

    void set_diffuse(glm::vec3 diffuse);
    glm::vec3 diffuse() const;

    void set_specular(glm::vec3 specular); 
    glm::vec3 specular() const;

    void set_shininess(float shininess);
    float shininess() const;

    void set_dissolve(float dissolve);
    float dissolve() const;

    /**
     * @brief The most distance fog may take of this surface, nought to one.
     *
     * One, the default, lets the fog take all of it past its far distance.
     * Less keeps a faded silhouette however far off it is -- for scenery on
     * the horizon, which fog would otherwise simply erase.
     */
    void set_fog_limit(float limit);
    float fog_limit() const;

    void set_accept_light(bool accept);
    bool accept_light() const;

    /**
     * @brief How far the surface rises and falls in waves, in world units.
     *
     * Nought, the default, leaves it as modelled. More makes it water: the
     * vertices are lowered and raised again by a slow swell where they stand
     * in the world -- never above where they were modelled, so nothing dry
     * is ever flooded -- and the surface is shaded by its own slopes -- lit where
     * they face the sun, glinting, paler towards the horizon -- even when it
     * takes no light otherwise. Only as smooth as the mesh is fine.
     */
    void set_waves(float height);
    float waves() const;

    /**
     * @brief How this surface is mixed with what is already there.
     *
     * Only consulted for a surface that is not fully opaque, since an opaque
     * one covers what is behind it whatever the mode says. alpha is the
     * default and is what glass or a decal wants; additive is what a glow
     * wants, and has the property that black is invisible -- which is how a
     * mesh fades out without a per-vertex alpha channel to fade.
     */
    void set_blending(blend_mode mode);
    blend_mode blending() const;

    void set_id(const std::string& id);
    std::string id() const;

private:
    std::string m_id;
    glm::vec3 m_ambient;
    glm::vec3 m_diffuse;
    glm::vec3 m_specular;
    float m_specular_intensity;
    float m_shininess;
    float m_dissolve;
    float m_fog_limit = 1.0f;
    float m_waves = 0.0f;
    bool m_accept_light;
    blend_mode m_blending = blend_mode::alpha;
};

} // namespace nle

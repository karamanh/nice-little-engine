/**
 * @file render_object_3d.h
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief 
 * @version 0.1
 * @date 2024-02-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once

#include "../object/object_3d.h"
#include "../renderer/shader.h"
#include "render_command.h"
#include "render_context.h"

#include <GL/gl.h>

namespace nle
{

enum class primitive_type
{
    points = GL_POINTS,
    lines = GL_LINES,
    triangles = GL_TRIANGLES
};

enum class render_mode
{
    point = GL_POINT,
    line = GL_LINE,
    fill = GL_FILL 
};

enum class render_layer
{
    _0,
    _1,
    _2,
    _3,
    _4,
    _5,
    _6,
    _7
};

class render_object_3d : public object_3d
{
public:
    render_object_3d(const std::string &id = "");
    virtual ~render_object_3d();

    /**
     * @brief Records the draw for this object into @p command_buffer.
     *
     * @p context carries everything that is constant for the frame (camera,
     * lights, fog). Objects must not re-upload those; the backend does it once
     * per shader program per frame.
     */
    virtual void render(render_command_buffer& command_buffer, const render_context& context) = 0;

    /**
     * @brief Whether anything it draws is see-through.
     *
     * Something see-through does not write depth -- it must not, or it would
     * hide what is behind it -- which means anything opaque drawn after it
     * paints straight over it. So the renderer draws these last, and this is
     * how it knows which they are.
     *
     * False by default: a thing that says nothing is drawn as though it were
     * solid, which is the safe way round. Drawing an opaque thing late costs
     * nothing; drawing a see-through one early loses it.
     */
    virtual bool see_through() { return false; }

    /**
     * @brief Whether this is drawn into the sun's shadow map.
     *
     * True by default. Something made of light -- particles -- casts no
     * shadow, and drawing it there anyway drew it with its own shader and
     * its own blending into a pass that wanted depth and nothing else.
     */
    virtual bool casts_shadow() const { return true; }

    /**
     * @brief The radius of a sphere around this, in world units.
     *
     * For asking whether the camera could see it at all. Zero means it does
     * not know its own size, and nothing that says zero is ever culled by
     * shape -- which is the safe way round, since drawing something need-
     * lessly costs a draw and skipping something wrongly is a hole in the
     * world.
     */
    virtual float bounding_radius() const { return 0.0f; }

    virtual void set_render_mode(enum render_mode rm);
    virtual enum render_mode render_mode() const;

    virtual void set_render_layer(enum render_layer rl);
    virtual enum render_layer render_layer() const;

    virtual void set_primitive_type(enum primitive_type pt);
    virtual enum primitive_type primitive_type();

    virtual void set_visible(bool visible);
    virtual bool visible() const;

    virtual void set_shader(ref<class shader> shader);
    virtual ref<class shader> shader();

    virtual void set_material_override(ref<class material> material_override);
    virtual ref<class material> material_override();

    /**
     * @brief Drawn again round its silhouette in a flat colour: the outline
     *        a game puts on whatever the pointer is over.
     *
     * @param width how far the outline stands out, as a share of the
     *              distance to the eye, so it is the same few pixels near
     *              and far. Nought for none, which is the default.
     *
     * Needs the renderer to have an outline shader; see
     * renderer_3d::set_outline_shader.
     */
    /**
     * @brief A polish laid over it: brighter, a rim of light, a glint.
     *
     * @param amount from nought, none, upwards; about one is a lot.
     * @param colour the tint of the rim and the glint.
     * @param tint how far the surface itself takes on @p colour as well,
     *             nought to one, at full @p amount: a frost blade blue
     *             through, rather than steel with a blue edge. Nought keeps
     *             the surface its own colour.
     *
     * Handed down to the pieces of something made of several, so setting it
     * on a model sets it on all of it.
     */
    void set_sheen(float amount, const glm::vec3& colour = glm::vec3(1.0f), float tint = 0.0f);
    float sheen() const;
    const glm::vec3& sheen_colour() const;
    float sheen_tint() const;

    /**
     * @brief The most distance fog may hide this, nought to one.
     *
     * One, the default, lets fog take it entirely, as it takes everything.
     * Less keeps a landmark standing out of the haze however far off it is
     * -- the one thing on the horizon you are meant to be able to steer by.
     * The material's own limit still applies; this only lowers it, and for
     * this object alone.
     */
    void set_fog_cap(float cap);
    float fog_cap() const;

    void set_outline(float width, const glm::vec3& colour = glm::vec3(1.0f));
    float outline_width() const;
    const glm::vec3& outline_colour() const;

    /**
     * @brief A glowing shell round it: the same shape drawn again a little
     *        larger, in one colour, adding light, from its far side only --
     *        so it shows as a halo round its edges rather than a tint over
     *        its face. What a game puts on an enchanted thing.
     *
     * @param width how far the shell stands out, as a share of the distance
     *              to the eye, as the outline's does. Nought for none.
     * @param colour the light it adds; brighter is stronger.
     *
     * Drawn with the outline shader; see renderer_3d::set_outline_shader.
     */
    void set_glow_shell(float width, const glm::vec3& colour = glm::vec3(1.0f));
    float glow_shell_width() const;
    const glm::vec3& glow_shell_colour() const;

    ref<class render_object_3d> scene();

    std::set<ref<class render_object_3d>, std::owner_less<ref<class render_object_3d>>> render_objects();

    virtual void add_child(ref<object_3d> child) override;
    virtual void delete_child(ref<object_3d> child) override;
    
    nlohmann::json to_json() const override;
    void from_json(const nlohmann::json& j) override;

protected:
    /**
     * @brief Tells this object, and everything under it, which scene it is in.
     *
     * Called by scene_3d as objects are added. Override it to react to joining
     * a scene -- terrain_3d uses it to hand over props that were scattered
     * before there was a scene to put them in -- and call the base version.
     */
    virtual void set_scene(ref<class render_object_3d> scene);

private:
    ref<class shader> m_shader;
    ref<class render_object_3d> m_scene;
    ref<class material> m_material_override;

    float m_outline_width = 0.0f;
    float m_glow_shell_width = 0.0f;
    glm::vec3 m_glow_shell_colour = glm::vec3(1.0f);
    float m_sheen = 0.0f;
    float m_sheen_tint = 0.0f;
    float m_fog_cap = 1.0f;
    glm::vec3 m_sheen_colour = glm::vec3(1.0f);
    glm::vec3 m_outline_colour = glm::vec3(1.0f);

    std::set<ref<class render_object_3d>, std::owner_less<ref<class render_object_3d>>> m_render_objects;

    enum render_layer m_render_layer;
    enum render_mode m_render_mode;
    enum primitive_type m_primitive_type;

    bool m_visible;

    friend class scene_3d;
    friend class renderer_3d;
};

} // namespace nle

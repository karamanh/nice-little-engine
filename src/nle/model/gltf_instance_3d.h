/**
 * @file gltf_instance_3d.h
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief A placed, animatable instance of a glTF model.
 * @version 0.1
 * @date 2026-09-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "model_gltf.h"

#include "../animation/animator.h"
#include "../mesh/multimesh_instance_3d.h"

#include <set>
#include <string>

namespace nle
{

/**
 * @brief An instance of a model_gltf, with its own animation playback state.
 *
 * Geometry, materials and the rig are shared with the model; the pose is not,
 * so two instances of the same model can play different clips at different
 * points in time.
 *
 * Playback is advanced from render(), using the frame's delta time, so an
 * instance animates simply by being in a scene:
 *
 *     auto knight = model->create_gltf_instance();
 *     knight->animator()->play("walk");
 *     scene->add_child(knight);
 *
 * A consequence worth knowing: an instance that is culled, hidden or on a
 * disabled layer is not rendered and therefore does not advance. Drive
 * animator()->update() yourself if you need it to keep time regardless.
 */
class gltf_instance_3d : public multimesh_instance_3d
{
public:
    explicit gltf_instance_3d(ref<model_gltf> model);
    virtual ~gltf_instance_3d();

    ref<class animator> animator();

    ref<model_gltf> model() const;

    /// Whether render() advances the animator. On by default.
    void set_auto_advance(bool auto_advance);
    bool auto_advance() const;

    /**
     * @brief Stops drawing the primitives belonging to one node.
     *
     * For a model that ships a weapon welded into it. The character packs put
     * the staff in the wizard's hand as a node of the same file, so putting a
     * different staff there means not drawing the one that came with it --
     * and that has to be per instance, since two wizards may be holding
     * different things.
     *
     * The node is still posed and its children still follow it, so it
     * remains a perfectly good place to hang something from. Only the
     * geometry stops.
     *
     * A name the model has no node for is ignored.
     */
    void set_node_visible(const std::string& name, bool visible);

    /// Whether a node is being drawn. True for anything never hidden.
    bool node_visible(int node) const;

    /// Index of a node by name, or -1. Handed on from the skeleton so that
    /// callers attaching something to a bone need not reach through it.
    int node_index(const std::string& name) const;

    void render(render_command_buffer& command_buffer, const render_context& context) override;

    /**
     * @brief The sphere around the whole model, posed as it is now.
     *
     * A glTF model is a tree, and a part of it can sit a long way from the
     * thing's own origin -- a sword held out at arm's length, a roof over a
     * doorway. Measuring the pieces alone would draw the sphere around the
     * largest piece rather than around the model, and the model would
     * vanish at the edge of the screen while the part nearest the camera
     * was still on it.
     */
    float bounding_radius() const override;

    /**
     * @brief Whether it is drawn see-through: when a material override
     *        says it is not fully opaque.
     *
     * So a character can be made translucent as a whole -- somebody hidden,
     * seen by those who may -- and is then blended, sorted with everything
     * else see-through and drawn after the solid world, as a mesh would be.
     */
    bool see_through() override;

    /**
     * @brief Its model, when it is drawn exactly as the model is.
     *
     * Not when anything about this one copy changes how it looks: a material
     * of its own, a sheen, fog it shows through, a part hidden, a skin -- the
     * pose of a skin is its own -- or a mode other than filled.
     */
    const void* copies_key() override;

    /// Each part once for all of @p copies, every copy where it stands.
    void render_copies(render_command_buffer& command_buffer, const render_context& context,
                       const std::vector<render_object_3d*>& copies) override;

private:
    /// Moves its pose on, once a frame however often it is drawn.
    void advance(const render_context& context);

    ref<model_gltf> m_model;
    ref<class animator> m_animator;
    bool m_auto_advance = true;

    /**
     * @brief The frame it last moved on in, by the frame's clock.
     *
     * Something may be drawn more than once in a frame -- again for its
     * outline, again for the sun's shadows -- and each draw used to move
     * the pose on by a whole frame, so whatever was outlined played its
     * clips at twice the pace. It moves on in the first draw of a frame and
     * holds still for the rest.
     */
    float m_advanced_at = -1.0f;

    /// Nodes whose geometry is not drawn. A set rather than a flag per node,
    /// because hiding anything at all is the unusual case.
    std::set<int> m_hidden_nodes;
};

} // namespace nle

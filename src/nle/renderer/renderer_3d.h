/**
 * @file renderer_3d.h
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief 
 * @version 0.1
 * @date 2024-02-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#pragma once

#include "../core/clock.h"
#include "../window/window_glfw.h"
#include "../scene/scene_3d.h"
#include "render_command.h"
#include "render_context.h"
#include "bloom.h"
#include "shadow_map.h"
#include "opengl_backend.h"

namespace nle
{

struct render_layer_attribute
{
    bool visible = true;
    float render_distance = 500000.0f;
};

/**
 * @brief Draws a scene_3d into a window.
 *
 * Each frame the renderer builds one render_context describing the camera and
 * the lights, hands it to the backend, and then walks the scene asking every
 * visible object to record its draw. Objects never touch GL and never upload
 * frame constants themselves.
 */
class renderer_3d
{
public:
    /**
     * @brief The fog banks worth drawing this frame: near enough to be seen
     *        and somewhere in front of the camera, thinned as they near the
     *        limit, farthest first.
     *
     * Every pixel of the world and the sky tests every bank it is given, so
     * a bank behind the camera or across the map is cost and nothing else.
     */
    static std::vector<fog_bank_data> banks_in_sight(const fog_data& fog, const glm::vec3& eye,
                                                     const frustum& view);

    renderer_3d(ref<window_glfw> render_target);
    virtual ~renderer_3d();

    void set_current_scene(ref<scene_3d> scene);
    ref<scene_3d> current_scene();

    ref<window_glfw> render_target();

    void set_render_layer_attribute(enum render_layer layer, const render_layer_attribute& attribute);
    render_layer_attribute render_layer_attribute_of(enum render_layer layer);

    /// Command/draw counts for the frame that was just submitted.
    const opengl_backend::frame_statistics& statistics() const;

    /// Seconds spent on the previous frame.
    float delta_time() const;

    /**
     * @brief How long the GPU spent drawing the 3D scene, in milliseconds:
     *        a frame or two ago, since asking for this frame's would wait for
     *        it. What a frame rate held to the screen's cannot show.
     */
    float gpu_milliseconds() const;

    void set_clear_color(const glm::vec3& color);
    glm::vec3 clear_color() const;

    /// Post-processing. Bright things bleed light into what is around them.
    class bloom& bloom();

    /// What the sun can see. Off until something turns it on.
    class shadow_map& shadows();

    /**
     * @brief The shader the depth pass draws everything with.
     *
     * Supplied rather than built here, because shaders are loaded from
     * wherever the program keeps them and the renderer has never known
     * where that is. Without one, shadows stay off however they are set.
     */
    void set_depth_shader(ref<class shader> shader);

    /**
     * @brief The shader outlines are drawn with: anything given an outline
     *        is drawn again with it, swollen along its normals and only its
     *        back faces, so what shows is a rim round its edge.
     *
     * Supplied for the same reason as the depth shader. Without one, an
     * outline asked for is simply not drawn.
     */
    void set_outline_shader(ref<class shader> shader);

    /**
     * @brief Draws @p scene into @p target rather than the window.
     *
     * For a second, small scene shown somewhere in the interface -- the
     * character on the character screen -- while the window keeps its own.
     * Cleared to @p clear first, which with an alpha of nought leaves what
     * nothing covers see-through. No shadows and no bloom: it is a picture,
     * not the world.
     *
     * Leaves whatever framebuffer, viewport and blending were current as
     * they were, so it may be called in the middle of drawing the interface.
     */
    void render_to(ref<scene_3d> scene, class render_texture& target,
                   const glm::vec4& clear = glm::vec4(0.0f));

private:
    ref<window_glfw> m_render_target;

    ref<scene_3d> m_current_scene;

    std::unordered_map<render_layer, render_layer_attribute> m_render_layer_attributes;

    render_command_buffer m_command_buffer;
    opengl_backend m_opengl_backend;

    class bloom m_bloom;
    class shadow_map m_shadows;

    ref<class shader> m_depth_shader;
    ref<class shader> m_outline_shader;

    /// Recorded one outlined thing at a time, since each may want its own
    /// width and colour and those are said once per draw.
    render_command_buffer m_outline_commands;

    /// A second buffer, so recording the depth pass does not disturb the
    /// one the lit pass is about to use.
    render_command_buffer m_shadow_commands;

    clock m_clock;
    int64_t m_last_frame_us = 0;

    /// Two timer queries, taken in turns, so the one read is always a frame
    /// old and finished.
    unsigned int m_gpu_queries[2] = { 0, 0 };

    /**
     * @brief The fog's noise, worked out once into a picture rather than in
     *        every fragment, every step of the way through a fog bank.
     *
     * The same value noise the shaders made of a hash, eight texels to a
     * cell so the filtering follows its smooth curve, repeating every 256
     * cells -- seven kilometres of fog bank -- so nobody sees it repeat.
     * Reading it is one fetch where working it out was four hashes.
     */
    unsigned int m_fog_noise = 0;
    void make_fog_noise();
    uint64_t m_gpu_frames = 0;
    float m_gpu_ms = 0.0f;
    float m_delta_time = 0.0f;
    float m_time = 0.0f;

    glm::vec3 m_clear_color = glm::vec3(0.5f);

    /// Gathers the camera, lights and fog for this frame.
    render_context build_render_context(ref<scene_3d> scene);

    /**
     * @brief Whether this is worth recording a draw for.
     *
     * Its layer, then how far away it is, then whether the camera is
     * pointing at it.
     */
    /// The outline pass, after everything else: see set_outline_shader.
    void draw_outlines(const ref<scene_3d>& scene, const render_context& context);

    /// Everything with a glow shell, its shell: see set_glow_shell.
    void draw_glow_shells(const ref<scene_3d>& scene, const render_context& context);

    bool is_visible(const ref<render_object_3d>& ro, const glm::vec3& eye,
                    const frustum& view);

    void render_scene(ref<scene_3d> scene, bool with_shadows = true);

    void main_routine();
};

} // namespace nle

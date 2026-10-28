/**
 * @file opengl_backend.h
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief OpenGL backend for executing render commands
 * @version 0.1
 * @date 2024-11-07
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once

#include "render_command.h"
#include "render_context.h"

#include <GL/glew.h>

#include <unordered_set>

namespace nle
{

class shader;

/**
 * @brief Translates a render_command_buffer into OpenGL calls.
 *
 * The backend owns all GL state tracking for a frame. Two things it does that
 * the objects recording commands do not have to care about:
 *
 * - Redundant state changes (shader binds, polygon mode, depth mask, vertex
 *   array binds) are dropped.
 * - The frame constants in render_context (camera, lights, fog) are uploaded
 *   lazily, once per shader program per frame, the first time that program is
 *   bound. Render objects therefore only emit what is actually per-draw.
 *
 * Cached state is invalidated at begin_frame(), because anything else sharing
 * the context -- ImGui, a mesh upload -- may have changed it behind our back.
 */
class opengl_backend
{
public:
    struct frame_statistics
    {
        size_t commands = 0;
        size_t draw_calls = 0;
        size_t shader_binds = 0;
        size_t uniform_uploads = 0;
        size_t redundant_commands_dropped = 0;
    };

    opengl_backend();
    ~opengl_backend();

    /// Records the frame constants and drops all cached GL state.
    void begin_frame(const render_context& context);

    /**
     * @brief Draws everything with this shader instead of its own.
     *
     * For a pass that only wants depth. Every object still records the
     * shader it would normally use and every uniform it would normally set;
     * the ones this shader has no use for are dropped on the floor, as any
     * uniform that does not exist already is.
     *
     * Null puts it back to each object using its own.
     */
    void force_shader(ref<class shader> shader);

    void execute_commands(const render_command_buffer& command_buffer);

    const frame_statistics& statistics() const;

    /// Forgets what it believes is bound, for after something has bound a
    /// program or a buffer without going through here.
    void invalidate_state_cache();

private:
    /// Set while a pass wants everything drawn with one shader.
    ref<class shader> m_forced_shader;

    void execute_command(const render_command& cmd);

    /// Uploads the render_context constants into @p shader, once per frame.
    void upload_frame_uniforms(const ref<class shader>& shader);

    int location(const std::string& name);

    void bind_shader(const ref<class shader>& shader);

    render_context m_context;

    ref<class shader> m_current_shader;

    /// 0 is a valid-but-never-linked program name, so it doubles as "unknown".
    GLuint m_current_program;
    GLuint m_current_vao;
    GLuint m_current_ebo;

    /// Where draw_copies() puts its placements on the way to the shader:
    /// one buffer, filled anew for each such draw.
    GLuint m_copies_buffer = 0;

    void draw_copies(const draw_copies_data& data);

    /// -1 means "unknown", forcing the next command through.
    GLint m_current_polygon_mode;
    GLint m_current_depth_mask;

    /// What is currently blended and how, so that a hundred particle draws in
    /// a row set it once.
    blend_mode m_current_blend_mode = blend_mode::none;

    /// programs that already have this frame's constants.
    std::unordered_set<GLuint> m_frame_uniform_programs;

    frame_statistics m_statistics;
};

} // namespace nle

/**
 * @file bloom.h
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief Post-processing: bright things bleed light into what is around them.
 * @version 0.1
 * @date 2026-09-21
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "gl.h"

#include <glm/glm.hpp>

#include "../core/ref.h"

namespace nle
{

class shader;

/**
 * @brief Draws the scene into a buffer, then blooms it onto the screen.
 *
 * The scene goes into a floating point target rather than straight to the
 * window, which is the point: a torch at seven times intensity is brighter
 * than white, and a normal target would have thrown that away before anything
 * could notice. Pixels above a threshold are taken out, blurred, and added
 * back.
 *
 * The blur runs at half resolution. Bloom is a wide, soft thing, so the
 * missing detail is not visible, and it is four times cheaper.
 *
 * Drop-in: if the buffers cannot be made, begin() says so and the caller
 * draws to the window as it did before.
 *
 *     if(!bloom.begin(width, height)) { glViewport(...); glClear(...); }
 *     ... draw the scene ...
 *     bloom.end();
 */
class bloom
{
public:
    bloom();
    ~bloom();

    bloom(const bloom&) = delete;
    bloom& operator=(const bloom&) = delete;

    void set_enabled(bool enabled);
    bool enabled() const;

    /// Brightness at which a pixel starts to glow. Above 1 means only things
    /// brighter than white bloom, which is usually what looks right.
    void set_threshold(float threshold);
    float threshold() const;

    /// How far past the threshold a pixel has to be before it glows fully.
    /// Zero makes the onset a hard edge, which shimmers as things move.
    void set_softness(float softness);
    float softness() const;

    /// How much of the glow is added back.
    void set_intensity(float intensity);
    float intensity() const;

    /// Blur iterations; each is a horizontal and a vertical pass. More is
    /// wider and softer rather than stronger.
    void set_passes(int passes);
    int passes() const;

    /**
     * @brief Points drawing at the offscreen target, sizing it to the window.
     *
     * @return false when it is off or could not be set up, in which case the
     *         caller should prepare the window itself.
     */
    bool begin(int width, int height);

    /// Resolves the glow and draws the result to the window.
    void end();

private:
    struct target
    {
        GLuint framebuffer = 0;
        GLuint color = 0;

        /// Only the scene target needs one: the blur passes have no depth.
        GLuint depth = 0;

        int width = 0;
        int height = 0;
    };

    bool m_enabled = true;
    float m_threshold = 1.0f;
    float m_softness = 0.6f;
    float m_intensity = 0.65f;
    int m_passes = 4;

    bool m_active = false;
    bool m_broken = false;

    int m_width = 0;
    int m_height = 0;

    target m_scene;
    target m_blur[2];

    ref<class shader> m_bright;
    ref<class shader> m_blur_shader;
    ref<class shader> m_composite;

    /// Empty: the fullscreen triangle's corners are worked out from the
    /// vertex index, so there is nothing to put in a buffer.
    GLuint m_empty_vertex_array = 0;

    bool ensure_shaders();
    bool ensure_targets(int width, int height);
    bool make_target(target& t, int width, int height, bool with_depth);
    void destroy(target& t);

    void draw_fullscreen();
};

} // namespace nle

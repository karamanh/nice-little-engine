/**
 * @file imgui_layer.h
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief Dear ImGui, set up against the engine's window and render loop.
 * @version 0.1
 * @date 2026-09-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "../core/ref.h"
#include "../window/platform_window.h"

#include <string>

namespace nle
{

/**
 * @brief Owns the imgui context and runs one frame of it.
 *
 * Tools built on the engine need a real interface -- panels, fields, text --
 * and imgui is already linked. This is the whole integration: construct one,
 * draw between begin_frame() and end_frame() from window::render_ui(), and
 * let it go out of scope to shut down.
 *
 *     nle::imgui_layer ui(app.window());
 *
 *     app.window()->render_ui() = [&]() {
 *         ui.begin_frame();
 *         ImGui::Begin("Tools");
 *         ...
 *         ImGui::End();
 *         ui.end_frame();
 *     };
 *
 * Only one of these may exist at a time, since the imgui context is global.
 *
 * wants_mouse() and wants_keyboard() are what keep a tool from acting on input
 * that the interface has already taken: a click that lands on a panel must not
 * also place something in the world behind it.
 */
class imgui_layer
{
public:
    /// @param glsl_version the version to compile imgui's shaders against.
    /// On OpenGL ES the version is always ES 3.00's, whatever is asked.
    explicit imgui_layer(ref<platform_window> window, unsigned int glsl_version = 330);
    ~imgui_layer();

    imgui_layer(const imgui_layer&) = delete;
    imgui_layer& operator=(const imgui_layer&) = delete;

    void begin_frame();
    void end_frame();

    /**
     * @brief Whether a frame is already in progress.
     *
     * Anything that draws a frame of its own -- a loading screen, say -- has
     * to ask first. Rendering one from inside another ends the outer frame
     * halfway through, with windows still open, which imgui catches as
     * mismatched Begin/End calls and which is fatal.
     */
    bool frame_open() const;

    /**
     * @brief Where the window positions are remembered.
     *
     * Every imgui program writes "imgui.ini" beside itself by default, so two
     * of them run from one directory overwrite each other's layouts -- and a
     * layout written by one is nonsense to the other, since the panels are
     * not the same. Give each its own name.
     *
     * Empty stops them being remembered at all, which is what a program with
     * no windows worth keeping wants.
     */
    void set_layout_file(const std::string& path);

    /**
     * @brief Has the font texture made again before the next frame.
     *
     * For fonts added to the atlas after it was first built. Before the
     * first frame there is nothing to rebuild and adding them is enough.
     */
    void rebuild_fonts();

    /// True when the interface is using the mouse, so the world should not.
    bool wants_mouse() const;

    /**
     * @brief True when any interface window has keyboard focus.
     *
     * Broader than it sounds, and usually not what a game wants: with
     * keyboard navigation on, this is true for as long as a panel is merely
     * open, so a bag left up would stop somebody walking. See wants_text().
     */
    bool wants_keyboard() const;

    /**
     * @brief True only when something is actually being typed into.
     *
     * The question a game usually means: a w typed into a name should not
     * walk anyone, but a w pressed while the bag happens to be open should.
     */
    bool wants_text() const;

private:
    ref<platform_window> m_window;
    bool m_frame_open = false;

    /// imgui keeps the pointer, not the string, so this has to outlive it.
    std::string m_layout_file;
};

} // namespace nle

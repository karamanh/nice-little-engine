/**
 * @file window_glfw.h
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief 
 * @version 0.1
 * @date 2024-02-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <string>
#include <functional>

#include "window.hpp"
#include "input_handler_glfw.h"
#include "../core/ref.h"

namespace nle
{

class window_glfw : public window<GLFWwindow*> {
public:
    window_glfw(unsigned int width, unsigned int height, const std::string& title = NLE_WINDOW_DEFAULT_TITLE);
    ~window_glfw();

    void display();
    void close();

    void set_fullscreen(bool fullscreen);
    bool fullscreen();

    void set_cursor_visibility(bool visible);
    bool cursor_visibility();

    /**
     * @brief The most frames a second to draw, or nought for as many as the
     *        machine can.
     *
     * Drawing more than the eye or the screen can use is heat and noise and
     * battery and nothing else: a town at a hundred and forty-four frames a
     * second costs twice what it costs at sixty and looks the same.
     *
     * @param unfocused the most while the window is not the one in front --
     *                  somebody reading something else does not need sixty.
     */
    void set_frame_limit(int fps, int unfocused = 20);

    /// Whether each frame waits for the screen to be ready for it.
    void set_vsync(bool on);

    /**
     * @brief How many times a second the screen the window is on redraws.
     *
     * The screen it is on now -- the one under the middle of the window, or
     * the one it fills -- not the system's first, so moving the window from
     * a laptop's screen to a monitor beside it gives the monitor's.
     */
    int refresh_rate() const;

    ref<input_handler_glfw> input_handler();

    /// The underlying GLFW window. Needed by anything that talks to GLFW
    /// directly, such as the imgui backend.
    GLFWwindow* handle() const;

private:
    ref<input_handler_glfw> m_input_handler;

    int m_frame_limit = 0;
    int m_unfocused_limit = 20;

    /// Holds the frame back to its share of a second, if there is a limit.
    void pace(double frame_started);

    /// The screen the window is on: the one it fills, or the one under its
    /// middle. The system's first if it is somehow on none.
    GLFWmonitor* current_monitor() const;
};

} // namespace nle

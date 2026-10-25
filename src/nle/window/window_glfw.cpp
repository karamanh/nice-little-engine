/**
 * @file window_glfw.cpp
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief 
 * @version 0.1
 * @date 2024-02-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "window_glfw.h"
#include "../core/utils.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace nle
{
window_glfw::window_glfw(unsigned int width, unsigned int height, const std::string& title)
{
    GLenum err = glfwInit();
    if (err != GLFW_TRUE)
    {
        throw std::runtime_error("fatal_error: could not initialize glfw. error code: " + std::to_string(err));
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // Switching to another window leaves this one where it is rather than
    // minimising it, unless it has taken the screen over: see set_mode.
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);

    m_handle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    glfwMakeContextCurrent(m_handle);

    err = glewInit();
    if (err != GLEW_OK)
    {
        throw std::runtime_error("fatal_error: could not initialize glew. error code: " + std::to_string(err));
    }

    m_input_handler = make_ref<input_handler_glfw>(m_handle);

    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    utils::print((char*)vendor);
    utils::print((char*)renderer);

    glfwSetWindowUserPointer(m_handle, this);
    
    // mouse & keyboard handlers
}

window_glfw::~window_glfw()
{
    glfwDestroyWindow(m_handle);
    glfwTerminate();
}

void window_glfw::display()
{
    double last_frame = glfwGetTime();

    while (!glfwWindowShouldClose(m_handle))
    {
        const double now = glfwGetTime();
        const float delta_time = static_cast<float>(now - last_frame);
        last_frame = now;

        glfwGetWindowSize(m_handle, &m_width, &m_height);

        // Minimised there is nothing to draw on, and drawing on nought by
        // nought is where the trouble was: everything sized by the window
        // is sized nought. The game goes on -- it is still connected, and
        // the world does not stop -- it is only not drawn.
        const bool showing = m_width > 0 && m_height > 0 && !glfwGetWindowAttrib(m_handle, GLFW_ICONIFIED);

        if(update())
        {
            update()(delta_time);
        }

        if(render_3d() && showing)
        {
            render_3d()();
        }

        if(render_ui() && showing)
        {
            render_ui()();
        }

        m_input_handler->poll_keyboard_input();
        m_input_handler->poll_mouse_input();
        
        glfwSwapBuffers(m_handle);
        glfwPollEvents();

        pace(now);
    }
    m_closed = true;
}

void window_glfw::set_frame_limit(int fps, int unfocused)
{
    m_frame_limit = fps > 0 ? fps : 0;
    m_unfocused_limit = unfocused > 0 ? unfocused : 0;
}

void window_glfw::set_vsync(bool on)
{
    glfwMakeContextCurrent(m_handle);
    glfwSwapInterval(on ? 1 : 0);
}

void window_glfw::pace(double frame_started)
{
    // Minimised, nobody sees anything; behind another window, barely.
    int limit = m_frame_limit;

    if(glfwGetWindowAttrib(m_handle, GLFW_ICONIFIED))
    {
        limit = 5;
    }
    else if(!glfwGetWindowAttrib(m_handle, GLFW_FOCUSED) && m_unfocused_limit > 0)
    {
        limit = limit > 0 ? std::min(limit, m_unfocused_limit) : m_unfocused_limit;
    }

    if(limit <= 0)
    {
        return;
    }

    const double frame = 1.0 / static_cast<double>(limit);

    // Slept for most of what is left, and the last millisecond waited out by
    // yielding, since a sleep may overshoot by about that much.
    for(;;)
    {
        const double left = frame - (glfwGetTime() - frame_started);

        if(left <= 0.0)
        {
            break;
        }

        if(left > 0.002)
        {
            std::this_thread::sleep_for(std::chrono::duration<double>(left - 0.0015));
        }
        else
        {
            std::this_thread::yield();
        }
    }
}

void window_glfw::close()
{
    glfwSetWindowShouldClose(m_handle, GLFW_TRUE);
}

GLFWmonitor* window_glfw::current_monitor() const
{
    if(GLFWmonitor* filling = glfwGetWindowMonitor(m_handle))
    {
        return filling;
    }

    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    glfwGetWindowPos(m_handle, &x, &y);
    glfwGetWindowSize(m_handle, &w, &h);

    const int middle_x = x + w / 2;
    const int middle_y = y + h / 2;

    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);

    for(int i = 0; i < count; ++i)
    {
        int mx = 0;
        int my = 0;
        glfwGetMonitorPos(monitors[i], &mx, &my);

        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);

        if(mode != nullptr && middle_x >= mx && middle_x < mx + mode->width
           && middle_y >= my && middle_y < my + mode->height)
        {
            return monitors[i];
        }
    }

    return glfwGetPrimaryMonitor();
}

int window_glfw::refresh_rate() const
{
    GLFWmonitor* monitor = current_monitor();
    const GLFWvidmode* mode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;

    return mode != nullptr ? mode->refreshRate : 60;
}

void window_glfw::set_mode(window_mode mode)
{
    // On the screen the window is on, not the system's first: a laptop with
    // a monitor beside it went fullscreen on the laptop whichever screen the
    // game had been moved to.
    GLFWmonitor* monitor = current_monitor();
    const GLFWvidmode* vm = glfwGetVideoMode(monitor);

    int mx = 0;
    int my = 0;
    glfwGetMonitorPos(monitor, &mx, &my);

    m_mode = mode;

    switch(mode)
    {
        case window_mode::fullscreen:
        {
            // The screen taken over, at the mode it is already in so nothing
            // changes on the way in. On Windows minimised when something else
            // is switched to, which is what taking the screen over means
            // there. Not on Linux, where a window that is not yet in front
            // when it goes fullscreen minimised itself before it had begun.
#ifdef _WIN32
            glfwSetWindowAttrib(m_handle, GLFW_AUTO_ICONIFY, GLFW_TRUE);
#else
            glfwSetWindowAttrib(m_handle, GLFW_AUTO_ICONIFY, GLFW_FALSE);
#endif
            glfwSetWindowAttrib(m_handle, GLFW_DECORATED, GLFW_TRUE);
            glfwSetWindowMonitor(m_handle, monitor, 0, 0, vm->width, vm->height, vm->refreshRate);
            break;
        }

        case window_mode::borderless:
        {
            // A window with no frame over the whole screen. Switching away
            // from it is switching windows: no change of mode on the way
            // out or back.
            glfwSetWindowAttrib(m_handle, GLFW_AUTO_ICONIFY, GLFW_FALSE);
            glfwSetWindowAttrib(m_handle, GLFW_DECORATED, GLFW_FALSE);

            // A pixel taller than the screen on Windows. A frameless window
            // exactly the size of the screen is taken by some drivers for one
            // that has taken the screen over, and alt-tab then flashed it
            // black and white as the two kinds were swapped back and forth.
#ifdef _WIN32
            const int taller = 1;
#else
            const int taller = 0;
#endif
            glfwSetWindowMonitor(m_handle, nullptr, mx, my, vm->width, vm->height + taller, 0);
            break;
        }

        case window_mode::windowed:
        {
            glfwSetWindowAttrib(m_handle, GLFW_AUTO_ICONIFY, GLFW_FALSE);
            glfwSetWindowAttrib(m_handle, GLFW_DECORATED, GLFW_TRUE);

            // In the middle of that same screen.
            const int width = NLE_WINDOW_DEFAULT_WIDTH;
            const int height = NLE_WINDOW_DEFAULT_HEIGHT;
            const int x = vm != nullptr ? mx + (vm->width - width) / 2 : mx;
            const int y = vm != nullptr ? my + (vm->height - height) / 2 : my;

            glfwSetWindowMonitor(m_handle, nullptr, x, y, width, height, 0);
            break;
        }
    }
}

window_mode window_glfw::mode() const
{
    return m_mode;
}

void window_glfw::set_cursor_visibility(bool visible)
{
    glfwSetInputMode(m_handle, GLFW_CURSOR, (m_cursor_visible = visible) ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
}

bool window_glfw::cursor_visibility()
{
    return m_cursor_visible;
}

ref<input_handler_glfw> window_glfw::input_handler()
{
    return m_input_handler;
}

GLFWwindow* window_glfw::handle() const
{
    return m_handle;
}

} // namespace nle

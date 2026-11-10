#ifdef NLE_SDL_WINDOW

#include "window_sdl.h"
#include "../renderer/gl.h"
#include "../core/utils.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>

namespace nle
{

window_sdl::window_sdl(unsigned int width, unsigned int height, const std::string& title)
{
    // Fingers are fingers: the game says which of them is a click. See
    // input_handler_sdl.h.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");

    // Held sideways, either way up.
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

    // Back is the game's to answer -- it closes what is open -- rather than
    // the end of the activity.
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");

    // Switched away from, the game keeps going rather than waiting where it
    // stood: it is connected to a world that does not wait.
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");

    // Controllers are read; a phone's tilt is not one, and is not kept
    // switched on to be offered as one.
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");

    if(SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0)
    {
        throw std::runtime_error(std::string("fatal_error: could not start SDL: ") + SDL_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);

    m_handle = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                static_cast<int>(width), static_cast<int>(height),
                                SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALLOW_HIGHDPI);

    if(m_handle == nullptr)
    {
        throw std::runtime_error(std::string("fatal_error: could not open a window: ") + SDL_GetError());
    }

    m_context = SDL_GL_CreateContext(m_handle);

    if(m_context == nullptr)
    {
        throw std::runtime_error(std::string("fatal_error: no OpenGL ES 3 here: ") + SDL_GetError());
    }

    SDL_GL_MakeCurrent(m_handle, m_context);
    SDL_GL_SetSwapInterval(1);

    m_input_handler = make_ref<input_handler_sdl>(m_handle);
    m_mode = window_mode::fullscreen;
    m_cursor_visible = true;
    m_started = SDL_GetPerformanceCounter();

    utils::print(reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
    utils::print(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    utils::print(reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    measure();
}

window_sdl::~window_sdl()
{
    if(m_context != nullptr)
    {
        SDL_GL_DeleteContext(m_context);
    }

    if(m_handle != nullptr)
    {
        SDL_DestroyWindow(m_handle);
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER);
}

void window_sdl::measure()
{
    int pixels_wide = 0;
    int pixels_tall = 0;
    SDL_GL_GetDrawableSize(m_handle, &pixels_wide, &pixels_tall);

    m_drawable_width = std::max(pixels_wide, 1);
    m_drawable_height = std::max(pixels_tall, 1);

    m_pixels_per_unit = m_height_in_units > 0.0f
                            ? std::max(1.0f, static_cast<float>(m_drawable_height) / m_height_in_units)
                            : 1.0f;

    m_width = static_cast<int>(std::lround(static_cast<float>(m_drawable_width) / m_pixels_per_unit));
    m_height = static_cast<int>(std::lround(static_cast<float>(m_drawable_height) / m_pixels_per_unit));

    // The window's own size, which is what SDL reports fingers against, may
    // not be its drawable's; the handler is told the ratio it needs.
    int window_wide = 0;
    int window_tall = 0;
    SDL_GetWindowSize(m_handle, &window_wide, &window_tall);

    const float window_to_drawable = window_tall > 0 ? static_cast<float>(m_drawable_height) / window_tall : 1.0f;
    m_input_handler->set_pixels_per_unit(m_pixels_per_unit / window_to_drawable);
}

void window_sdl::pump()
{
    SDL_Event event;

    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
            case SDL_QUIT:
                m_closed = true;
                break;

            case SDL_APP_WILLENTERBACKGROUND:
            case SDL_APP_DIDENTERBACKGROUND:
                m_background = true;
                break;

            case SDL_APP_DIDENTERFOREGROUND:
                m_background = false;
                measure();
                break;

            // SDL could not get the drawing context back after the program
            // was away, and has made a new, empty one: every texture, mesh
            // and shader the program had is gone.
            case SDL_RENDER_DEVICE_RESET:
                m_context_lost = true;
                break;

            case SDL_WINDOWEVENT:
                if(event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED
                   || event.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    measure();
                }

                break;

            default:
                break;
        }

        m_input_handler->handle(event);

        if(m_on_event)
        {
            m_on_event(event);
        }
    }
}

void window_sdl::display()
{
    double last_frame = seconds();

    while(!m_closed)
    {
        const double now = seconds();
        const float delta_time = static_cast<float>(now - last_frame);
        last_frame = now;

        pump();

        if(update())
        {
            update()(delta_time);
        }

        // Out of sight there may be nothing to draw on at all. The world goes
        // on; it is only not drawn, and slowly.
        if(m_background)
        {
            m_input_handler->end_frame(delta_time);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if(render_3d())
        {
            render_3d()();
        }

        if(render_ui())
        {
            render_ui()();
        }

        m_input_handler->poll_keyboard_input();
        m_input_handler->poll_mouse_input();
        m_input_handler->end_frame(delta_time);

        SDL_GL_SwapWindow(m_handle);

        pace(now);
    }
}

void window_sdl::present_frame()
{
    if(!m_background)
    {
        SDL_GL_SwapWindow(m_handle);
    }

    pump();
}

double window_sdl::seconds() const
{
    return static_cast<double>(SDL_GetPerformanceCounter() - m_started)
         / static_cast<double>(SDL_GetPerformanceFrequency());
}

void window_sdl::close()
{
    m_closed = true;
}

void window_sdl::set_mode(window_mode)
{
}

window_mode window_sdl::mode() const
{
    return m_mode;
}

void window_sdl::set_cursor_visibility(bool visible)
{
    m_cursor_visible = visible;
}

bool window_sdl::cursor_visibility()
{
    return m_cursor_visible;
}

void window_sdl::set_frame_limit(int fps, int)
{
    m_frame_limit = fps > 0 ? fps : 0;
}

void window_sdl::set_vsync(bool on)
{
    SDL_GL_SetSwapInterval(on ? 1 : 0);
}

int window_sdl::refresh_rate() const
{
    SDL_DisplayMode mode;

    if(SDL_GetCurrentDisplayMode(std::max(SDL_GetWindowDisplayIndex(m_handle), 0), &mode) == 0
       && mode.refresh_rate > 0)
    {
        return mode.refresh_rate;
    }

    return 60;
}

void window_sdl::set_height_in_units(float units)
{
    m_height_in_units = units > 0.0f ? units : 0.0f;
    measure();
}

float window_sdl::pixels_per_unit() const
{
    return m_pixels_per_unit;
}

float window_sdl::inches_tall() const
{
    float vertical_dpi = 0.0f;

    if(SDL_GetDisplayDPI(std::max(SDL_GetWindowDisplayIndex(m_handle), 0), nullptr, nullptr, &vertical_dpi) != 0
       || vertical_dpi <= 0.0f)
    {
        return 0.0f;
    }

    return static_cast<float>(m_drawable_height) / vertical_dpi;
}

void window_sdl::pace(double frame_started)
{
    if(m_frame_limit <= 0)
    {
        return;
    }

    const double frame = 1.0 / static_cast<double>(m_frame_limit);

    for(;;)
    {
        const double left = frame - (seconds() - frame_started);

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

ref<input_handler_sdl> window_sdl::input_handler()
{
    return m_input_handler;
}

SDL_Window* window_sdl::handle() const
{
    return m_handle;
}

std::function<void(const SDL_Event&)>& window_sdl::on_event()
{
    return m_on_event;
}

bool window_sdl::in_background() const
{
    return m_background;
}

} // namespace nle

#endif // NLE_SDL_WINDOW

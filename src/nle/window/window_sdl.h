/**
 * @file window_sdl.h
 * @brief A window, an OpenGL ES 3 context and input, from SDL: for a phone.
 *
 * The same face as window_glfw, so a game built for one builds for the other.
 * A phone's window is the whole screen and always is; it comes and goes as
 * the player switches away, and while it is away nothing is drawn but the
 * game goes on, so a connection is not dropped for a glance at a message.
 */

#pragma once

#include <SDL.h>

#include <functional>
#include <string>

#include "window.hpp"
#include "input_handler_sdl.h"
#include "../core/ref.h"

namespace nle
{

class window_sdl : public window<SDL_Window*>
{
public:
    window_sdl(unsigned int width, unsigned int height, const std::string& title = NLE_WINDOW_DEFAULT_TITLE);
    ~window_sdl();

    void display();
    void close();

    /// A phone's window is the screen, however it is asked for.
    void set_mode(window_mode mode) override;
    window_mode mode() const override;

    /// There is no cursor to show; a mouse plugged in keeps its own.
    void set_cursor_visibility(bool visible) override;
    bool cursor_visibility() override;

    void present_frame() override;
    double seconds() const override;

    /// As window_glfw's: the most frames a second, nought for no limit.
    void set_frame_limit(int fps, int unfocused = 20);
    void set_vsync(bool on);
    int refresh_rate() const;

    /**
     * @brief How tall the window is in its own units, nought for its pixels.
     *
     * What makes an interface laid out for a desktop the right size on a
     * phone. Taken as asked, unless the screen has fewer pixels than that.
     */
    void set_height_in_units(float units);

    /// Screen pixels to one of the window's units.
    float pixels_per_unit() const;

    /// How tall the screen is, in inches, or nought if it will not say.
    float inches_tall() const;

    ref<input_handler_sdl> input_handler();

    SDL_Window* handle() const;

    /// Everything SDL reports, for anything else that wants it: the
    /// interface, which takes text from it.
    std::function<void(const SDL_Event&)>& on_event();

    /// Whether the game is out of sight, switched away from.
    bool in_background() const;

    /**
     * @brief Whether everything drawn with has been lost: the phone took the
     *        drawing context away while the program was in the background,
     *        and what it has now is a new one with nothing in it.
     *
     * Nothing made before can be drawn again. A program either loads
     * everything a second time or starts over.
     */
    bool context_lost() const { return m_context_lost; }

private:
    SDL_GLContext m_context = nullptr;
    ref<input_handler_sdl> m_input_handler;
    std::function<void(const SDL_Event&)> m_on_event;

    int m_frame_limit = 0;
    float m_height_in_units = 0.0f;
    float m_pixels_per_unit = 1.0f;
    bool m_background = false;
    bool m_context_lost = false;
    uint64_t m_started = 0;

    /// What SDL has to say since last asked.
    void pump();

    /// The screen's size, and so the window's in its units.
    void measure();

    void pace(double frame_started);
};

} // namespace nle

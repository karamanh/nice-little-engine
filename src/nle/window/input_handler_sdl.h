/**
 * @file input_handler_sdl.h
 * @brief Keys, the pointer and fingers, from SDL: what a phone has.
 *
 * Keys arrive as GLFW's numbers (see keys.h), so a game reads them the same
 * way whichever window system is underneath.
 *
 * A touch screen has no mouse. SDL is told not to make one up out of the
 * first finger -- the first finger down is as likely to be a thumb on a move
 * stick as anything meant as a click -- and the fingers are handed over as
 * they are. The pointer is then whatever the game says it is, through
 * point() and press(): a game decides which finger is the one clicking and
 * where. A real mouse, plugged into a tablet, moves the pointer itself.
 */

#pragma once

#include "input_handler.hpp"
#include "keys.h"

#include <SDL.h>

#include <cstdint>
#include <vector>

namespace nle
{

class input_handler_sdl : public input_handler<SDL_Window*>
{
public:
    /// The keys, by name. See keys.h.
    using key = ::nle::key;

    /// A finger on the screen, in the same units as the pointer.
    struct finger
    {
        int64_t id = 0;

        float x = 0.0f;
        float y = 0.0f;

        /// Where it came down.
        float start_x = 0.0f;
        float start_y = 0.0f;

        /// Seconds since it came down.
        float held = 0.0f;

        /// Came down this frame, or lifted this frame. A finger that lifted
        /// is kept for the one frame it is reported lifted, then forgotten.
        bool began = false;
        bool ended = false;
    };

    input_handler_sdl(SDL_Window* handle);

    /// Everything SDL says about input, as it says it. The window calls this.
    void handle(const SDL_Event& event);

    /// Every finger on the screen, and those that left it this frame.
    const std::vector<finger>& fingers() const;

    /// Whether anything has touched the screen since the program started --
    /// whether this is being played by hand rather than with a mouse.
    bool touched() const;

    /// Moves the pointer, in the window's units.
    void point(double x, double y);

    /// Holds a pointer button down or lets it go.
    void press(int button, bool down);

    /// Where the pointer is now, and what is held: as set, before the poll
    /// that hands them on.
    double pointer_x() const { return m_pointer_x; }
    double pointer_y() const { return m_pointer_y; }
    bool pointer_down(int button) const
    {
        return button >= 0 && static_cast<size_t>(button) < m_pressed.size() && m_pressed[static_cast<size_t>(button)];
    }

    /**
     * @brief The window's units per pixel of the screen, inverted: how many
     *        screen pixels to one unit.
     *
     * Fingers and the mouse are reported in pixels and handed on in units.
     */
    void set_pixels_per_unit(float scale);

    /// Called once a frame, after everything that frame has read the input:
    /// fingers that lifted are dropped, and none of them is new any more.
    void end_frame(float delta_time);

    void poll_mouse_input() override;
    void poll_keyboard_input() override;

    gamepad pad() const override;

private:
    std::vector<finger> m_fingers;
    bool m_touched = false;

    /// Held keys as SDL has said them, applied on the next poll.
    std::array<bool, 1024> m_held{};

    /// Keys that went down since the last poll, held or not: one pressed and
    /// let go between two frames -- Back, a quick tap -- is still a press.
    std::array<bool, 1024> m_went_down{};

    double m_pointer_x = 0.0;
    double m_pointer_y = 0.0;
    std::array<bool, 8> m_pressed{};

    float m_pixels_per_unit = 1.0f;

    /// The controller being read, once one has been plugged in.
    SDL_GameController* m_pad = nullptr;

    finger* find(int64_t id);
};

/// GLFW's number for an SDL scancode, or GLFW_KEY_UNKNOWN.
int key_from_sdl(SDL_Scancode code);

} // namespace nle

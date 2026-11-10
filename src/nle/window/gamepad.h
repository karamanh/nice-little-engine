/**
 * @file gamepad.h
 * @brief A controller's sticks, triggers and buttons, as they are now.
 *
 * One layout whichever window system reads it: the one both GLFW and SDL
 * already map every controller they know onto, named for where a button is
 * rather than for what is printed on it.
 */

#pragma once

#include <array>
#include <cstddef>

namespace nle
{

struct gamepad
{
    enum button : size_t
    {
        a, b, x, y,
        left_shoulder, right_shoulder,
        back, start, guide,
        left_stick, right_stick,
        dpad_up, dpad_right, dpad_down, dpad_left,
        count
    };

    /// Whether there is one at all. Everything below is at rest when not.
    bool connected = false;

    /// The sticks, from -1 to 1: right and down are positive.
    float left_x = 0.0f;
    float left_y = 0.0f;
    float right_x = 0.0f;
    float right_y = 0.0f;

    /// The triggers, from 0 at rest to 1 pulled in.
    float left_trigger = 0.0f;
    float right_trigger = 0.0f;

    std::array<bool, count> held{};
};

} // namespace nle

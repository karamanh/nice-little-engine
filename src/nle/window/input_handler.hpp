/**
 * @file input_handler.hpp
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief 
 * @version 0.1
 * @date 2024-08-19
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#pragma once

#include <array>

#include "../core/signal.hpp"
#include "gamepad.h"

namespace nle
{

template <typename winhandle_type>
class input_handler
{
protected:
    winhandle_type m_handle;

    bool m_mouse_moved_once = false;
    bool m_mouse_cam_lock = false;
    double m_mouse_last_x = 0.0;
    double m_mouse_last_y = 0.0;
    double m_mouse_delta_x = 0.0;
    double m_mouse_delta_y = 0.0;
    double m_input_timestamp = 0.0;
    
    std::array<bool, 1024> m_keys{};
    std::array<bool, 8> m_mouse_buttons{};

    void set_key_state(int key, bool state)
    {
        if(state)
        {
            if(m_keys[key] != state)
            {
                m_keys[key] = state;
                sig_key_just_pressed.emit(key, false);
            }
            else
            {
                sig_key_pressed.emit(key, false);
            }
        }
        else
        {
            if(m_keys[key] != state)
            {
                m_keys[key] = state;
                sig_key_released.emit(key, false);
            }
        }
    }

    void set_mouse_position(double x, double y)
    {
        if(!m_mouse_moved_once)
        {
            m_mouse_last_x = x;
            m_mouse_last_y = y;
            m_mouse_moved_once = true;
            return;
        }

        m_mouse_delta_x = x - m_mouse_last_x;
        m_mouse_delta_y = m_mouse_last_y - y;

        m_mouse_last_x = x;
        m_mouse_last_y = y;

        sig_mouse_moved.emit(m_mouse_delta_x, m_mouse_delta_y, false);
    }

    void set_mouse_button_state(int button, bool state)
    {
        // Edges only, as the signal's name says and as set_key_state() above
        // already does. The buttons are polled every frame, so emitting
        // unconditionally made this fire continuously for every button in
        // whatever state it was in -- including the ones nobody was touching.
        // A handler reading it as "the button just went down" then ran on
        // every frame the button was up.
        if(m_mouse_buttons[button] == state)
        {
            return;
        }

        m_mouse_buttons[button] = state;
        sig_mouse_state_changed.emit(button, state, m_mouse_last_x, m_mouse_last_y, false);
    }

    virtual void poll_keyboard_input() = 0;

    virtual void poll_mouse_input() = 0;

public:
    signal<int> sig_key_pressed;
    signal<int> sig_key_just_pressed;
    signal<int> sig_key_released;
    signal<double, double> sig_mouse_moved;
    signal<int, bool, double, double> sig_mouse_state_changed;

    input_handler(winhandle_type handle)
        : m_handle(handle)
    {}

    virtual ~input_handler() = default;

    /**
     * @brief True while @p key is held down.
     *
     * Polling beats the signals for continuous input such as movement: the
     * signals report edges and repeats, so reconstructing "is it held" from
     * them means every caller keeping its own copy of the keyboard.
     */
    bool key_down(int key) const
    {
        return key >= 0 && static_cast<size_t>(key) < m_keys.size() && m_keys[static_cast<size_t>(key)];
    }

    /// Cursor position in pixels, origin at the top left of the window.
    double mouse_x() const { return m_mouse_last_x; }
    double mouse_y() const { return m_mouse_last_y; }

    bool mouse_button_down(int button) const
    {
        return button >= 0 && static_cast<size_t>(button) < m_mouse_buttons.size()
            && m_mouse_buttons[static_cast<size_t>(button)];
    }

    /// The first controller plugged in, as it is this moment; not connected
    /// when there is none.
    virtual gamepad pad() const
    {
        return gamepad();
    }
};

} // namespace nle

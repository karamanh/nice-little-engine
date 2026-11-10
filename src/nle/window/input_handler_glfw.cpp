#include "input_handler_glfw.h"

namespace nle
{
    input_handler_glfw::input_handler_glfw(GLFWwindow *handle)
        : input_handler(handle)
    {
    }

    void input_handler_glfw::poll_mouse_input()
    {
        if(!m_handle)
        {
            return;
        }

        double x, y;
        glfwGetCursorPos(m_handle, &x, &y);
        set_mouse_position(x, y);

        for(size_t i = 0; i < m_mouse_buttons.size(); i++)
        {
            int state = glfwGetMouseButton(m_handle, i);
            set_mouse_button_state(i, static_cast<bool>(state));
        }
    }

    void input_handler_glfw::poll_keyboard_input()
    {
        if(!m_handle)
        {
            return;
        }
        
        for(size_t i = 0; i < m_keys.size(); i++)
        {
            int state = glfwGetKey(m_handle, i);
            set_key_state(i, static_cast<bool>(state));
        }
    }

    gamepad input_handler_glfw::pad() const
    {
        gamepad state;

        for(int id = GLFW_JOYSTICK_1; id <= GLFW_JOYSTICK_LAST; ++id)
        {
            GLFWgamepadstate read;

            if(!glfwJoystickIsGamepad(id) || !glfwGetGamepadState(id, &read))
            {
                continue;
            }

            state.connected = true;

            state.left_x = read.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
            state.left_y = read.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
            state.right_x = read.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
            state.right_y = read.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];

            // GLFW's triggers rest at -1.
            state.left_trigger = (read.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] + 1.0f) * 0.5f;
            state.right_trigger = (read.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1.0f) * 0.5f;

            // The same order as gamepad::button, which was taken from here.
            for(size_t i = 0; i < gamepad::count; ++i)
            {
                state.held[i] = read.buttons[i] == GLFW_PRESS;
            }

            break;
        }

        return state;
    }
} // namespace nle

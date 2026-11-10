#ifdef NLE_SDL_WINDOW

#include "input_handler_sdl.h"

#include <algorithm>

namespace nle
{

input_handler_sdl::input_handler_sdl(SDL_Window* handle)
    : input_handler(handle)
{
}

input_handler_sdl::finger* input_handler_sdl::find(int64_t id)
{
    for(finger& one : m_fingers)
    {
        if(one.id == id && !one.ended)
        {
            return &one;
        }
    }

    return nullptr;
}

void input_handler_sdl::handle(const SDL_Event& event)
{
    int width = 1;
    int height = 1;

    if(m_handle != nullptr)
    {
        SDL_GetWindowSize(m_handle, &width, &height);
    }

    switch(event.type)
    {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            const int code = key_from_sdl(event.key.keysym.scancode);

            if(code >= 0 && static_cast<size_t>(code) < m_held.size())
            {
                m_held[static_cast<size_t>(code)] = event.type == SDL_KEYDOWN;

                if(event.type == SDL_KEYDOWN)
                {
                    m_went_down[static_cast<size_t>(code)] = true;

                }
            }

            break;
        }

        // Fingers come in as fractions of the window.
        case SDL_FINGERDOWN:
        {
            finger made;
            made.id = static_cast<int64_t>(event.tfinger.fingerId);
            made.x = made.start_x = event.tfinger.x * static_cast<float>(width) / m_pixels_per_unit;
            made.y = made.start_y = event.tfinger.y * static_cast<float>(height) / m_pixels_per_unit;
            made.began = true;

            m_fingers.push_back(made);
            m_touched = true;
            break;
        }

        case SDL_FINGERMOTION:
        case SDL_FINGERUP:
        {
            finger* one = find(static_cast<int64_t>(event.tfinger.fingerId));

            if(one == nullptr)
            {
                break;
            }

            one->x = event.tfinger.x * static_cast<float>(width) / m_pixels_per_unit;
            one->y = event.tfinger.y * static_cast<float>(height) / m_pixels_per_unit;

            if(event.type == SDL_FINGERUP)
            {
                one->ended = true;
            }

            break;
        }

        // A real mouse. Fingers do not make these: see the hint the window
        // sets.
        case SDL_MOUSEMOTION:
        {
            if(event.motion.which != SDL_TOUCH_MOUSEID)
            {
                point(event.motion.x / m_pixels_per_unit, event.motion.y / m_pixels_per_unit);
            }

            break;
        }

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            if(event.button.which == SDL_TOUCH_MOUSEID)
            {
                break;
            }

            // GLFW's order: left, right, middle.
            const int button = event.button.button == SDL_BUTTON_LEFT    ? 0
                             : event.button.button == SDL_BUTTON_RIGHT   ? 1
                             : event.button.button == SDL_BUTTON_MIDDLE  ? 2
                                                                         : event.button.button - 1;

            point(event.button.x / m_pixels_per_unit, event.button.y / m_pixels_per_unit);
            press(button, event.type == SDL_MOUSEBUTTONDOWN);
            break;
        }

        // The first controller plugged in is the one played with; another
        // takes its place only once it has gone.
        case SDL_CONTROLLERDEVICEADDED:
            if(m_pad == nullptr)
            {
                m_pad = SDL_GameControllerOpen(event.cdevice.which);
            }

            break;

        case SDL_CONTROLLERDEVICEREMOVED:
            if(m_pad != nullptr
               && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_pad)) == event.cdevice.which)
            {
                SDL_GameControllerClose(m_pad);
                m_pad = nullptr;
            }

            break;

        default:
            break;
    }
}

gamepad input_handler_sdl::pad() const
{
    gamepad state;

    if(m_pad == nullptr || !SDL_GameControllerGetAttached(m_pad))
    {
        return state;
    }

    auto axis = [&](SDL_GameControllerAxis which) {
        return std::clamp(static_cast<float>(SDL_GameControllerGetAxis(m_pad, which)) / 32767.0f, -1.0f, 1.0f);
    };

    auto held = [&](SDL_GameControllerButton which) {
        return SDL_GameControllerGetButton(m_pad, which) != 0;
    };

    state.connected = true;

    state.left_x = axis(SDL_CONTROLLER_AXIS_LEFTX);
    state.left_y = axis(SDL_CONTROLLER_AXIS_LEFTY);
    state.right_x = axis(SDL_CONTROLLER_AXIS_RIGHTX);
    state.right_y = axis(SDL_CONTROLLER_AXIS_RIGHTY);
    state.left_trigger = std::max(0.0f, axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT));
    state.right_trigger = std::max(0.0f, axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT));

    state.held[gamepad::a] = held(SDL_CONTROLLER_BUTTON_A);
    state.held[gamepad::b] = held(SDL_CONTROLLER_BUTTON_B);
    state.held[gamepad::x] = held(SDL_CONTROLLER_BUTTON_X);
    state.held[gamepad::y] = held(SDL_CONTROLLER_BUTTON_Y);
    state.held[gamepad::left_shoulder] = held(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    state.held[gamepad::right_shoulder] = held(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    state.held[gamepad::back] = held(SDL_CONTROLLER_BUTTON_BACK);
    state.held[gamepad::start] = held(SDL_CONTROLLER_BUTTON_START);
    state.held[gamepad::guide] = held(SDL_CONTROLLER_BUTTON_GUIDE);
    state.held[gamepad::left_stick] = held(SDL_CONTROLLER_BUTTON_LEFTSTICK);
    state.held[gamepad::right_stick] = held(SDL_CONTROLLER_BUTTON_RIGHTSTICK);
    state.held[gamepad::dpad_up] = held(SDL_CONTROLLER_BUTTON_DPAD_UP);
    state.held[gamepad::dpad_right] = held(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    state.held[gamepad::dpad_down] = held(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    state.held[gamepad::dpad_left] = held(SDL_CONTROLLER_BUTTON_DPAD_LEFT);

    return state;
}

const std::vector<input_handler_sdl::finger>& input_handler_sdl::fingers() const
{
    return m_fingers;
}

bool input_handler_sdl::touched() const
{
    return m_touched;
}

void input_handler_sdl::point(double x, double y)
{
    m_pointer_x = x;
    m_pointer_y = y;
}

void input_handler_sdl::press(int button, bool down)
{
    if(button >= 0 && static_cast<size_t>(button) < m_pressed.size())
    {
        m_pressed[static_cast<size_t>(button)] = down;
    }
}

void input_handler_sdl::set_pixels_per_unit(float scale)
{
    m_pixels_per_unit = scale > 0.0f ? scale : 1.0f;
}

void input_handler_sdl::end_frame(float delta_time)
{
    m_fingers.erase(std::remove_if(m_fingers.begin(), m_fingers.end(),
                                   [](const finger& one) { return one.ended; }),
                    m_fingers.end());

    for(finger& one : m_fingers)
    {
        one.began = false;
        one.held += delta_time;
    }
}

void input_handler_sdl::poll_mouse_input()
{
    set_mouse_position(m_pointer_x, m_pointer_y);

    for(size_t i = 0; i < m_mouse_buttons.size(); ++i)
    {
        set_mouse_button_state(static_cast<int>(i), m_pressed[i]);
    }
}

void input_handler_sdl::poll_keyboard_input()
{
    for(size_t i = 0; i < m_keys.size(); ++i)
    {
        set_key_state(static_cast<int>(i), m_held[i] || m_went_down[i]);
        m_went_down[i] = false;
    }
}

int key_from_sdl(SDL_Scancode code)
{
    if(code >= SDL_SCANCODE_A && code <= SDL_SCANCODE_Z)
    {
        return GLFW_KEY_A + (code - SDL_SCANCODE_A);
    }

    // SDL counts 1 to 9 and then 0.
    if(code >= SDL_SCANCODE_1 && code <= SDL_SCANCODE_9)
    {
        return GLFW_KEY_1 + (code - SDL_SCANCODE_1);
    }

    if(code >= SDL_SCANCODE_F1 && code <= SDL_SCANCODE_F12)
    {
        return GLFW_KEY_F1 + (code - SDL_SCANCODE_F1);
    }

    if(code >= SDL_SCANCODE_KP_1 && code <= SDL_SCANCODE_KP_9)
    {
        return GLFW_KEY_KP_1 + (code - SDL_SCANCODE_KP_1);
    }

    switch(code)
    {
        case SDL_SCANCODE_0:            return GLFW_KEY_0;
        case SDL_SCANCODE_KP_0:         return GLFW_KEY_KP_0;
        case SDL_SCANCODE_SPACE:        return GLFW_KEY_SPACE;
        case SDL_SCANCODE_APOSTROPHE:   return GLFW_KEY_APOSTROPHE;
        case SDL_SCANCODE_COMMA:        return GLFW_KEY_COMMA;
        case SDL_SCANCODE_MINUS:        return GLFW_KEY_MINUS;
        case SDL_SCANCODE_PERIOD:       return GLFW_KEY_PERIOD;
        case SDL_SCANCODE_SLASH:        return GLFW_KEY_SLASH;
        case SDL_SCANCODE_SEMICOLON:    return GLFW_KEY_SEMICOLON;
        case SDL_SCANCODE_EQUALS:       return GLFW_KEY_EQUAL;
        case SDL_SCANCODE_LEFTBRACKET:  return GLFW_KEY_LEFT_BRACKET;
        case SDL_SCANCODE_BACKSLASH:    return GLFW_KEY_BACKSLASH;
        case SDL_SCANCODE_RIGHTBRACKET: return GLFW_KEY_RIGHT_BRACKET;
        case SDL_SCANCODE_GRAVE:        return GLFW_KEY_GRAVE_ACCENT;
        case SDL_SCANCODE_ESCAPE:       return GLFW_KEY_ESCAPE;

        // A phone's back is the way out of whatever is open, which is what
        // Escape is.
        case SDL_SCANCODE_AC_BACK:      return GLFW_KEY_ESCAPE;

        case SDL_SCANCODE_RETURN:       return GLFW_KEY_ENTER;
        case SDL_SCANCODE_TAB:          return GLFW_KEY_TAB;
        case SDL_SCANCODE_BACKSPACE:    return GLFW_KEY_BACKSPACE;
        case SDL_SCANCODE_INSERT:       return GLFW_KEY_INSERT;
        case SDL_SCANCODE_DELETE:       return GLFW_KEY_DELETE;
        case SDL_SCANCODE_RIGHT:        return GLFW_KEY_RIGHT;
        case SDL_SCANCODE_LEFT:         return GLFW_KEY_LEFT;
        case SDL_SCANCODE_DOWN:         return GLFW_KEY_DOWN;
        case SDL_SCANCODE_UP:           return GLFW_KEY_UP;
        case SDL_SCANCODE_PAGEUP:       return GLFW_KEY_PAGE_UP;
        case SDL_SCANCODE_PAGEDOWN:     return GLFW_KEY_PAGE_DOWN;
        case SDL_SCANCODE_HOME:         return GLFW_KEY_HOME;
        case SDL_SCANCODE_END:          return GLFW_KEY_END;
        case SDL_SCANCODE_CAPSLOCK:     return GLFW_KEY_CAPS_LOCK;
        case SDL_SCANCODE_KP_PERIOD:    return GLFW_KEY_KP_DECIMAL;
        case SDL_SCANCODE_KP_DIVIDE:    return GLFW_KEY_KP_DIVIDE;
        case SDL_SCANCODE_KP_MULTIPLY:  return GLFW_KEY_KP_MULTIPLY;
        case SDL_SCANCODE_KP_MINUS:     return GLFW_KEY_KP_SUBTRACT;
        case SDL_SCANCODE_KP_PLUS:      return GLFW_KEY_KP_ADD;
        case SDL_SCANCODE_KP_ENTER:     return GLFW_KEY_KP_ENTER;
        case SDL_SCANCODE_LSHIFT:       return GLFW_KEY_LEFT_SHIFT;
        case SDL_SCANCODE_LCTRL:        return GLFW_KEY_LEFT_CONTROL;
        case SDL_SCANCODE_LALT:         return GLFW_KEY_LEFT_ALT;
        case SDL_SCANCODE_RSHIFT:       return GLFW_KEY_RIGHT_SHIFT;
        case SDL_SCANCODE_RCTRL:        return GLFW_KEY_RIGHT_CONTROL;
        case SDL_SCANCODE_RALT:         return GLFW_KEY_RIGHT_ALT;
        default:                        return GLFW_KEY_UNKNOWN;
    }
}

} // namespace nle

#endif // NLE_SDL_WINDOW

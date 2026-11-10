/**
 * @file input_handler_glfw.h
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief 
 * @version 0.1
 * @date 2024-08-19
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#pragma once

#include "input_handler.hpp"
#include "keys.h"

#include <GLFW/glfw3.h>

namespace nle
{

class input_handler_glfw : public input_handler<GLFWwindow*>
{
public:
    /// The keys, by name. See keys.h.
    using key = ::nle::key;

    input_handler_glfw(GLFWwindow *handle);
    
    void poll_mouse_input() override;

    void poll_keyboard_input() override;

    gamepad pad() const override;
};

} // namespace nle

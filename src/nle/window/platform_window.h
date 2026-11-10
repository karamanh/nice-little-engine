/**
 * @file platform_window.h
 * @brief The window and input this build has: GLFW on a desktop, SDL on a
 *        phone (NLE_SDL_WINDOW). Both have the same face, so code written
 *        against these names builds for either.
 */

#pragma once

#ifdef NLE_SDL_WINDOW
#include "window_sdl.h"

namespace nle
{
using platform_window = window_sdl;
using platform_input = input_handler_sdl;
}
#else
#include "window_glfw.h"

namespace nle
{
using platform_window = window_glfw;
using platform_input = input_handler_glfw;
}
#endif

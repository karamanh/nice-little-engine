/**
 * @file keys.h
 * @brief The engine's key and mouse button numbers.
 *
 * GLFW's numbers, whichever window system is underneath: they are what a
 * game keeps in its settings when a player chooses a key, so a build on
 * another window system has to mean the same key by the same number. With
 * GLFW they come from GLFW itself; without it, from here, with GLFW's names
 * and values.
 */

#pragma once

#ifndef NLE_SDL_WINDOW
#include <GLFW/glfw3.h>
#else
#define GLFW_KEY_UNKNOWN             -1
#define GLFW_KEY_SPACE               32
#define GLFW_KEY_APOSTROPHE          39
#define GLFW_KEY_COMMA               44
#define GLFW_KEY_MINUS               45
#define GLFW_KEY_PERIOD              46
#define GLFW_KEY_SLASH               47
#define GLFW_KEY_0                   48
#define GLFW_KEY_1                   49
#define GLFW_KEY_2                   50
#define GLFW_KEY_3                   51
#define GLFW_KEY_4                   52
#define GLFW_KEY_5                   53
#define GLFW_KEY_6                   54
#define GLFW_KEY_7                   55
#define GLFW_KEY_8                   56
#define GLFW_KEY_9                   57
#define GLFW_KEY_SEMICOLON           59
#define GLFW_KEY_EQUAL               61
#define GLFW_KEY_A                   65
#define GLFW_KEY_B                   66
#define GLFW_KEY_C                   67
#define GLFW_KEY_D                   68
#define GLFW_KEY_E                   69
#define GLFW_KEY_F                   70
#define GLFW_KEY_G                   71
#define GLFW_KEY_H                   72
#define GLFW_KEY_I                   73
#define GLFW_KEY_J                   74
#define GLFW_KEY_K                   75
#define GLFW_KEY_L                   76
#define GLFW_KEY_M                   77
#define GLFW_KEY_N                   78
#define GLFW_KEY_O                   79
#define GLFW_KEY_P                   80
#define GLFW_KEY_Q                   81
#define GLFW_KEY_R                   82
#define GLFW_KEY_S                   83
#define GLFW_KEY_T                   84
#define GLFW_KEY_U                   85
#define GLFW_KEY_V                   86
#define GLFW_KEY_W                   87
#define GLFW_KEY_X                   88
#define GLFW_KEY_Y                   89
#define GLFW_KEY_Z                   90
#define GLFW_KEY_LEFT_BRACKET        91
#define GLFW_KEY_BACKSLASH           92
#define GLFW_KEY_RIGHT_BRACKET       93
#define GLFW_KEY_GRAVE_ACCENT        96
#define GLFW_KEY_WORLD_1             161
#define GLFW_KEY_WORLD_2             162
#define GLFW_KEY_ESCAPE              256
#define GLFW_KEY_ENTER               257
#define GLFW_KEY_TAB                 258
#define GLFW_KEY_BACKSPACE           259
#define GLFW_KEY_INSERT              260
#define GLFW_KEY_DELETE              261
#define GLFW_KEY_RIGHT               262
#define GLFW_KEY_LEFT                263
#define GLFW_KEY_DOWN                264
#define GLFW_KEY_UP                  265
#define GLFW_KEY_PAGE_UP             266
#define GLFW_KEY_PAGE_DOWN           267
#define GLFW_KEY_HOME                268
#define GLFW_KEY_END                 269
#define GLFW_KEY_CAPS_LOCK           280
#define GLFW_KEY_SCROLL_LOCK         281
#define GLFW_KEY_NUM_LOCK            282
#define GLFW_KEY_PRINT_SCREEN        283
#define GLFW_KEY_PAUSE               284
#define GLFW_KEY_F1                  290
#define GLFW_KEY_F2                  291
#define GLFW_KEY_F3                  292
#define GLFW_KEY_F4                  293
#define GLFW_KEY_F5                  294
#define GLFW_KEY_F6                  295
#define GLFW_KEY_F7                  296
#define GLFW_KEY_F8                  297
#define GLFW_KEY_F9                  298
#define GLFW_KEY_F10                 299
#define GLFW_KEY_F11                 300
#define GLFW_KEY_F12                 301
#define GLFW_KEY_F13                 302
#define GLFW_KEY_F14                 303
#define GLFW_KEY_F15                 304
#define GLFW_KEY_F16                 305
#define GLFW_KEY_F17                 306
#define GLFW_KEY_F18                 307
#define GLFW_KEY_F19                 308
#define GLFW_KEY_F20                 309
#define GLFW_KEY_F21                 310
#define GLFW_KEY_F22                 311
#define GLFW_KEY_F23                 312
#define GLFW_KEY_F24                 313
#define GLFW_KEY_F25                 314
#define GLFW_KEY_KP_0                320
#define GLFW_KEY_KP_1                321
#define GLFW_KEY_KP_2                322
#define GLFW_KEY_KP_3                323
#define GLFW_KEY_KP_4                324
#define GLFW_KEY_KP_5                325
#define GLFW_KEY_KP_6                326
#define GLFW_KEY_KP_7                327
#define GLFW_KEY_KP_8                328
#define GLFW_KEY_KP_9                329
#define GLFW_KEY_KP_DECIMAL          330
#define GLFW_KEY_KP_DIVIDE           331
#define GLFW_KEY_KP_MULTIPLY         332
#define GLFW_KEY_KP_SUBTRACT         333
#define GLFW_KEY_KP_ADD              334
#define GLFW_KEY_KP_ENTER            335
#define GLFW_KEY_KP_EQUAL            336
#define GLFW_KEY_LEFT_SHIFT          340
#define GLFW_KEY_LEFT_CONTROL        341
#define GLFW_KEY_LEFT_ALT            342
#define GLFW_KEY_LEFT_SUPER          343
#define GLFW_KEY_RIGHT_SHIFT         344
#define GLFW_KEY_RIGHT_CONTROL       345
#define GLFW_KEY_RIGHT_ALT           346
#define GLFW_KEY_RIGHT_SUPER         347
#define GLFW_KEY_MENU                348
#define GLFW_KEY_LAST                GLFW_KEY_MENU
#define GLFW_MOUSE_BUTTON_1          0
#define GLFW_MOUSE_BUTTON_2          1
#define GLFW_MOUSE_BUTTON_3          2
#define GLFW_MOUSE_BUTTON_4          3
#define GLFW_MOUSE_BUTTON_5          4
#define GLFW_MOUSE_BUTTON_6          5
#define GLFW_MOUSE_BUTTON_7          6
#define GLFW_MOUSE_BUTTON_8          7
#define GLFW_MOUSE_BUTTON_LAST       GLFW_MOUSE_BUTTON_8
#define GLFW_MOUSE_BUTTON_LEFT       GLFW_MOUSE_BUTTON_1
#define GLFW_MOUSE_BUTTON_RIGHT      GLFW_MOUSE_BUTTON_2
#define GLFW_MOUSE_BUTTON_MIDDLE     GLFW_MOUSE_BUTTON_3
#endif

namespace nle
{

/// The keys by name, numbered as above.
enum class key
{
    unknown = -1,
    space = GLFW_KEY_SPACE,
    apostrophe = GLFW_KEY_APOSTROPHE,
    comma = GLFW_KEY_COMMA,
    minus = GLFW_KEY_MINUS,
    period = GLFW_KEY_PERIOD,
    slash = GLFW_KEY_SLASH,
    num0 = GLFW_KEY_0,
    num1 = GLFW_KEY_1,
    num2 = GLFW_KEY_2,
    num3 = GLFW_KEY_3,
    num4 = GLFW_KEY_4,
    num5 = GLFW_KEY_5,
    num6 = GLFW_KEY_6,
    num7 = GLFW_KEY_7,
    num8 = GLFW_KEY_8,
    num9 = GLFW_KEY_9,
    semicolon = GLFW_KEY_SEMICOLON,
    equal = GLFW_KEY_EQUAL,
    a = GLFW_KEY_A,
    b = GLFW_KEY_B,
    c = GLFW_KEY_C,
    d = GLFW_KEY_D,
    e = GLFW_KEY_E,
    f = GLFW_KEY_F,
    g = GLFW_KEY_G,
    h = GLFW_KEY_H,
    i = GLFW_KEY_I,
    j = GLFW_KEY_J,
    k = GLFW_KEY_K,
    l = GLFW_KEY_L,
    m = GLFW_KEY_M,
    n = GLFW_KEY_N,
    o = GLFW_KEY_O,
    p = GLFW_KEY_P,
    q = GLFW_KEY_Q,
    r = GLFW_KEY_R,
    s = GLFW_KEY_S,
    t = GLFW_KEY_T,
    u = GLFW_KEY_U,
    v = GLFW_KEY_V,
    w = GLFW_KEY_W,
    x = GLFW_KEY_X,
    y = GLFW_KEY_Y,
    z = GLFW_KEY_Z,
    left_bracket = GLFW_KEY_LEFT_BRACKET, // '['
    backslash = GLFW_KEY_BACKSLASH,       // '\'
    right_bracket = GLFW_KEY_RIGHT_BRACKET, // ']'
    grave_accent = GLFW_KEY_GRAVE_ACCENT, // '`'
    world_1 = GLFW_KEY_WORLD_1, // non-US #1
    world_2 = GLFW_KEY_WORLD_2, // non-US #2
    
    escape = GLFW_KEY_ESCAPE,
    enter = GLFW_KEY_ENTER,
    tab = GLFW_KEY_TAB,
    backspace = GLFW_KEY_BACKSPACE,
    insert = GLFW_KEY_INSERT,
    delete_key = GLFW_KEY_DELETE,
    right = GLFW_KEY_RIGHT,
    left = GLFW_KEY_LEFT,
    down = GLFW_KEY_DOWN,
    up = GLFW_KEY_UP,
    page_up = GLFW_KEY_PAGE_UP,
    page_down = GLFW_KEY_PAGE_DOWN,
    home = GLFW_KEY_HOME,
    end = GLFW_KEY_END,
    caps_lock = GLFW_KEY_CAPS_LOCK,
    scroll_lock = GLFW_KEY_SCROLL_LOCK,
    num_lock = GLFW_KEY_NUM_LOCK,
    print_screen = GLFW_KEY_PRINT_SCREEN,
    pause = GLFW_KEY_PAUSE,
    f1 = GLFW_KEY_F1,
    f2 = GLFW_KEY_F2,
    f3 = GLFW_KEY_F3,
    f4 = GLFW_KEY_F4,
    f5 = GLFW_KEY_F5,
    f6 = GLFW_KEY_F6,
    f7 = GLFW_KEY_F7,
    f8 = GLFW_KEY_F8,
    f9 = GLFW_KEY_F9,
    f10 = GLFW_KEY_F10,
    f11 = GLFW_KEY_F11,
    f12 = GLFW_KEY_F12,
    f13 = GLFW_KEY_F13,
    f14 = GLFW_KEY_F14,
    f15 = GLFW_KEY_F15,
    f16 = GLFW_KEY_F16,
    f17 = GLFW_KEY_F17,
    f18 = GLFW_KEY_F18,
    f19 = GLFW_KEY_F19,
    f20 = GLFW_KEY_F20,
    f21 = GLFW_KEY_F21,
    f22 = GLFW_KEY_F22,
    f23 = GLFW_KEY_F23,
    f24 = GLFW_KEY_F24,
    f25 = GLFW_KEY_F25,
    keypad_0 = GLFW_KEY_KP_0,
    keypad_1 = GLFW_KEY_KP_1,
    keypad_2 = GLFW_KEY_KP_2,
    keypad_3 = GLFW_KEY_KP_3,
    keypad_4 = GLFW_KEY_KP_4,
    keypad_5 = GLFW_KEY_KP_5,
    keypad_6 = GLFW_KEY_KP_6,
    keypad_7 = GLFW_KEY_KP_7,
    keypad_8 = GLFW_KEY_KP_8,
    keypad_9 = GLFW_KEY_KP_9,
    keypad_decimal = GLFW_KEY_KP_DECIMAL,
    keypad_divide = GLFW_KEY_KP_DIVIDE,
    keypad_multiply = GLFW_KEY_KP_MULTIPLY,
    keypad_subtract = GLFW_KEY_KP_SUBTRACT,
    keypad_add = GLFW_KEY_KP_ADD,
    keypad_enter = GLFW_KEY_KP_ENTER,
    keypad_equal = GLFW_KEY_KP_EQUAL,
    left_shift = GLFW_KEY_LEFT_SHIFT,
    left_control = GLFW_KEY_LEFT_CONTROL,
    left_alt = GLFW_KEY_LEFT_ALT,
    left_super = GLFW_KEY_LEFT_SUPER,
    right_shift = GLFW_KEY_RIGHT_SHIFT,
    right_control = GLFW_KEY_RIGHT_CONTROL,
    right_alt = GLFW_KEY_RIGHT_ALT,
    right_super = GLFW_KEY_RIGHT_SUPER,
    menu = GLFW_KEY_MENU
};

} // namespace nle

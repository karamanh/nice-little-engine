/**
 * @file nle.h
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief 
 * @version 0.1
 * @date 2024-02-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#pragma once

#include "core/ref.h"
#include "window/platform_window.h"
#include "renderer/renderer_3d.h"
#include "splash.h"

#include <memory>

#include <string>

namespace nle
{

class nle
{
public:
    /**
     * @brief Opens a window and a renderer for it.
     *
     * The default is small on purpose -- it is what a demo wants. A tool with
     * eight panels down its sides wants rather more, and asking for it here
     * rather than resizing afterwards matters: anything that lays itself out
     * relative to the window does so the first time it is drawn, and would
     * lay itself out for the small one.
     */
    nle(unsigned int width = NLE_WINDOW_DEFAULT_WIDTH,
        unsigned int height = NLE_WINDOW_DEFAULT_HEIGHT,
        const std::string& title = NLE_WINDOW_DEFAULT_TITLE);

    /**
     * @brief A renderer for a window that is already open.
     *
     * For a program with something to show before the renderer can be made
     * -- one that has to fetch what the renderer reads first -- and that
     * goes on in the same window afterwards.
     */
    explicit nle(ref<platform_window> window);

    ~nle();

    ref<platform_window> window();

    ref<class renderer_3d> renderer_3d(); 

    /**
     * @brief Changes the card shown at startup, or turns it off.
     *
     * Nought seconds is off. Called before run(), which is when the card is
     * set up -- after that the loop already has it.
     */
    void set_splash(const std::string& picture, float seconds);

    void run();

private:
    ref<platform_window> m_window;
    ref<class renderer_3d> m_renderer;

    std::string m_splash_picture;
    float m_splash_seconds = 2.2f;

    std::unique_ptr<class splash> m_splash;
};

} // namespace nle

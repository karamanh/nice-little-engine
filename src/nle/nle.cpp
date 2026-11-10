#include "nle.h"

#include "renderer/renderer_2d.h"

namespace nle
{

nle::nle(unsigned int width, unsigned int height, const std::string& title)
    : m_splash_picture(std::string(NLE_ASSET_DIR) + "/nle-splash.png")
{
    m_window = make_ref<platform_window>(width, height, title);
    m_renderer = make_ref<class renderer_3d>(m_window);
}

nle::nle(ref<platform_window> window)
    : m_window(window)
    , m_splash_picture(std::string(NLE_ASSET_DIR) + "/nle-splash.png")
{
    m_renderer = make_ref<class renderer_3d>(m_window);
}

nle::~nle()
{
}

ref<platform_window> nle::window()
{
    return m_window;
}

ref<class renderer_3d> nle::renderer_3d()
{
    return m_renderer;
}

void nle::set_splash(const std::string& picture, float seconds)
{
    m_splash_picture = picture;
    m_splash_seconds = seconds;
}

void nle::run()
{
    // The engine's own card, shown without being asked. It is wrapped around
    // whatever the game draws last rather than given a hook of its own,
    // because "on top of everything" is exactly what last means, and a hook
    // is a thing a game can forget to call.
    if(m_splash_seconds > 0.0f)
    {
        auto overlay = make_ref<class renderer_2d>(std::string(NLE_SHADER_DIR) + "/ui_vert.glsl",
                                                   std::string(NLE_SHADER_DIR) + "/ui_frag.glsl");

        m_splash = std::make_unique<class splash>(overlay, m_splash_picture, m_splash_seconds);

        auto theirs = m_window->render_ui();

        m_window->render_ui() = [this, theirs]() {
            if(theirs)
            {
                theirs();
            }

            if(m_splash && !m_splash->finished())
            {
                m_splash->draw(glm::vec2(static_cast<float>(m_window->width()),
                                         static_cast<float>(m_window->height())),
                               m_renderer->delta_time());
            }
        };
    }

    m_window->display();
}

} // namespace nle

#include "imgui_layer.h"

#include "../core/utils.h"

// From the system imgui, which is also the one linked. See the note in
// CMakeLists.txt about vendor/imgui.
#include <imgui.h>
#ifdef NLE_SDL_WINDOW
#include <backends/imgui_impl_sdl2.h>
#else
#include <backends/imgui_impl_glfw.h>
#endif
#include <backends/imgui_impl_opengl3.h>

#include <string>

namespace nle
{

imgui_layer::imgui_layer(ref<platform_window> window, unsigned int glsl_version)
    : m_window(window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

#ifdef NLE_SDL_WINDOW
    (void)glsl_version;

    ImGui_ImplSDL2_InitForOpenGL(m_window->handle(), SDL_GL_GetCurrentContext());
    ImGui_ImplOpenGL3_Init("#version 300 es");

    // Keys, text and the wheel as SDL says them. Not the pointer: that is
    // whatever the game made of the fingers, and is given in begin_frame().
    m_window->on_event() = [](const SDL_Event& event) {
        switch(event.type)
        {
            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_FINGERDOWN:
            case SDL_FINGERUP:
            case SDL_FINGERMOTION:
                return;

            default:
                ImGui_ImplSDL2_ProcessEvent(&event);
        }
    };
#else
    // imgui installs its own GLFW callbacks and chains to any already there,
    // which is what lets it see input without the engine forwarding anything.
    ImGui_ImplGlfw_InitForOpenGL(m_window ? m_window->handle() : nullptr, true);

    const std::string version = "#version " + std::to_string(glsl_version);
    ImGui_ImplOpenGL3_Init(version.c_str());
#endif

    ImGui::StyleColorsDark();
}

imgui_layer::~imgui_layer()
{
    ImGui_ImplOpenGL3_Shutdown();
#ifdef NLE_SDL_WINDOW
    m_window->on_event() = nullptr;
    ImGui_ImplSDL2_Shutdown();
#else
    ImGui_ImplGlfw_Shutdown();
#endif
    ImGui::DestroyContext();
}

void imgui_layer::begin_frame()
{
    if(m_frame_open)
    {
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
#ifdef NLE_SDL_WINDOW
    ImGui_ImplSDL2_NewFrame();

    // Laid out in the window's units and drawn in the screen's pixels.
    ImGuiIO& io = ImGui::GetIO();
    const float scale = m_window->pixels_per_unit();

    io.DisplaySize = ImVec2(static_cast<float>(m_window->width()), static_cast<float>(m_window->height()));
    io.DisplayFramebufferScale = ImVec2(scale, scale);

    const auto input = m_window->input_handler();

    io.AddMouseSourceEvent(input->touched() ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
    io.AddMousePosEvent(static_cast<float>(input->pointer_x()), static_cast<float>(input->pointer_y()));

    for(int button = 0; button < 3; ++button)
    {
        io.AddMouseButtonEvent(button, input->pointer_down(button));
    }
#else
    ImGui_ImplGlfw_NewFrame();
#endif
    ImGui::NewFrame();

    m_frame_open = true;
}

void imgui_layer::end_frame()
{
    if(!m_frame_open)
    {
        return;
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

#ifdef NLE_SDL_WINDOW
    // The phone's keyboard, for as long as something is being typed into.
    const bool typing = ImGui::GetIO().WantTextInput;

    if(typing && !SDL_IsTextInputActive())
    {
        SDL_StartTextInput();
    }
    else if(!typing && SDL_IsTextInputActive())
    {
        SDL_StopTextInput();
    }
#endif

    m_frame_open = false;
}

void imgui_layer::set_layout_file(const std::string& path)
{
    m_layout_file = path;

    // imgui holds the pointer rather than copying it, which is why the string
    // is a member: handing it a temporary is a use-after-free that shows up
    // as a corrupted layout file much later.
    ImGui::GetIO().IniFilename = m_layout_file.empty() ? nullptr : m_layout_file.c_str();
}

void imgui_layer::rebuild_fonts()
{
    // The backend makes its objects again, font texture included, on the
    // next new frame once they are gone.
    ImGui_ImplOpenGL3_DestroyDeviceObjects();
}

bool imgui_layer::frame_open() const
{
    return m_frame_open;
}

bool imgui_layer::wants_mouse() const
{
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
}

bool imgui_layer::wants_keyboard() const
{
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard;
}

bool imgui_layer::wants_text() const
{
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput;
}

} // namespace nle

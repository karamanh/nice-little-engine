# Everything the engine needs, downloaded and built here rather than taken
# from the system -- for a build that has to run on somebody else's machine:
# Windows, cross-compiled from Linux, where there is no system to take them
# from. Every version is pinned, so a build today and a build next month are
# the same program. All of it is linked statically: the game is one .exe.
#
# Provides nle_dependencies, an interface target the engine links.

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

if(NOT NLE_SDL_WINDOW)
    # ---- GLFW: windows and input ---------------------------------------------------
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL)

    # ---- GLEW: OpenGL's functions ----------------------------------------------------
    set(glew-cmake_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(glew-cmake_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(ONLY_LIBS ON CACHE BOOL "" FORCE)

    FetchContent_Declare(glew
        GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
        GIT_TAG glew-cmake-2.2.0
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL)
endif()

# ---- SDL2 and SDL2_mixer: sound, and on a phone the window too ---------------------
# A library of its own on Android, which is what SDL's half in Java loads by
# name before the game's.
if(ANDROID)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.9
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL)

# The music is MP3 and the effects WAV, both decoded by SDL2_mixer itself, so
# every codec that would pull in a library of its own is left out.
set(SDL2MIXER_VENDORED OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_CMD OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_FLAC OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_GME OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_VORBIS "STB" CACHE STRING "" FORCE)
set(SDL2MIXER_MP3 ON CACHE BOOL "" FORCE)
set(SDL2MIXER_MP3_DRMP3 ON CACHE BOOL "" FORCE)
set(SDL2MIXER_MP3_MPG123 OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVE ON CACHE BOOL "" FORCE)

FetchContent_Declare(SDL2_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG release-2.8.0
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL)

# ---- glm: the maths --------------------------------------------------------------
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL)

# ---- ImGui: the interface ---------------------------------------------------------
# 1.90.1, the version the game's interface is written against -- the one a
# working copy on Linux gets from the system. It has no build of its own, so
# its sources are compiled into a library here.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.1
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL)

if(NLE_SDL_WINDOW)
    FetchContent_MakeAvailable(SDL2)

    # The mixer is static and asks for a static SDL by name; on Android SDL is
    # the shared library, and the mixer links that instead.
    if(ANDROID AND NOT TARGET SDL2::SDL2-static)
        add_library(SDL2::SDL2-static ALIAS SDL2)
    endif()

    FetchContent_MakeAvailable(SDL2_mixer glm imgui)
else()
    FetchContent_MakeAvailable(glfw glew SDL2 SDL2_mixer glm imgui)
endif()

add_library(nle_imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp")

target_include_directories(nle_imgui PUBLIC "${imgui_SOURCE_DIR}")

if(NLE_SDL_WINDOW)
    target_sources(nle_imgui PRIVATE "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp")
    target_link_libraries(nle_imgui PUBLIC SDL2::SDL2)
else()
    target_sources(nle_imgui PRIVATE "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp")
    target_link_libraries(nle_imgui PUBLIC glfw)
endif()

# ---- all of it, as one thing to link --------------------------------------------
add_library(nle_dependencies INTERFACE)

if(ANDROID)
    target_link_libraries(nle_dependencies INTERFACE
        glm::glm
        nle_imgui
        SDL2_mixer::SDL2_mixer-static
        SDL2::SDL2
        GLESv3
        EGL
        android
        log)
else()
    target_link_libraries(nle_dependencies INTERFACE
        OpenGL::GL
        glfw
        libglew_static
        glm::glm
        nle_imgui
        SDL2_mixer::SDL2_mixer-static
        SDL2::SDL2-static)
endif()

# The two headers included by the paths a Linux system lays them out at, and
# stb's implementation, which a Linux system supplies as a library.
target_include_directories(nle_dependencies INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/../vendor/include-shim")

# glm 1.0 keeps its extensions behind a flag the older versions a Linux system
# has did not ask for; the engine uses a few of them.
target_compile_definitions(nle_dependencies INTERFACE
    GLM_ENABLE_EXPERIMENTAL
    GLEW_STATIC
    NLE_HAS_AUDIO
    NLE_BUNDLED_STB)

cmake_minimum_required(VERSION 3.21)
project(ImMidiMapper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# FetchContent — all external dependencies
# =============================================================================
include(FetchContent)

# ── Dear ImGui ────────────────────────────────────────────────────────────────
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(imgui)

# ── SDL2 ──────────────────────────────────────────────────────────────────────
FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.9
    GIT_SHALLOW    TRUE)
set(SDL_SHARED  OFF CACHE BOOL "" FORCE)
set(SDL_STATIC  ON  CACHE BOOL "" FORCE)
set(SDL_TEST    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2)

# ── RtMidi ───────────────────────────────────────────────────────────────────
FetchContent_Declare(rtmidi
    GIT_REPOSITORY https://github.com/thestk/rtmidi.git
    GIT_TAG        6.0.0
    GIT_SHALLOW    TRUE)
set(RTMIDI_BUILD_TESTING     OFF CACHE BOOL "" FORCE)
set(RTMIDI_BUILD_STATIC_LIBS ON  CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(rtmidi)

# =============================================================================
# OpenGL
# =============================================================================
find_package(OpenGL REQUIRED)

# =============================================================================
# imgui static library  (SDL2 + OpenGL3 backends)
# =============================================================================
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)

target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)

target_link_libraries(imgui_lib PUBLIC
    SDL2::SDL2-static
    OpenGL::GL)

# macOS: SDL2 needs these frameworks; OpenGL is deprecated but functional
if(APPLE)
    target_link_libraries(imgui_lib PUBLIC
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework CoreFoundation")
endif()

# =============================================================================
# ImMidiMapper  —  header-only interface targets
# =============================================================================

# Core (pure ImGui, no MIDI backend)
add_library(ImMidiMapper INTERFACE)
target_include_directories(ImMidiMapper INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(ImMidiMapper INTERFACE imgui_lib)

# RtMidi backend
add_library(ImMidiMapperRtMidi INTERFACE)
target_include_directories(ImMidiMapperRtMidi INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(ImMidiMapperRtMidi INTERFACE ImMidiMapper rtmidi)

# macOS: RtMidi needs CoreMIDI + CoreAudio
if(APPLE)
    target_link_libraries(ImMidiMapperRtMidi INTERFACE
        "-framework CoreMIDI"
        "-framework CoreAudio"
        "-framework CoreServices")
endif()

# Linux: RtMidi needs ALSA
if(UNIX AND NOT APPLE)
    find_package(ALSA QUIET)
    if(ALSA_FOUND)
        target_link_libraries(ImMidiMapperRtMidi INTERFACE ALSA::ALSA)
    else()
        message(WARNING
            "ALSA not found. Install libasound2-dev (Debian/Ubuntu) "
            "or alsa-lib-devel (Fedora) for MIDI support.")
    endif()
endif()

# =============================================================================
# Example
# =============================================================================
add_subdirectory(example)

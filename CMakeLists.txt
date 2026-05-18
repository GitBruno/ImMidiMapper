cmake_minimum_required(VERSION 3.21)
project(ImMidiMapper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# Platform helpers
# =============================================================================

# Windows / MSYS2-UCRT64: expose system packages to CMake + pkg-config
if(WIN32)
    list(PREPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")
    set(PKG_CONFIG_EXECUTABLE "C:/msys64/ucrt64/bin/pkg-config.exe"
        CACHE FILEPATH "pkg-config executable")
endif()

# =============================================================================
# System dependencies  (SDL2, RtMidi via pkg-config)
# =============================================================================
find_package(OpenGL REQUIRED)
find_package(SDL2   REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(RTMIDI REQUIRED IMPORTED_TARGET rtmidi)

# =============================================================================
# FetchContent — Dear ImGui only
# =============================================================================
include(FetchContent)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(imgui)

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
    ${imgui_SOURCE_DIR}/backends
    ${SDL2_INCLUDE_DIRS})

target_link_libraries(imgui_lib PUBLIC
    SDL2::SDL2
    OpenGL::GL)

if(APPLE)
    target_compile_definitions(imgui_lib PUBLIC GL_SILENCE_DEPRECATION)
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
target_link_libraries(ImMidiMapperRtMidi INTERFACE ImMidiMapper PkgConfig::RTMIDI)

if(APPLE)
    target_link_libraries(ImMidiMapperRtMidi INTERFACE
        "-framework CoreMIDI"
        "-framework CoreAudio"
        "-framework CoreServices")
endif()

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

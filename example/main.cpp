// =============================================================================
// ImMidiMapper — example demo
// SDL2 + OpenGL3 + Dear ImGui + ImMidiMapperRtMidi
// =============================================================================
// Demonstrates both usage patterns:
//   1. Wrapper widgets  (mapper.SliderFloat / mapper.Checkbox)
//   2. watchLast()      (existing ImGui:: widget + mapper.watchLast)
//
// Right-click any labelled control to enter MIDI Learn mode.
// =============================================================================

#define IMMIDIMAPPER_IMPLEMENTATION
#include "../ImMidiMapper.h"
#include "../ImMidiMapperRtMidi.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#  include <SDL_opengles2.h>
#else
#  include <SDL_opengl.h>
#endif

// Silence macOS OpenGL deprecation warnings
#ifdef __APPLE__
#  define GL_SILENCE_DEPRECATION
#endif

#include <cstdio>
#include <cstring>
#include <string>

// =============================================================================
// Simulated synth parameters (stand-ins for real engine params)
// =============================================================================
struct SynthParams {
    float cutoff      = 0.50f;
    float resonance   = 0.40f;
    float envMod      = 0.60f;
    float decay       = 0.45f;
    float accentLevel = 0.50f;
    float volume      = 0.80f;
    float distortion  = 0.00f;
    bool  squareWave  = false;
    int   octave      = 0;       // -1 / 0 / +1
};

// =============================================================================
// Draw the synth control panel using both mapping patterns
// =============================================================================
static void DrawSynthPanel(ImMidiMapperRtMidi& mapper, SynthParams& p)
{
    ImGui::SetNextWindowSize(ImVec2(340, 420), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("TB-303 Synth  (MIDI Mapper demo)")) { ImGui::End(); return; }

    ImGui::TextDisabled("Right-click any control  ->  MIDI Learn...");
    ImGui::Spacing();

    // ── Pattern 1: wrapper widgets ───────────────────────────────────────────
    ImGui::SeparatorText("VCF  (wrapper widgets)");

    mapper.SliderFloat("cutoff",    "Cutoff",    &p.cutoff,    0.f, 1.f);
    mapper.SliderFloat("resonance", "Resonance", &p.resonance, 0.f, 1.f);
    mapper.SliderFloat("env_mod",   "Env Mod",   &p.envMod,    0.f, 1.f);

    ImGui::Spacing();
    ImGui::SeparatorText("ENV  (wrapper widgets)");

    mapper.SliderFloat("decay",       "Decay",       &p.decay,       0.f, 1.f);
    mapper.SliderFloat("accent_lvl",  "Accent Level",&p.accentLevel, 0.f, 1.f);

    ImGui::Spacing();
    ImGui::SeparatorText("AMP  (wrapper widgets)");

    mapper.SliderFloat("volume",     "Volume",      &p.volume,     0.f, 1.f);
    mapper.SliderFloat("distortion", "Distortion",  &p.distortion, 0.f, 1.f);

    // ── Pattern 2: existing ImGui:: widget + watchLast() ────────────────────
    ImGui::Spacing();
    ImGui::SeparatorText("OSC  (watchLast on existing widgets)");

    ImGui::Checkbox("Square Wave", &p.squareWave);
    mapper.watchLast("square_wave", &p.squareWave, "Square Wave");

    ImGui::Spacing();
    const char* octNames[] = {"-1", "0", "+1"};
    int octIdx = p.octave + 1;
    ImGui::SetNextItemWidth(120.f);
    if (ImGui::Combo("Octave", &octIdx, octNames, 3))
        p.octave = octIdx - 1;
    mapper.watchLast("octave", &octIdx, 0, 2, "Octave");

    // ── Live values readout ──────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::SeparatorText("Live values");
    ImGui::TextDisabled(
        "Cut %.2f  Res %.2f  Env %.2f  Dec %.2f\n"
        "Vol %.2f  Dst %.2f  Sq %s   Oct %+d",
        p.cutoff, p.resonance, p.envMod, p.decay,
        p.volume, p.distortion,
        p.squareWave ? "on" : "off", p.octave);

    ImGui::End();
}

// =============================================================================
// main
// =============================================================================
int main(int, char**)
{
    // ── SDL2 init ─────────────────────────────────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    // ── SDL2 + OpenGL context ─────────────────────────────────────────────────
    // macOS requires OpenGL 3.2 core; Windows/Linux work fine with 3.0.
#if defined(__APPLE__)
    const char* glslVersion = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,
                        SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#elif defined(IMGUI_IMPL_OPENGL_ES2)
    const char* glslVersion = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else  // Windows / Linux
    const char* glslVersion = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_WindowFlags wf = (SDL_WindowFlags)(SDL_WINDOW_OPENGL
                                          | SDL_WINDOW_RESIZABLE
                                          | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow(
        "ImMidiMapper Demo",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1100, 680, wf);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext glCtx = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glCtx);
    SDL_GL_SetSwapInterval(1);

    // ── Dear ImGui setup ──────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, glCtx);
    ImGui_ImplOpenGL3_Init(glslVersion);

    // ── ImMidiMapperRtMidi ────────────────────────────────────────────────────
    ImMidiMapperRtMidi mapper;

    if (!mapper.initError().empty())
        fprintf(stderr, "RtMidi init warning: %s\n", mapper.initError().c_str());

    // Auto-open first available ports (can be changed in the editor)
    auto ins  = mapper.inputPorts();
    auto outs = mapper.outputPorts();
    if (!ins .empty()) mapper.openInputPort (0);
    if (!outs.empty()) mapper.openOutputPort(0);

    // Try to restore a previous mapping
    mapper.load("midiMapper.json");

    // ── App state ─────────────────────────────────────────────────────────────
    SynthParams synth;
    bool showImGuiDemo    = false;
    bool showMapperEditor = true;
    bool running          = true;

    // ── Main loop ─────────────────────────────────────────────────────────────
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_WINDOWEVENT
                && ev.window.event == SDL_WINDOWEVENT_CLOSE
                && ev.window.windowID == SDL_GetWindowID(window))
                running = false;
        }

        // Flush MIDI queue into bound variables
        mapper.update();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ── Menu bar ──────────────────────────────────────────────────────────
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Windows")) {
                ImGui::MenuItem("MIDI Mapper Editor", nullptr, &showMapperEditor);
                ImGui::MenuItem("ImGui Demo",         nullptr, &showImGuiDemo);
                ImGui::EndMenu();
            }
            ImGui::TextDisabled("  |  right-click any slider to learn MIDI");
            ImGui::EndMainMenuBar();
        }

        // ── Synth control panel ───────────────────────────────────────────────
        DrawSynthPanel(mapper, synth);

        // ── MIDI Mapper editor ────────────────────────────────────────────────
        if (showMapperEditor)
            mapper.drawEditor(&showMapperEditor);

        // ── Standard ImGui demo ───────────────────────────────────────────────
        if (showImGuiDemo)
            ImGui::ShowDemoWindow(&showImGuiDemo);

        // ── Render ────────────────────────────────────────────────────────────
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.12f, 0.12f, 0.14f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // ── Save mapping on exit ──────────────────────────────────────────────────
    mapper.save("midiMapper.json");

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(glCtx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

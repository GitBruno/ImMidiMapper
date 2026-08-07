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
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <vector>
#include <filesystem>

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

// Offscreen dump: --capture <dir> <WxH> <frames>
static bool g_captureMode   = false;
static char g_captureDir[512] = "";
static int  g_captureW      = 800;
static int  g_captureH      = 480;
static int  g_captureFrames = 96;
static int  g_captureIndex  = 0;
static int  g_captureWarmup = 6;
static bool g_captureBound  = false;

static bool parseSizeWxH(const char* s, int* w, int* h)
{
    int ww = 0, hh = 0;
    if (sscanf(s, "%dx%d", &ww, &hh) != 2 || ww < 64 || hh < 64) return false;
    *w = ww; *h = hh;
    return true;
}

static bool saveFramePpm(const char* path, int w, int h, const uint8_t* rgba)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    std::vector<uint8_t> row((size_t)w * 3);
    for (int y = h - 1; y >= 0; --y) {
        const uint8_t* src = rgba + (size_t)y * w * 4;
        for (int x = 0; x < w; ++x) {
            row[x * 3 + 0] = src[x * 4 + 0];
            row[x * 3 + 1] = src[x * 4 + 1];
            row[x * 3 + 2] = src[x * 4 + 2];
        }
        fwrite(row.data(), 1, row.size(), f);
    }
    fclose(f);
    return true;
}

// Capture story: establish mapped CCs → MIDI Learn adds Env Mod → long hold.
// Learn happens early so most of the loop is the "added" state (not an unbind).
static void driveCaptureMidi(ImMidiMapperRtMidi& mapper, SynthParams& synth, int fi, int n)
{
    const float t = 2.f * 3.14159265f * (float)fi / (float)n;
    auto cc = [](float u) {
        return (int)std::lround(std::min(1.f, std::max(0.f, u)) * 127.f);
    };
    auto cutoffAt = [&](float phase) {
        return 0.5f + 0.38f * std::sin(phase);
    };

    const int tLearn = (int)(n * 0.12f);   // start Learn early
    const int tBind  = tLearn + 20;        // yellow Learn banner dwell

    if (fi == tLearn)
        mapper.startLearn("env_mod");

    // While learning we must not pushCC other controls — the first CC completes
    // Learn. Keep Cutoff moving by writing the float directly so the UI doesn't
    // look frozen.
    if (mapper.isLearning()) {
        synth.cutoff = cutoffAt(t);
        if (fi >= tBind)
            mapper.pushCC(1, 73, cc(0.62f));
        return;
    }

    mapper.pushCC(1, 74, cc(cutoffAt(t)));
    if (fi >= tBind)
        mapper.pushCC(1, 73, cc(0.55f + 0.28f * std::sin(t * 0.9f + 1.1f)));
}

// =============================================================================
// Draw the synth control panel using both mapping patterns
// =============================================================================
static void DrawSynthPanel(ImMidiMapperRtMidi& mapper, SynthParams& p)
{
    if (g_captureMode) {
        // Narrow strip — Mapper editor is the hero of the preview GIF.
        ImGui::SetNextWindowSize(ImVec2(220, 440), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(8, 28), ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowSize(ImVec2(340, 420), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    }

    if (!ImGui::Begin(g_captureMode ? "Synth" : "TB-303 Synth  (MIDI Mapper demo)")) {
        ImGui::End();
        return;
    }

    if (!g_captureMode)
        ImGui::TextDisabled("Right-click any control  ->  MIDI Learn...");
    else
        ImGui::TextDisabled("right-click -> Learn");
    ImGui::Spacing();

    // ── Pattern 1: wrapper widgets ───────────────────────────────────────────
    ImGui::SeparatorText(g_captureMode ? "VCF" : "VCF  (wrapper widgets)");

    mapper.SliderFloat("cutoff",    "Cutoff",    &p.cutoff,    0.f, 1.f);
    mapper.SliderFloat("resonance", "Resonance", &p.resonance, 0.f, 1.f);
    mapper.SliderFloat("env_mod",   "Env Mod",   &p.envMod,    0.f, 1.f);

    if (!g_captureMode) {
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
    } else {
        ImGui::Spacing();
        ImGui::SeparatorText("AMP");
        mapper.SliderFloat("volume", "Volume", &p.volume, 0.f, 1.f);
        ImGui::Spacing();
        ImGui::TextDisabled("Cut %.2f  Res %.2f\nEnv %.2f  Vol %.2f",
                            p.cutoff, p.resonance, p.envMod, p.volume);
    }

    ImGui::End();
}

// =============================================================================
// main
// =============================================================================
int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--capture") == 0) {
            if (i + 3 >= argc) {
                fprintf(stderr, "usage: --capture <dir> <WxH> <frames>\n");
                return 1;
            }
            g_captureMode = true;
            strncpy(g_captureDir, argv[++i], sizeof(g_captureDir) - 1);
            if (!parseSizeWxH(argv[++i], &g_captureW, &g_captureH)) {
                fprintf(stderr, "bad size (expected e.g. 720x480)\n");
                return 1;
            }
            g_captureFrames = atoi(argv[++i]);
            if (g_captureFrames < 1) g_captureFrames = 1;
        }
    }

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
    const int winW = g_captureMode ? g_captureW : 1100;
    const int winH = g_captureMode ? g_captureH : 680;
    SDL_Window* window = SDL_CreateWindow(
        g_captureMode ? "ImMidiMapper Demo (capture)" : "ImMidiMapper Demo",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        winW, winH, wf);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext glCtx = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glCtx);
    SDL_GL_SetSwapInterval(g_captureMode ? 0 : 1);

    // ── Dear ImGui setup ──────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (g_captureMode)
        io.IniFilename = nullptr;  // don't clobber interactive layout

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

    // Try to restore a previous mapping (skip in capture — scripted bindings)
    if (!g_captureMode)
        mapper.load("midiMapper.json");

    if (g_captureMode) {
        std::error_code ec;
        std::filesystem::create_directories(g_captureDir, ec);
        if (ec) {
            fprintf(stderr, "cannot create capture dir %s: %s\n",
                    g_captureDir, ec.message().c_str());
            return 1;
        }
        fprintf(stderr, "capture → %s  %dx%d  %d frames\n",
                g_captureDir, g_captureW, g_captureH, g_captureFrames);
    }

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

        if (g_captureMode && g_captureWarmup <= 0) {
            if (!g_captureBound) {
                // One pre-bound control — keeps the frame calm; Learn adds Env Mod.
                mapper.assign("cutoff", 1, 74);
                g_captureBound = true;
            }
            const int fi = std::min(g_captureIndex, g_captureFrames - 1);
            driveCaptureMidi(mapper, synth, fi, g_captureFrames);
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
        if (showMapperEditor) {
            if (g_captureMode) {
                // Hero panel — most of the frame
                ImGui::SetNextWindowPos(ImVec2(236, 28), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(556, 440), ImGuiCond_Always);
            }
            mapper.drawEditor(&showMapperEditor);
        }

        // ── Standard ImGui demo ───────────────────────────────────────────────
        if (showImGuiDemo)
            ImGui::ShowDemoWindow(&showImGuiDemo);

        // ── Render ────────────────────────────────────────────────────────────
        ImGui::Render();
        int fbW = 0, fbH = 0;
        SDL_GL_GetDrawableSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0.12f, 0.12f, 0.14f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (g_captureMode) {
            if (g_captureWarmup > 0) {
                --g_captureWarmup;
            } else if (g_captureIndex < g_captureFrames) {
                std::vector<uint8_t> px((size_t)fbW * fbH * 4);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadPixels(0, 0, fbW, fbH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                char path[768];
                snprintf(path, sizeof(path), "%s/frame_%04d.ppm",
                         g_captureDir, g_captureIndex);
                if (!saveFramePpm(path, fbW, fbH, px.data()))
                    fprintf(stderr, "failed to write %s\n", path);
                ++g_captureIndex;
                if (g_captureIndex >= g_captureFrames)
                    running = false;
            }
        }

        SDL_GL_SwapWindow(window);
    }

    // ── Save mapping on exit ──────────────────────────────────────────────────
    if (!g_captureMode)
        mapper.save("midiMapper.json");

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(glCtx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

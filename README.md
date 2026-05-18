# ImMidiMapper

> Map any Dear ImGui control to a MIDI CC — right-click to learn, zero extra dependencies.

Pure **C++17 + Dear ImGui** extension. No openFrameworks, no platform SDK in the core.  
MIDI I/O is a plug-in callback interface — bring your own backend, or use the included **RtMidi** backend.

---

## Features

- **Two registration patterns** — wrapper widgets or `watchLast()` after any existing `ImGui::` call
- **Right-click MIDI Learn** on any registered control
- **Indicator dot** — gray (unbound) · green (mapped) · pulsing yellow (learning)
- **Two-way sync** — drag a UI control → send CC back to hardware
- **Persistence** — save/load mappings as JSON (hand-rolled, no extra dependency)
- **Editor window** — full binding table, inline CC editing, port selector
- **Cross-platform** — macOS · Windows · Linux (CMake + FetchContent, no manual installs)

---

## Quick start

```cpp
// One translation unit only:
#define IMMIDIMAPPER_IMPLEMENTATION
#include "ImMidiMapper.h"
#include "ImMidiMapperRtMidi.h"   // optional — RtMidi backend

ImMidiMapperRtMidi mapper;
mapper.openInputPort(0);           // first available port

// main loop:
mapper.update();                   // flush MIDI queue → bound variables

// inside ImGui block — two patterns:

// 1. Wrapper widgets (drop-in ImGui:: replacements)
mapper.SliderFloat("cutoff",    "Cutoff",    &params.cutoff,    0.f, 1.f);
mapper.SliderFloat("resonance", "Resonance", &params.resonance, 0.f, 1.f);
mapper.Checkbox   ("square",    "Square",    &params.square);

// 2. watchLast() — after any existing ImGui:: widget
ImGui::SliderFloat("Decay", &params.decay, 0.f, 1.f);
mapper.watchLast("decay", &params.decay, 0.f, 1.f);

// Works with custom widgets too (knobs, dials, etc.):
MyKnob("Env Mod", &params.envMod, 40.f);
mapper.watchLast("env_mod", &params.envMod, 0.f, 1.f);

// Editor with built-in port selector:
mapper.drawEditor(&editorVisible);

// Persist:
mapper.save("midiMapper.json");
mapper.load("midiMapper.json");
```

---

## Building

```bash
git clone https://github.com/you/ImMidiMapper.git
cd ImMidiMapper
cmake -B build
cmake --build build
./bin/ImMidiMapperDemo
```

All dependencies (Dear ImGui, SDL2, RtMidi) are fetched automatically by CMake.

### Platform notes

| Platform | MIDI backend | Extra requirement |
|----------|-------------|-------------------|
| **macOS** | CoreMIDI (built-in) | Xcode CLT: `xcode-select --install` |
| **Windows** | WinMM (built-in) | Visual Studio 2019+ or MinGW |
| **Linux** | ALSA | `sudo apt install libasound2-dev` |

---

## Architecture

```
ImMidiMapper.h          ← core: pure C++17 + ImGui
                          pushCC()  /  onSendCC callback
                          
ImMidiMapperRtMidi.h    ← RtMidi adapter (header-only subclass)
                          routes RtMidi → pushCC
                          sets  onSendCC → RtMidi sendMessage
                          injects port selector into drawEditor()

example/main.cpp        ← SDL2 + OpenGL3 demo
```

### Bring your own backend

If you already have MIDI working (e.g. via ofxMidi, PortMidi, Web MIDI API):

```cpp
// Core mapper — no RtMidi needed
#define IMMIDIMAPPER_IMPLEMENTATION
#include "ImMidiMapper.h"

ImMidiMapper mapper;

// Feed incoming CC from your callback (thread-safe):
void yourMidiCallback(int channel, int cc, int value) {
    mapper.pushCC(channel, cc, value);
}

// Receive CC output for two-way sync:
mapper.onSendCC = [](int ch, int cc, int val) {
    yourMidiOut.sendCC(ch, cc, val);
};
```

---

## API reference

| Method | Description |
|--------|-------------|
| `pushCC(ch, cc, val)` | Thread-safe CC input |
| `pushNoteOn(ch, note, vel)` | Note-on as boolean trigger |
| `update()` | Apply queued events (call every frame) |
| `SliderFloat(id, label, &val, min, max)` | Wrapper for `ImGui::SliderFloat` |
| `SliderInt(id, label, &val, min, max)` | Wrapper for `ImGui::SliderInt` |
| `DragFloat(id, label, &val, ...)` | Wrapper for `ImGui::DragFloat` |
| `Checkbox(id, label, &val)` | Wrapper for `ImGui::Checkbox` |
| `Button(id, label, &toggle)` | Wrapper for `ImGui::Button` |
| `watchLast(id, &val, min, max)` | Post-widget hook for any ImGui item |
| `startLearn(id)` / `stopLearn()` | Programmatic learn control |
| `assign(id, channel, cc)` | Manual CC assignment |
| `clearBinding(id)` | Remove a CC assignment |
| `save(path)` / `load(path)` | JSON persistence |
| `drawEditor(&visible)` | Full mapping editor window |
| `bind(id, &val, ...)` | Register without drawing a widget |

---

## License

MIT

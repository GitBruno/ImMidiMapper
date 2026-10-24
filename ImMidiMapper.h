#pragma once
// =============================================================================
// ImMidiMapper  —  map any ImGui control to a MIDI CC
// =============================================================================
// Pure C++17 + Dear ImGui extension
// MIDI delivery is a push/callback interface:
//
//   MIDI in  →  call ImMidiMapper::pushCC(channel, cc, value)   (thread-safe)
//   MIDI out ←  set  ImMidiMapper::onSendCC  callback
//
// TWO WAYS TO REGISTER A CONTROL
// ──────────────────────────────
//  1. Wrapper widgets (drop-in ImGui:: replacements):
//
//       mapper.SliderFloat("cutoff", "Cutoff", &val, 0.f, 1.f);
//       mapper.Checkbox   ("square", "Square", &flag);
//
//  2. watchLast() after any existing ImGui item:
//
//       ImGui::SliderFloat("Cutoff", &val, 0.f, 1.f);
//       mapper.watchLast("cutoff", &val, 0.f, 1.f);
//
// MIDI LEARN
// ──────────
//  Wrappers and watchLast(): right-click the control, then "Map MIDI".
//  The status line reads Listening. Move a CC or note. It reads Mapped.
//  On a Dear ImGui carrying SetWidgetValueCallback() (IMGUI_HAS_WIDGET_VALUE_CALLBACK),
//  any other slider, drag, checkbox, or dropdown is learned from the widget
//  itself, with no registration call at all. On stock Dear ImGui that mode is
//  compiled out and the two registration paths above are the whole API.
//
// PERSISTENCE
// ───────────
//  mapper.save("map.json");  /  mapper.load("map.json");
//
// INDICATOR / STATUS
// ──────────────────
//  Mapping state is shown in the editor table (Ch / CC columns) and the
//  yellow "* Learning: …" banner — no overlay dots on widgets.
// =============================================================================

#include "imgui.h"
#include <string>
#include <vector>
#include <set>
#include <mutex>
#include <functional>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>

// =============================================================================
// ImMidiBinding
// =============================================================================
struct ImMidiBinding {
    std::string id;      ///< stable key supplied by the caller
    std::string label;   ///< human-readable name (shown in editor)

    // Target — exactly one non-null
    float* fTarget {nullptr};
    int*   iTarget {nullptr};
    bool*  bTarget {nullptr};

    float fMin {0.f},  fMax {1.f};
    int   iMin {0},    iMax {127};

    // MIDI assignment
    int channel {0};    ///< 0 = any channel
    int cc      {-1};   ///< -1 = unbound

    // Two-way: UI change → send CC out
    bool  twoWay   {false};
    float lastSent {-1.f};  ///< last normalised value sent

    /// Explicit controls (watchLast / wrappers) stay in the editor before a CC is learned.
    bool listed {true};
    bool hasPending {false};
    int  pendingValue {0};

    bool isMapped() const { return cc >= 0; }
    bool isFloat()  const { return fTarget != nullptr; }
    bool isInt()    const { return iTarget != nullptr; }
    bool isBool()   const { return bTarget != nullptr; }
};

// =============================================================================
// ImMidiMapper
// =============================================================================
class ImMidiMapper {
public:

    // ── Output callback (set this to route CC output to your MIDI backend) ──
    std::function<void(int channel, int cc, int value)> onSendCC;

    // ── Optional: called when a learn is completed ──────────────────────────
    std::function<void(const std::string& id, int channel, int cc)> onLearnComplete;

    // ── Optional: draw custom port-selector UI inside drawEditor() ──────────
    std::function<void()> drawPortsUI;

    // ── Optional: called when the editor edits a binding (Ch / CC / 2-way /
    //    clear / Clear All). Hosts that own persistence save here. ──────────
    std::function<void()> onBindingsChanged;

    // ── Optional: extra UI inside drawEditor(), below the mapper table. ────
    std::function<void()> drawEditorExtra;

    // ── File used by the editor's Save... / Load... buttons ─────────────────
    std::string editorFile {"midiMapper.json"};

    // =========================================================================
    // Lifecycle
    // =========================================================================

    ImMidiMapper();
    ~ImMidiMapper();

    /// Call every frame (e.g. in your main loop before ImGui::Render()).
    /// Applies any queued MIDI CC events to bound variables.
    void update();

    // =========================================================================
    // MIDI input (thread-safe) — call from your MIDI callback
    // =========================================================================

    /// Push a CC event.  Safe to call from any thread.
    void pushCC(int channel, int cc, int value);

    /// Push a Note-On as a boolean trigger (velocity > 0 = true).
    /// Velocity 0 is note-off (bool targets go false).
    void pushNoteOn(int channel, int note, int velocity);

    /// Send two-way CC for every mapped binding whose value moved.
    void flushTwoWay();

    // =========================================================================
    // Registration
    // =========================================================================

    ImMidiBinding& bind(const std::string& id, float* val,
                        float min = 0.f, float max = 1.f,
                        const std::string& label = "");
    ImMidiBinding& bind(const std::string& id, int*   val,
                        int min = 0, int max = 127,
                        const std::string& label = "");
    ImMidiBinding& bind(const std::string& id, bool*  val,
                        const std::string& label = "");

    void unbind(const std::string& id);
    void clearAll();

    bool             hasBinding(const std::string& id) const;
    ImMidiBinding*   findBinding(const std::string& id);
    const ImMidiBinding* findBinding(const std::string& id) const;
    const std::vector<ImMidiBinding>& bindings() const { return m_bindings; }

    // =========================================================================
    // Learn & assignment
    // =========================================================================

    void startLearn(const std::string& id);
    void stopLearn();
    bool isLearning(const std::string& id) const { return m_learningId == id; }
    bool isLearning()                       const { return !m_learningId.empty(); }

    void assign(const std::string& id, int channel, int cc);
    void clearBinding(const std::string& id);

    // =========================================================================
    // Wrapper widgets  (drop-in ImGui:: replacements)
    // =========================================================================
    // Return true when value changed (MIDI or UI), same contract as ImGui.

    bool SliderFloat(const std::string& id, const std::string& label,
                     float* val, float min = 0.f, float max = 1.f,
                     const char* fmt = "%.2f", float width = -1.f);

    bool SliderInt(const std::string& id, const std::string& label,
                   int* val, int min = 0, int max = 127);

    bool DragFloat(const std::string& id, const std::string& label,
                   float* val, float speed = 0.01f,
                   float min = 0.f, float max = 1.f,
                   const char* fmt = "%.3f");

    bool Checkbox(const std::string& id, const std::string& label, bool* val);

    bool Button(const std::string& id, const std::string& label,
                bool* toggleState = nullptr);

    // =========================================================================
    // watchLast() — post-widget hook for any existing ImGui:: item
    // =========================================================================
    // Call immediately after any ImGui:: widget to make it MIDI-mappable.
    // Returns true if the value was modified by MIDI this frame.

    bool watchLast(const std::string& id, float* val,
                   float min = 0.f, float max = 1.f,
                   const std::string& label = "");
    bool watchLast(const std::string& id, int*   val,
                   int min = 0, int max = 127,
                   const std::string& label = "");
    bool watchLast(const std::string& id, bool*  val,
                   const std::string& label = "");

    // =========================================================================
    // Persistence  (tiny hand-rolled JSON — no external dependency)
    // =========================================================================

    bool save(const std::string& path) const;
    bool load(const std::string& path);

    std::string toJson()       const;      ///< serialise to JSON string
    bool        fromJson(const std::string& json); ///< deserialise

    // =========================================================================
    // Editor window
    // =========================================================================

    /// Draws the full mapping editor as an ImGui window.
    void drawEditor(bool* open = nullptr);

    /// Draws the mapping editor contents inside the current ImGui window.
    /// Use this when a host window (e.g. a dockable panel) already owns Begin()/End().
    void drawEditorContents();

    /// Listening / Mapped line. Safe to call once a frame from any window.
    void drawStatus();

private:

    // ── Queued CC events (filled on MIDI thread, drained on main thread) ─────
    struct CCEvent { int channel; int cc; int value; bool isNote; };
    std::vector<CCEvent> m_queue;
    std::mutex           m_queueMutex;

    // ── State ────────────────────────────────────────────────────────────────
    struct WidgetRec {
        ImGuiID item {0};
        int type {-1};
        double minV {0};
        double maxV {1};
        bool hasRange {false};
        void* heapPtr {nullptr};
        std::string label;
        std::string explicitId;
        bool valid {false};
    };

    std::vector<ImMidiBinding> m_bindings;
    std::string                m_learningId;
    std::set<std::string>      m_updatedThisFrame;
    WidgetRec                  m_rec;
    std::string                m_notice;
    double                     m_noticeUntil {0};
    int                        m_statusFrame {-1};

#ifdef IMGUI_HAS_WIDGET_VALUE_CALLBACK
    static bool widgetValueCallback(ImGuiWidgetValueCallbackData* data);
    bool onWidget(ImGuiWidgetValueCallbackData* data);
#endif
    void noteWidgetOwner(const std::string& id, const void* ptr);
    void foldAutos(const std::string& keepId, const void* ptr);
    ImMidiBinding* findByPointer(const void* ptr);
    void rememberTarget(ImMidiBinding& b, int type, void* data, double lo, double hi, bool hasRange);
    void armWidget(const std::string& id, const std::string& label);
    bool writeMidi(int type, void* data, const void* vmin, const void* vmax, int midiValue) const;
    void drawMenuItems(const std::string& id, const std::string& label, bool allowArm);
    static bool addressOnStack(const void* p);
    static std::string widgetKey(ImGuiID id);
    static std::string widgetLabel(const char* windowName, const char* label);

    // ── Internal helpers ─────────────────────────────────────────────────────
    ImMidiBinding& getOrCreate(const std::string& id);
    void           applyCC(ImMidiBinding& b, int value);
    void           sendCC(ImMidiBinding& b);

    void drawIndicator(const std::string& id);
    void handleContextMenu(const std::string& id, const std::string& label);
    bool postWidget(const std::string& id, const std::string& label,
                    bool widgetChanged);

    /// `footerH` is the height the caller still needs below the table.
    void drawBindingTable(float footerH);

    // Tiny JSON helpers
    static std::string jsonEscape(const std::string& s);
    static bool        jsonExtractString(const std::string& src,
                                         const std::string& key, std::string& out);
    static bool        jsonExtractInt(const std::string& src,
                                      const std::string& key, int& out);
    static bool        jsonExtractBool(const std::string& src,
                                       const std::string& key, bool& out);
};

// =============================================================================
// Implementation  (header-only — include guard handles multiple inclusion)
// =============================================================================
#ifdef IMMIDIMAPPER_IMPLEMENTATION

#include "imgui_internal.h"

// With IMGUI_HAS_WIDGET_VALUE_CALLBACK (a Dear ImGui carrying SetWidgetValueCallback()),
// plain widgets are learned and driven automatically. Without it, this library works
// exactly as it always has on stock Dear ImGui: bind(), watchLast() and the wrapper
// widgets below are the registration paths, and no core hook is installed.
ImMidiMapper::ImMidiMapper()
{
#ifdef IMGUI_HAS_WIDGET_VALUE_CALLBACK
    ImGui::SetWidgetValueCallback(&ImMidiMapper::widgetValueCallback, this);
#endif
}

ImMidiMapper::~ImMidiMapper()
{
#ifdef IMGUI_HAS_WIDGET_VALUE_CALLBACK
    if (ImGui::GetWidgetValueCallback() == &ImMidiMapper::widgetValueCallback &&
        ImGui::GetWidgetValueCallbackUserData() == this)
        ImGui::SetWidgetValueCallback(nullptr);
#endif
}

#ifdef IMGUI_HAS_WIDGET_VALUE_CALLBACK
bool ImMidiMapper::widgetValueCallback(ImGuiWidgetValueCallbackData* data)
{
    auto* self = data ? static_cast<ImMidiMapper*>(data->UserData) : nullptr;
    return self ? self->onWidget(data) : false;
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
void ImMidiMapper::update()
{
    m_updatedThisFrame.clear();

    std::vector<CCEvent> events;
    {
        std::lock_guard<std::mutex> lk(m_queueMutex);
        events.swap(m_queue);
    }

    for (auto& ev : events) {
        // Learn: first event after startLearn() fires the binding
        if (!m_learningId.empty()) {
            if (ev.isNote && ev.value <= 0) continue; // note-off is not a learn hit
            int effectiveCC = ev.isNote ? (128 + ev.cc) : ev.cc;
            const std::string learned = m_learningId;
            assign(learned, ev.channel, effectiveCC);
            if (onLearnComplete) onLearnComplete(learned, ev.channel, effectiveCC);
            m_learningId.clear();
            m_notice = "Mapped";
            m_noticeUntil = -1.0;
        }

        for (auto& b : m_bindings) {
            if (!b.isMapped()) continue;
            if (b.cc != (ev.isNote ? 128 + ev.cc : ev.cc)) continue;
            if (b.channel != 0 && b.channel != ev.channel) continue;
            const bool live = b.fTarget || b.iTarget || b.bTarget;
            if (live)
                applyCC(b, ev.value);
            else {
                b.hasPending = true;
                b.pendingValue = ev.value;
            }
            m_updatedThisFrame.insert(b.id);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void ImMidiMapper::pushCC(int channel, int cc, int value)
{
    std::lock_guard<std::mutex> lk(m_queueMutex);
    m_queue.push_back({channel, cc, value, false});
}

void ImMidiMapper::pushNoteOn(int channel, int note, int velocity)
{
    std::lock_guard<std::mutex> lk(m_queueMutex);
    m_queue.push_back({channel, note, velocity, true});
}

// ─────────────────────────────────────────────────────────────────────────────
ImMidiBinding& ImMidiMapper::getOrCreate(const std::string& id)
{
    auto* b = findBinding(id);
    if (b) return *b;
    m_bindings.push_back({});
    m_bindings.back().id    = id;
    m_bindings.back().label = id;
    return m_bindings.back();
}

ImMidiBinding& ImMidiMapper::bind(const std::string& id, float* val,
                                   float min, float max,
                                   const std::string& label)
{
    auto& b = getOrCreate(id);
    b.fTarget = val;
    b.iTarget = nullptr;
    b.bTarget = nullptr;
    b.fMin = min;
    b.fMax = max;
    if (!label.empty()) b.label = label;
    return b;
}
ImMidiBinding& ImMidiMapper::bind(const std::string& id, int* val,
                                   int min, int max,
                                   const std::string& label)
{
    auto& b = getOrCreate(id);
    b.iTarget = val;
    b.fTarget = nullptr;
    b.bTarget = nullptr;
    b.iMin = min;
    b.iMax = max;
    if (!label.empty()) b.label = label;
    return b;
}
ImMidiBinding& ImMidiMapper::bind(const std::string& id, bool* val,
                                   const std::string& label)
{
    auto& b = getOrCreate(id);
    b.bTarget = val;
    b.fTarget = nullptr;
    b.iTarget = nullptr;
    if (!label.empty()) b.label = label;
    return b;
}

void ImMidiMapper::unbind(const std::string& id)
{
    m_bindings.erase(
        std::remove_if(m_bindings.begin(), m_bindings.end(),
                       [&](const ImMidiBinding& b){ return b.id == id; }),
        m_bindings.end());
}
void ImMidiMapper::clearAll()  { m_bindings.clear(); m_learningId.clear(); }

bool ImMidiMapper::hasBinding(const std::string& id) const
{
    for (auto& b : m_bindings) if (b.id == id) return true;
    return false;
}
ImMidiBinding* ImMidiMapper::findBinding(const std::string& id)
{
    for (auto& b : m_bindings) if (b.id == id) return &b;
    return nullptr;
}
const ImMidiBinding* ImMidiMapper::findBinding(const std::string& id) const
{
    for (auto& b : m_bindings) if (b.id == id) return &b;
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
void ImMidiMapper::startLearn(const std::string& id)
{
    if (!m_learningId.empty() && m_learningId != id)
        stopLearn();
    getOrCreate(id);
    m_learningId = id;
}
void ImMidiMapper::stopLearn()
{
    const std::string id = m_learningId;
    m_learningId.clear();
    m_notice.clear();
    m_noticeUntil = 0;
    if (auto* b = findBinding(id)) {
        if (!b->listed && !b->isMapped())
            unbind(id);
    }
}

void ImMidiMapper::assign(const std::string& id, int channel, int cc)
{
    auto& b = getOrCreate(id);
    b.channel = channel;
    b.cc      = cc;
}
void ImMidiMapper::clearBinding(const std::string& id)
{
    auto* b = findBinding(id);
    if (!b)
        return;
    if (!b->listed) {
        if (m_learningId == id)
            m_learningId.clear();
        unbind(id);
        return;
    }
    b->cc = -1;
    b->channel = 0;
    b->hasPending = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internals
// ─────────────────────────────────────────────────────────────────────────────
void ImMidiMapper::applyCC(ImMidiBinding& b, int value)
{
    float n = std::clamp(value / 127.f, 0.f, 1.f);
    if      (b.fTarget) *b.fTarget = b.fMin + n * (b.fMax - b.fMin);
    else if (b.iTarget) *b.iTarget = (int)std::round(b.iMin + n * (b.iMax - b.iMin));
    else if (b.bTarget) *b.bTarget = (value >= 64);
}

void ImMidiMapper::sendCC(ImMidiBinding& b)
{
    if (!onSendCC || !b.twoWay || !b.isMapped()) return;
    if (b.cc < 0 || b.cc > 127) return; // notes live at 128+n; feedback is CC only

    float n = 0.f;
    if      (b.fTarget) n = (*b.fTarget - b.fMin) / std::max(b.fMax - b.fMin, 1e-6f);
    else if (b.iTarget) n = float(*b.iTarget - b.iMin) / std::max(float(b.iMax - b.iMin), 1.f);
    else if (b.bTarget) n = *b.bTarget ? 1.f : 0.f;
    n = std::clamp(n, 0.f, 1.f);

    if (std::abs(n - b.lastSent) < 1.f / 128.f) return;
    b.lastSent = n;
    int ch = (b.channel == 0) ? 1 : b.channel;
    onSendCC(ch, b.cc, (int)(n * 127.f));
}

void ImMidiMapper::flushTwoWay()
{
    for (auto& b : m_bindings) sendCC(b);
}

// ─────────────────────────────────────────────────────────────────────────────
// Visual indicator dot — overlaid on the top-right of the last ImGui item
// ─────────────────────────────────────────────────────────────────────────────
void ImMidiMapper::drawIndicator(const std::string& /*id*/)
{
    // Intentionally empty: Ch/CC + Learn banner already communicate state.
    // Overlay dots were easy to mistake for a cursor / hit target.
}

// ─────────────────────────────────────────────────────────────────────────────
// Right-click context menu
// ─────────────────────────────────────────────────────────────────────────────
void ImMidiMapper::handleContextMenu(const std::string& id,
                                      const std::string& label)
{
    const std::string pid = "##imm_" + id;
    if (!ImGui::BeginPopupContextItem(pid.c_str(), ImGuiPopupFlags_MouseButtonRight))
        return;
    drawMenuItems(id, label, false);
    ImGui::EndPopup();
}

// ─────────────────────────────────────────────────────────────────────────────
// Shared post-widget logic
// ─────────────────────────────────────────────────────────────────────────────
bool ImMidiMapper::postWidget(const std::string& id, const std::string& label,
                               bool widgetChanged)
{
    drawIndicator(id);
#ifdef IMGUI_HAS_WIDGET_VALUE_CALLBACK
    bool opened = false;
    if (ImGui::BeginPopupContextWidget(&opened)) {
        if (opened)
            ImGui::EndPopupContextWidget();
    } else
#endif
    {
        handleContextMenu(id, label);
    }
    bool midiChanged = (m_updatedThisFrame.count(id) > 0);
    if (widgetChanged || ImGui::IsItemDeactivatedAfterEdit()) {
        if (auto* b = findBinding(id)) sendCC(*b);
    }
    return widgetChanged || midiChanged;
}

// ─────────────────────────────────────────────────────────────────────────────
// Wrapper widgets
// ─────────────────────────────────────────────────────────────────────────────
bool ImMidiMapper::SliderFloat(const std::string& id, const std::string& label,
                                float* val, float min, float max,
                                const char* fmt, float width)
{
    bind(id, val, min, max, label);
    if (width > 0.f) ImGui::SetNextItemWidth(width);
    bool ch = ImGui::SliderFloat(label.c_str(), val, min, max, fmt);
    return postWidget(id, label, ch);
}

bool ImMidiMapper::SliderInt(const std::string& id, const std::string& label,
                               int* val, int min, int max)
{
    bind(id, val, min, max, label);
    bool ch = ImGui::SliderInt(label.c_str(), val, min, max);
    return postWidget(id, label, ch);
}

bool ImMidiMapper::DragFloat(const std::string& id, const std::string& label,
                               float* val, float speed,
                               float min, float max, const char* fmt)
{
    bind(id, val, min, max, label);
    bool ch = ImGui::DragFloat(label.c_str(), val, speed, min, max, fmt);
    return postWidget(id, label, ch);
}

bool ImMidiMapper::Checkbox(const std::string& id, const std::string& label,
                              bool* val)
{
    bind(id, val, label);
    bool ch = ImGui::Checkbox(label.c_str(), val);
    return postWidget(id, label, ch);
}

bool ImMidiMapper::Button(const std::string& id, const std::string& label,
                           bool* toggleState)
{
    if (toggleState) bind(id, toggleState, label);
    bool pressed = ImGui::Button(label.c_str());
    if (pressed && toggleState) { *toggleState = !*toggleState; }
    return postWidget(id, label, pressed);
}

// ─────────────────────────────────────────────────────────────────────────────
// watchLast
// ─────────────────────────────────────────────────────────────────────────────
bool ImMidiMapper::watchLast(const std::string& id, float* val,
                               float min, float max, const std::string& label)
{
    bind(id, val, min, max, label.empty() ? id : label);
    foldAutos(id, val);
    noteWidgetOwner(id, val);
    return postWidget(id, label.empty() ? id : label,
                      /*widgetChanged=*/false);
}
bool ImMidiMapper::watchLast(const std::string& id, int* val,
                               int min, int max, const std::string& label)
{
    bind(id, val, min, max, label.empty() ? id : label);
    foldAutos(id, val);
    noteWidgetOwner(id, val);
    return postWidget(id, label.empty() ? id : label, false);
}
bool ImMidiMapper::watchLast(const std::string& id, bool* val,
                               const std::string& label)
{
    bind(id, val, label.empty() ? id : label);
    foldAutos(id, val);
    noteWidgetOwner(id, val);
    return postWidget(id, label.empty() ? id : label, false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Persistence — hand-rolled minimal JSON (no external dependency)
// ─────────────────────────────────────────────────────────────────────────────
std::string ImMidiMapper::jsonEscape(const std::string& s)
{
    std::string out;
    for (char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else                out += c;
    }
    return out;
}

std::string ImMidiMapper::toJson() const
{
    std::ostringstream o;
    o << "[\n";
    bool first = true;
    for (auto& b : m_bindings) {
        if (!b.isMapped()) continue;
        if (!first) o << ",\n";
        first = false;
        o << "  {\"id\":\"" << jsonEscape(b.id)
          << "\",\"label\":\"" << jsonEscape(b.label)
          << "\",\"channel\":" << b.channel
          << ",\"cc\":" << b.cc
          << ",\"twoWay\":" << (b.twoWay ? "true" : "false")
          << "}";
    }
    o << "\n]";
    return o.str();
}

bool ImMidiMapper::jsonExtractString(const std::string& src,
                                      const std::string& key, std::string& out)
{
    std::string needle = "\"" + key + "\":\"";
    auto pos = src.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.size();
    auto end = src.find('"', pos);
    if (end == std::string::npos) return false;
    out = src.substr(pos, end - pos);
    return true;
}
bool ImMidiMapper::jsonExtractInt(const std::string& src,
                                   const std::string& key, int& out)
{
    std::string needle = "\"" + key + "\":";
    auto pos = src.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.size();
    try { out = std::stoi(src.substr(pos)); return true; }
    catch (...) { return false; }
}
bool ImMidiMapper::jsonExtractBool(const std::string& src,
                                    const std::string& key, bool& out)
{
    std::string needle = "\"" + key + "\":";
    auto pos = src.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.size();
    if (src.substr(pos, 4) == "true")  { out = true;  return true; }
    if (src.substr(pos, 5) == "false") { out = false; return true; }
    return false;
}

bool ImMidiMapper::fromJson(const std::string& text)
{
    // Split on '{' ... '}' objects
    size_t pos = 0;
    while ((pos = text.find('{', pos)) != std::string::npos) {
        auto end = text.find('}', pos);
        if (end == std::string::npos) break;
        std::string obj = text.substr(pos, end - pos + 1);
        pos = end + 1;

        std::string id, label;
        int channel = 0, cc = -1;
        bool twoWay = false;
        if (!jsonExtractString(obj, "id", id) || id.empty()) continue;
        jsonExtractString(obj, "label",   label);
        jsonExtractInt   (obj, "channel", channel);
        jsonExtractInt   (obj, "cc",      cc);
        jsonExtractBool  (obj, "twoWay",  twoWay);

        auto& b = getOrCreate(id);
        if (!label.empty()) b.label = label;
        b.channel = channel;
        b.cc      = cc;
        b.twoWay  = twoWay;
    }
    return true;
}

bool ImMidiMapper::save(const std::string& path) const
{
    std::ofstream f(path);
    if (!f) return false;
    f << toJson();
    return true;
}
bool ImMidiMapper::load(const std::string& path)
{
    std::ifstream f(path);
    if (!f) return false;
    std::string text((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    return fromJson(text);
}

// ─────────────────────────────────────────────────────────────────────────────
// Editor window
// ─────────────────────────────────────────────────────────────────────────────
void ImMidiMapper::drawBindingTable(float footerH)
{
    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
        | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    // Sizes come from the font and style, not pixel constants, so a themed
    // or scaled font neither clips the buttons nor leaves the window with a
    // scrollbar of its own: the table takes what the footer does not need.
    const ImGuiStyle& st = ImGui::GetStyle();
    const float em      = ImGui::GetFontSize();
    const float cellPad = st.CellPadding.x * 2.f;
    const float padX    = st.FramePadding.x * 2.f;
    const float chW     = ImGui::CalcTextSize("16").x  + padX + cellPad + em * 0.5f;
    const float ccW     = ImGui::CalcTextSize("255").x + padX + cellPad + em * 0.5f;
    const float twW     = std::max(ImGui::CalcTextSize("2-way").x,
                                   ImGui::GetFrameHeight()) + cellPad;
    const float actW    = ImGui::CalcTextSize("Cancel").x + ImGui::CalcTextSize("x").x
                        + padX * 2.f + st.ItemSpacing.x + cellPad;

    float tableH = ImGui::GetContentRegionAvail().y - footerH;
    const float minH = ImGui::GetFrameHeightWithSpacing() * 3.f;
    if (tableH < minH) tableH = minH;

    if (!ImGui::BeginTable("##imm_tbl", 5, kFlags, ImVec2(0, tableH)))
        return;

    ImGui::TableSetupColumn("Control",   ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Ch",        ImGuiTableColumnFlags_WidthFixed, chW);
    ImGui::TableSetupColumn("CC",        ImGuiTableColumnFlags_WidthFixed, ccW);
    ImGui::TableSetupColumn("2-way",     ImGuiTableColumnFlags_WidthFixed, twW);
    ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed, actW);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    bool changed = false;
    for (auto& b : m_bindings) {
        if (!b.listed && !b.isMapped() && m_learningId != b.id)
            continue;
        ImGui::TableNextRow();
        ImGui::PushID(b.id.c_str());

        // Label
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        if (isLearning(b.id))
            ImGui::TextColored({1.f, 0.88f, 0.f, 1.f}, "%s",
                               b.label.empty() ? b.id.c_str() : b.label.c_str());
        else
            ImGui::TextUnformatted(b.label.empty() ? b.id.c_str() : b.label.c_str());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("id: %s", b.id.c_str());

        // Channel
        ImGui::TableSetColumnIndex(1);
        if (b.isMapped()) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##ch", &b.channel, 0)) {
                b.channel = std::clamp(b.channel, 0, 16);
                changed = true;
            }
        } else { ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("-"); }

        // CC (editable). Notes live at 128 + note, so the range is 0..255.
        ImGui::TableSetColumnIndex(2);
        if (b.isMapped()) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##cc", &b.cc, 0)) {
                b.cc = std::clamp(b.cc, 0, 255);
                changed = true;
            }
        } else { ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("-"); }

        // Two-way
        ImGui::TableSetColumnIndex(3);
        if (ImGui::Checkbox("##tw", &b.twoWay)) changed = true;

        // Actions
        ImGui::TableSetColumnIndex(4);
        if (isLearning(b.id)) {
            if (ImGui::SmallButton("Cancel")) stopLearn();
        } else {
            if (ImGui::SmallButton("Map"))  startLearn(b.id);
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!b.isMapped());
        if (ImGui::SmallButton("x")) { clearBinding(b.id); changed = true; }
        ImGui::EndDisabled();

        ImGui::PopID();
    }
    ImGui::EndTable();

    if (changed && onBindingsChanged) onBindingsChanged();
}

void ImMidiMapper::drawEditor(bool* open)
{
    // Window size/pos: caller may SetNextWindowSize/Pos before this (e.g. capture).
    if (!ImGui::Begin("MIDI Mapper###ImMidiMapper", open)) {
        ImGui::End();  return;
    }
    drawEditorContents();
    ImGui::End();
}

void ImMidiMapper::drawEditorContents()
{
    // Optional: custom port-selector drawn by the backend.
    // A full-width control at the end of that UI is the break; no extra rule.
    if (drawPortsUI) drawPortsUI();

    drawStatus();
    if (!m_learningId.empty()) {
        const char* name = m_learningId.c_str();
        if (auto* b = findBinding(m_learningId)) {
            if (!b->label.empty())
                name = b->label.c_str();
        }
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored({1.f, 0.88f, 0.f, 1.f}, "Listening  %s", name);
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel")) stopLearn();
        ImGui::Spacing();
    }

    // Footer is one filled button row plus the count. The row is the break.
    const ImGuiStyle& st = ImGui::GetStyle();
    const float footerH = ImGui::GetFrameHeight() + st.ItemSpacing.y
                        + ImGui::GetTextLineHeight();
    drawBindingTable(footerH);

    const float gap = st.ItemSpacing.x;
    const float avail = ImGui::GetContentRegionAvail().x;
    const float bw = std::max(1.f, (avail - gap * 2.f) / 3.f);
    if (ImGui::Button("Save...", ImVec2(bw, 0.f))) save(editorFile);
    ImGui::SameLine();
    if (ImGui::Button("Load...", ImVec2(bw, 0.f))) {
        load(editorFile);
        if (onBindingsChanged) onBindingsChanged();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All", ImVec2(bw, 0.f))) {
        clearAll();
        if (onBindingsChanged) onBindingsChanged();
    }
    ImGui::TextDisabled("%d registered  /  %d mapped",
        (int)m_bindings.size(),
        (int)std::count_if(m_bindings.begin(), m_bindings.end(),
             [](auto& b){ return b.isMapped(); }));

    if (drawEditorExtra) {
        drawEditorExtra();
    }
}

bool ImMidiMapper::addressOnStack(const void* p)
{
    if (!p)
        return false;
    const char here = 0;
    const auto a = reinterpret_cast<std::uintptr_t>(p);
    const auto b = reinterpret_cast<std::uintptr_t>(&here);
    const auto dist = a > b ? a - b : b - a;
    return dist < (static_cast<std::uintptr_t>(1) << 20);
}

std::string ImMidiMapper::widgetKey(ImGuiID id)
{
    return "w:" + std::to_string(id);
}

std::string ImMidiMapper::widgetLabel(const char* windowName, const char* label)
{
    std::string win = windowName ? windowName : "";
    const auto hash = win.find("###");
    if (hash != std::string::npos)
        win.resize(hash);
    const char* text = label ? label : "";
    const char* end = ImGui::FindRenderedTextEnd(text);
    std::string vis(text, end);
    if (vis.empty())
        return win.empty() ? std::string("control") : win;
    if (win.empty())
        return vis;
    return win + " / " + vis;
}

bool ImMidiMapper::writeMidi(int type, void* data, const void* vmin, const void* vmax,
                              int midiValue) const
{
    if (!data)
        return false;
    const float n = std::clamp(midiValue / 127.f, 0.f, 1.f);
    if (type == ImGuiDataType_Float) {
        float lo = 0.f, hi = 1.f;
        if (vmin && vmax) {
            lo = *static_cast<const float*>(vmin);
            hi = *static_cast<const float*>(vmax);
        }
        if (!(hi > lo)) {
            lo = 0.f;
            hi = 1.f;
        }
        float& dst = *static_cast<float*>(data);
        const float v = lo + n * (hi - lo);
        if (dst == v)
            return false;
        dst = v;
        return true;
    }
    if (type == ImGuiDataType_Double) {
        double lo = 0.0, hi = 1.0;
        if (vmin && vmax) {
            lo = *static_cast<const double*>(vmin);
            hi = *static_cast<const double*>(vmax);
        }
        if (!(hi > lo)) {
            lo = 0.0;
            hi = 1.0;
        }
        double& dst = *static_cast<double*>(data);
        const double v = lo + static_cast<double>(n) * (hi - lo);
        if (dst == v)
            return false;
        dst = v;
        return true;
    }
    if (type == ImGuiDataType_S32) {
        int lo = 0, hi = 127;
        if (vmin && vmax) {
            lo = *static_cast<const int*>(vmin);
            hi = *static_cast<const int*>(vmax);
        }
        if (!(hi > lo)) {
            lo = 0;
            hi = 127;
        }
        int& dst = *static_cast<int*>(data);
        const int v = static_cast<int>(std::lround(lo + n * static_cast<float>(hi - lo)));
        if (dst == v)
            return false;
        dst = v;
        return true;
    }
    if (type == ImGuiDataType_Bool) {
        bool& dst = *static_cast<bool*>(data);
        const bool v = midiValue >= 64;
        if (dst == v)
            return false;
        dst = v;
        return true;
    }
    return false;
}

ImMidiBinding* ImMidiMapper::findByPointer(const void* p)
{
    if (!p)
        return nullptr;
    ImMidiBinding* fallback = nullptr;
    for (auto& b : m_bindings) {
        const void* t = b.fTarget ? static_cast<void*>(b.fTarget)
                                  : b.iTarget ? static_cast<void*>(b.iTarget)
                                              : static_cast<void*>(b.bTarget);
        if (t != p)
            continue;
        if (b.listed)
            return &b;
        if (!fallback)
            fallback = &b;
    }
    return fallback;
}

void ImMidiMapper::rememberTarget(ImMidiBinding& b, int type, void* data, double lo, double hi,
                                  bool hasRange)
{
    const bool listed = b.listed;
    const std::string id = b.id;
    const std::string label = b.label;
    if (type == ImGuiDataType_Float) {
        float a = hasRange ? static_cast<float>(lo) : 0.f;
        float c = hasRange ? static_cast<float>(hi) : 1.f;
        if (!(c > a)) {
            a = 0.f;
            c = 1.f;
        }
        bind(id, static_cast<float*>(data), a, c, label).listed = listed;
    } else if (type == ImGuiDataType_S32) {
        int a = hasRange ? static_cast<int>(lo) : 0;
        int c = hasRange ? static_cast<int>(hi) : 127;
        if (!(c > a)) {
            a = 0;
            c = 127;
        }
        bind(id, static_cast<int*>(data), a, c, label).listed = listed;
    } else if (type == ImGuiDataType_Bool) {
        bind(id, static_cast<bool*>(data), label).listed = listed;
    }
}

void ImMidiMapper::armWidget(const std::string& id, const std::string& label)
{
    auto& created = getOrCreate(id);
    if (!label.empty())
        created.label = label;
    created.listed = false;
    if (!m_rec.heapPtr)
        return;
    rememberTarget(created, m_rec.type, m_rec.heapPtr, m_rec.minV, m_rec.maxV, m_rec.hasRange);
    if (auto* b = findBinding(id))
        b->listed = false;
}

void ImMidiMapper::foldAutos(const std::string& keepId, const void* ptr)
{
    if (!ptr)
        return;
    auto* keep = findBinding(keepId);
    if (!keep)
        return;
    std::vector<std::string> drop;
    for (const auto& b : m_bindings) {
        if (b.id == keepId || b.listed)
            continue;
        const void* t = b.fTarget ? static_cast<void*>(b.fTarget)
                                  : b.iTarget ? static_cast<void*>(b.iTarget)
                                              : static_cast<void*>(b.bTarget);
        if (t != ptr)
            continue;
        if (b.isMapped() && !keep->isMapped()) {
            keep->channel = b.channel;
            keep->cc = b.cc;
            keep->twoWay = b.twoWay;
        }
        if (m_learningId == b.id)
            m_learningId = keepId;
        drop.push_back(b.id);
    }
    for (const auto& id : drop)
        unbind(id);
}

void ImMidiMapper::noteWidgetOwner(const std::string& id, const void* ptr)
{
    if (!m_rec.valid || !ptr || m_rec.item != ImGui::GetItemID())
        return;
    m_rec.explicitId = id;
}

#ifdef IMGUI_HAS_WIDGET_VALUE_CALLBACK
bool ImMidiMapper::onWidget(ImGuiWidgetValueCallbackData* ev)
{
    if (!ev)
        return false;
    if (ev->Phase == ImGuiWidgetValueCallbackPhase_ContextMenuQuery)
        return m_rec.valid && m_rec.item == ev->ID;
    if (ev->Phase == ImGuiWidgetValueCallbackPhase_ContextMenuDraw) {
        const std::string id = m_rec.explicitId.empty() ? widgetKey(m_rec.item) : m_rec.explicitId;
        const ImMidiBinding* b = findBinding(id);
        const std::string label = (b && !b->label.empty()) ? b->label : m_rec.label;
        drawMenuItems(id, label, m_rec.explicitId.empty());
        return false;
    }

    const int type = ev->DataType;
    const bool supported = type == ImGuiDataType_Float || type == ImGuiDataType_Double ||
                           type == ImGuiDataType_S32 || type == ImGuiDataType_Bool;
    if (!supported || !ev->Value || ev->ID == 0)
        return false;

    const std::string key = widgetKey(ev->ID);
    if (ev->Phase == ImGuiWidgetValueCallbackPhase_BeforeSubmit) {
        ImMidiBinding* b = nullptr;
        if (!addressOnStack(ev->Value))
            b = findByPointer(ev->Value);
        if (!b)
            b = findBinding(key);
        if (!b)
            return false;
        if (b->hasPending) {
            const bool wrote = writeMidi(type, ev->Value, ev->ValueMin, ev->ValueMax, b->pendingValue);
            b->hasPending = false;
            return wrote;
        }
        return m_updatedThisFrame.count(b->id) > 0;
    }

    m_rec.valid = true;
    m_rec.item = ev->ID;
    m_rec.type = type;
    m_rec.heapPtr = (type == ImGuiDataType_Double || addressOnStack(ev->Value)) ? nullptr : ev->Value;
    m_rec.hasRange = ev->ValueMin && ev->ValueMax && type != ImGuiDataType_Bool;
    m_rec.minV = 0;
    m_rec.maxV = 1;
    if (m_rec.hasRange && type == ImGuiDataType_Float) {
        m_rec.minV = *static_cast<const float*>(ev->ValueMin);
        m_rec.maxV = *static_cast<const float*>(ev->ValueMax);
    } else if (m_rec.hasRange && type == ImGuiDataType_Double) {
        m_rec.minV = *static_cast<const double*>(ev->ValueMin);
        m_rec.maxV = *static_cast<const double*>(ev->ValueMax);
    } else if (m_rec.hasRange && type == ImGuiDataType_S32) {
        m_rec.minV = *static_cast<const int*>(ev->ValueMin);
        m_rec.maxV = *static_cast<const int*>(ev->ValueMax);
    }
    const char* win = ImGui::GetCurrentWindow() ? ImGui::GetCurrentWindow()->Name : "";
    m_rec.label = widgetLabel(win, ev->Label);
    m_rec.explicitId.clear();
    if (m_rec.heapPtr) {
        if (auto* b = findByPointer(m_rec.heapPtr)) {
            m_rec.explicitId = b->id;
            if (!b->label.empty())
                m_rec.label = b->label;
            rememberTarget(*b, type, m_rec.heapPtr, m_rec.minV, m_rec.maxV, m_rec.hasRange);
        }
    }
    if (auto* byKey = findBinding(key)) {
        if (m_rec.explicitId.empty())
            m_rec.explicitId = byKey->id;
        if (m_rec.heapPtr && byKey->id != m_rec.explicitId)
            rememberTarget(*byKey, type, m_rec.heapPtr, m_rec.minV, m_rec.maxV, m_rec.hasRange);
    }
    return false;
}
#endif // IMGUI_HAS_WIDGET_VALUE_CALLBACK

void ImMidiMapper::drawMenuItems(const std::string& id, const std::string& label, bool allowArm)
{
    ImGui::TextDisabled("%s", label.c_str());
    ImGui::Separator();
    if (isLearning(id)) {
        ImGui::TextColored(ImVec4(1.f, 0.88f, 0.f, 1.f), "Listening");
        if (ImGui::MenuItem("Cancel"))
            stopLearn();
    } else if (ImGui::MenuItem("Map MIDI")) {
        if (allowArm && !findBinding(id))
            armWidget(id, label);
        startLearn(id);
        m_statusFrame = -1;
        drawStatus();
    }
    if (auto* live = findBinding(id); live && live->isMapped()) {
        if (ImGui::MenuItem("Clear Mapping")) {
            clearBinding(id);
            if (onBindingsChanged)
                onBindingsChanged();
        } else {
            ImGui::Separator();
            ImGui::TextDisabled("ch %d  CC %d", live->channel == 0 ? 1 : live->channel, live->cc);
            if (ImGui::Checkbox("Two-way (UI -> CC out)", &live->twoWay) && onBindingsChanged)
                onBindingsChanged();
        }
    }
}

void ImMidiMapper::drawStatus()
{
    if (!ImGui::GetCurrentContext() || ImGui::GetFont() == nullptr)
        return;
    const int frame = ImGui::GetFrameCount();
    if (m_statusFrame == frame)
        return;

    const bool listening = !m_learningId.empty();
    if (!listening && m_notice == "Mapped" && m_noticeUntil < 0.0)
        m_noticeUntil = ImGui::GetTime() + 1.5;
    const bool mapped = !listening && m_notice == "Mapped" && ImGui::GetTime() < m_noticeUntil;
    if (!listening && !mapped) {
        m_statusFrame = frame;
        return;
    }
    m_statusFrame = frame;

    const char* head = listening ? "Listening" : "Mapped";
    const char* sub = listening ? "Wiggle a control" : nullptr;
    const ImVec2 headSize = ImGui::CalcTextSize(head);
    const ImVec2 subSize = sub ? ImGui::CalcTextSize(sub) : ImVec2(0.f, 0.f);
    const ImVec2 pad = ImGui::GetStyle().FramePadding;
    const float gap = sub ? ImGui::GetStyle().ItemSpacing.y : 0.f;
    const ImVec2 box(std::max(headSize.x, subSize.x) + pad.x * 2.f,
                     headSize.y + (sub ? subSize.y + gap : 0.f) + pad.y * 2.f);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 pos(vp->WorkPos.x + (vp->WorkSize.x - box.x) * 0.5f,
                     vp->WorkPos.y + vp->WorkSize.y - box.y - ImGui::GetFontSize());
    const ImVec2 boxMax(pos.x + box.x, pos.y + box.y);
    ImDrawList* dl = ImGui::GetForegroundDrawList(const_cast<ImGuiViewport*>(vp));
    const float rounding = ImGui::GetStyle().FrameRounding;
    dl->AddRectFilled(pos, boxMax, ImGui::GetColorU32(ImGuiCol_WindowBg), rounding);
    dl->AddRect(pos, boxMax, ImGui::GetColorU32(ImGuiCol_Border), rounding);
    const ImU32 headCol = listening ? IM_COL32(255, 224, 0, 255)
                                    : ImGui::GetColorU32(ImGuiCol_CheckMark);
    dl->AddText(ImVec2(pos.x + pad.x, pos.y + pad.y), headCol, head);
    if (sub)
        dl->AddText(ImVec2(pos.x + pad.x, pos.y + pad.y + headSize.y + gap),
                    ImGui::GetColorU32(ImGuiCol_Text), sub);
}

#endif // IMMIDIMAPPER_IMPLEMENTATION

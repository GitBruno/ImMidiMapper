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
//  Right-click any registered control → "MIDI Learn…" → move a CC → bound.
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
#include <cmath>
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

    // =========================================================================
    // Lifecycle
    // =========================================================================

    ImMidiMapper()  = default;
    ~ImMidiMapper() = default;

    /// Call every frame (e.g. in your main loop before ImGui::Render()).
    /// Applies any queued MIDI CC events to bound variables.
    void update();

    // =========================================================================
    // MIDI input (thread-safe) — call from your MIDI callback
    // =========================================================================

    /// Push a CC event.  Safe to call from any thread.
    void pushCC(int channel, int cc, int value);

    /// Push a Note-On as a boolean trigger (velocity > 0 = true).
    void pushNoteOn(int channel, int note, int velocity);

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

private:

    // ── Queued CC events (filled on MIDI thread, drained on main thread) ─────
    struct CCEvent { int channel; int cc; int value; bool isNote; };
    std::vector<CCEvent> m_queue;
    std::mutex           m_queueMutex;

    // ── State ────────────────────────────────────────────────────────────────
    std::vector<ImMidiBinding> m_bindings;
    std::string                m_learningId;
    std::set<std::string>      m_updatedThisFrame;

    // ── Internal helpers ─────────────────────────────────────────────────────
    ImMidiBinding& getOrCreate(const std::string& id);
    void           applyCC(ImMidiBinding& b, int value);
    void           sendCC(ImMidiBinding& b);

    void drawIndicator(const std::string& id);
    void handleContextMenu(const std::string& id, const std::string& label);
    bool postWidget(const std::string& id, const std::string& label,
                    bool widgetChanged);

    void drawBindingTable();

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
            int effectiveCC = ev.isNote ? (128 + ev.cc) : ev.cc;
            assign(m_learningId, ev.channel, effectiveCC);
            if (onLearnComplete) onLearnComplete(m_learningId, ev.channel, effectiveCC);
            m_learningId.clear();
        }

        for (auto& b : m_bindings) {
            if (!b.isMapped()) continue;
            if (b.cc != (ev.isNote ? 128 + ev.cc : ev.cc)) continue;
            if (b.channel != 0 && b.channel != ev.channel) continue;
            applyCC(b, ev.value);
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
    if (velocity <= 0) return;
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
    b.fTarget = val;  b.fMin = min;  b.fMax = max;
    if (!label.empty()) b.label = label;
    return b;
}
ImMidiBinding& ImMidiMapper::bind(const std::string& id, int* val,
                                   int min, int max,
                                   const std::string& label)
{
    auto& b = getOrCreate(id);
    b.iTarget = val;  b.iMin = min;  b.iMax = max;
    if (!label.empty()) b.label = label;
    return b;
}
ImMidiBinding& ImMidiMapper::bind(const std::string& id, bool* val,
                                   const std::string& label)
{
    auto& b = getOrCreate(id);
    b.bTarget = val;
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
    getOrCreate(id);
    m_learningId = id;
}
void ImMidiMapper::stopLearn() { m_learningId.clear(); }

void ImMidiMapper::assign(const std::string& id, int channel, int cc)
{
    auto& b = getOrCreate(id);
    b.channel = channel;
    b.cc      = cc;
}
void ImMidiMapper::clearBinding(const std::string& id)
{
    auto* b = findBinding(id);
    if (b) { b->cc = -1; b->channel = 0; }
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
    if (!ImGui::IsItemHovered()) return;
    std::string pid = "##imm_ctx_" + id;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        ImGui::OpenPopup(pid.c_str());

    if (!ImGui::BeginPopup(pid.c_str())) return;

    auto* b = findBinding(id);
    ImGui::TextDisabled("%s", label.c_str());
    ImGui::Separator();

    if (isLearning(id)) {
        ImGui::TextColored({1.f, 0.88f, 0.f, 1.f}, "* Waiting for CC...");
        if (ImGui::MenuItem("Cancel")) stopLearn();
    } else {
        if (ImGui::MenuItem("MIDI Learn...")) startLearn(id);
    }

    if (b && b->isMapped()) {
        if (ImGui::MenuItem("Clear Mapping")) clearBinding(id);
        ImGui::Separator();
        ImGui::TextDisabled("ch %d  CC %d", b->channel == 0 ? 1 : b->channel, b->cc);
        ImGui::Checkbox("Two-way (UI -> CC out)", &b->twoWay);
    }

    ImGui::EndPopup();
}

// ─────────────────────────────────────────────────────────────────────────────
// Shared post-widget logic
// ─────────────────────────────────────────────────────────────────────────────
bool ImMidiMapper::postWidget(const std::string& id, const std::string& label,
                               bool widgetChanged)
{
    drawIndicator(id);
    handleContextMenu(id, label);
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
    return postWidget(id, label.empty() ? id : label,
                      /*widgetChanged=*/false);
}
bool ImMidiMapper::watchLast(const std::string& id, int* val,
                               int min, int max, const std::string& label)
{
    bind(id, val, min, max, label.empty() ? id : label);
    return postWidget(id, label.empty() ? id : label, false);
}
bool ImMidiMapper::watchLast(const std::string& id, bool* val,
                               const std::string& label)
{
    bind(id, val, label.empty() ? id : label);
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
void ImMidiMapper::drawBindingTable()
{
    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
        | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

    float tableH = ImGui::GetContentRegionAvail().y - 40.f;
    if (!ImGui::BeginTable("##imm_tbl", 5, kFlags, ImVec2(0, tableH)))
        return;

    ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch,  2.f);
    ImGui::TableSetupColumn("Ch",      ImGuiTableColumnFlags_WidthFixed,   34.f);
    ImGui::TableSetupColumn("CC",      ImGuiTableColumnFlags_WidthFixed,   44.f);
    ImGui::TableSetupColumn("2-way",   ImGuiTableColumnFlags_WidthFixed,   50.f);
    ImGui::TableSetupColumn("",        ImGuiTableColumnFlags_WidthFixed,   88.f);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    for (auto& b : m_bindings) {
        ImGui::TableNextRow();
        ImGui::PushID(b.id.c_str());

        // Label
        ImGui::TableSetColumnIndex(0);
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
            ImGui::SetNextItemWidth(30.f);
            if (ImGui::InputInt("##ch", &b.channel, 0))
                b.channel = std::clamp(b.channel, 0, 16);
        } else { ImGui::TextDisabled("-"); }

        // CC (editable)
        ImGui::TableSetColumnIndex(2);
        if (b.isMapped()) {
            ImGui::SetNextItemWidth(38.f);
            if (ImGui::InputInt("##cc", &b.cc, 0))
                b.cc = std::clamp(b.cc, 0, 127);
        } else { ImGui::TextDisabled("-"); }

        // Two-way
        ImGui::TableSetColumnIndex(3);
        ImGui::Checkbox("##tw", &b.twoWay);

        // Actions
        ImGui::TableSetColumnIndex(4);
        if (isLearning(b.id)) {
            if (ImGui::SmallButton("Cancel")) stopLearn();
        } else {
            if (ImGui::SmallButton("Learn"))  startLearn(b.id);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) clearBinding(b.id);

        ImGui::PopID();
    }
    ImGui::EndTable();
}

void ImMidiMapper::drawEditor(bool* open)
{
    // Window size/pos: caller may SetNextWindowSize/Pos before this (e.g. capture).
    if (!ImGui::Begin("MIDI Mapper###ImMidiMapper", open)) {
        ImGui::End();  return;
    }

    // Optional: custom port-selector drawn by the backend
    if (drawPortsUI) { drawPortsUI(); ImGui::Separator(); ImGui::Spacing(); }

    if (!m_learningId.empty()) {
        ImGui::TextColored({1.f, 0.88f, 0.f, 1.f},
            "* Learning: %s  -- move a CC knob/fader", m_learningId.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel")) stopLearn();
        ImGui::Spacing();
    }

    drawBindingTable();

    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button("Save...")) save("midiMapper.json");
    ImGui::SameLine();
    if (ImGui::Button("Load...")) load("midiMapper.json");
    ImGui::SameLine();
    if (ImGui::Button("Clear All")) clearAll();
    ImGui::SameLine();
    ImGui::TextDisabled("%d registered  /  %d mapped",
        (int)m_bindings.size(),
        (int)std::count_if(m_bindings.begin(), m_bindings.end(),
             [](auto& b){ return b.isMapped(); }));

    ImGui::End();
}

#endif // IMMIDIMAPPER_IMPLEMENTATION

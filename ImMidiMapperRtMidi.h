#pragma once
// =============================================================================
// ImMidiMapperRtMidi.h  —  RtMidi backend for ImMidiMapper
// =============================================================================
// Header-only.  Subclass ImMidiMapper that owns RtMidiIn/Out, routes messages
// via the pushCC / onSendCC callback interface, and injects a port-selector
// panel into the editor via drawPortsUI.
//
// Usage:
//   #define IMMIDIMAPPER_IMPLEMENTATION
//   #include "ImMidiMapper.h"
//   #include "ImMidiMapperRtMidi.h"
//
//   ImMidiMapperRtMidi mapper;
//   mapper.openInputPort(0);
//   mapper.openOutputPort("IAC Bus 1");
//
//   // main loop:
//   mapper.update();
//
//   // ImGui block:
//   mapper.SliderFloat("cutoff", "Cutoff", &val, 0.f, 1.f);
//   mapper.drawEditor();
// =============================================================================

#include "ImMidiMapper.h"
#include "RtMidi.h"

#include <memory>
#include <vector>
#include <string>
#include <algorithm>

class ImMidiMapperRtMidi : public ImMidiMapper {
public:

    ImMidiMapperRtMidi()
    {
        try {
            m_in  = std::make_unique<RtMidiIn> (RtMidi::UNSPECIFIED, "ImMidiMapper In");
            m_out = std::make_unique<RtMidiOut>(RtMidi::UNSPECIFIED, "ImMidiMapper Out");
            m_in->setCallback(&rtCallback, this);
            m_in->ignoreTypes(true, true, true);   // drop sysex, timing, sense
        } catch (const RtMidiError& e) {
            m_initError = e.getMessage();
        }

        // Wire output: onSendCC → RtMidi sendMessage
        onSendCC = [this](int ch, int cc, int val) {
            if (!m_out || !m_out->isPortOpen()) return;
            std::vector<unsigned char> msg = {
                (unsigned char)(0xB0 | std::clamp(ch - 1, 0, 15)),
                (unsigned char)std::clamp(cc,  0, 127),
                (unsigned char)std::clamp(val, 0, 127)
            };
            try { m_out->sendMessage(&msg); } catch (...) {}
        };

        // Inject port selector into the editor
        drawPortsUI = [this] { drawSelector(); };
    }

    ~ImMidiMapperRtMidi() { closeAll(); }

    // ── Ports ────────────────────────────────────────────────────────────────

    bool openInputPort(unsigned int idx)
    {
        if (!m_in) return false;
        try {
            if (m_in->isPortOpen()) m_in->closePort();
            m_in->openPort(idx);
            m_curIn = safeName(m_in.get(), idx);
            m_portsDirty = true;
            return true;
        } catch (const RtMidiError& e) { m_lastError = e.getMessage(); return false; }
    }

    bool openInputPort(const std::string& name)
    {
        unsigned n = m_in ? m_in->getPortCount() : 0;
        for (unsigned i = 0; i < n; ++i)
            if (safeName(m_in.get(), i) == name) return openInputPort(i);
        m_lastError = "Input port not found: " + name;
        return false;
    }

    bool openOutputPort(unsigned int idx)
    {
        if (!m_out) return false;
        try {
            if (m_out->isPortOpen()) m_out->closePort();
            m_out->openPort(idx);
            m_curOut = safeName(m_out.get(), idx);
            m_portsDirty = true;
            return true;
        } catch (const RtMidiError& e) { m_lastError = e.getMessage(); return false; }
    }

    bool openOutputPort(const std::string& name)
    {
        unsigned n = m_out ? m_out->getPortCount() : 0;
        for (unsigned i = 0; i < n; ++i)
            if (safeName(m_out.get(), i) == name) return openOutputPort(i);
        m_lastError = "Output port not found: " + name;
        return false;
    }

    void closeAll()
    {
        try {
            if (m_in  && m_in ->isPortOpen()) m_in ->closePort();
            if (m_out && m_out->isPortOpen()) m_out->closePort();
        } catch (...) {}
        m_curIn.clear();
        m_curOut.clear();
    }

    bool isInputOpen()  const { return m_in  && m_in ->isPortOpen(); }
    bool isOutputOpen() const { return m_out && m_out->isPortOpen(); }

    std::vector<std::string> inputPorts() const
    {
        std::vector<std::string> v;
        if (!m_in) return v;
        for (unsigned i = 0, n = m_in->getPortCount(); i < n; ++i)
            v.push_back(safeName(m_in.get(), i));
        return v;
    }

    std::vector<std::string> outputPorts() const
    {
        std::vector<std::string> v;
        if (!m_out) return v;
        for (unsigned i = 0, n = m_out->getPortCount(); i < n; ++i)
            v.push_back(safeName(m_out.get(), i));
        return v;
    }

    const std::string& initError()  const { return m_initError; }
    const std::string& lastError()  const { return m_lastError; }

private:

    std::unique_ptr<RtMidiIn>  m_in;
    std::unique_ptr<RtMidiOut> m_out;
    std::string m_curIn, m_curOut;
    std::string m_initError, m_lastError;

    std::vector<std::string> m_cacheIn, m_cacheOut;
    bool m_portsDirty {true};

    // ── Safe port name helper (RtMidi can throw on getPortName) ─────────────
    static std::string safeName(RtMidi* rt, unsigned idx)
    {
        try { return rt->getPortName(idx); }
        catch (...) { return "(error)"; }
    }

    // ── RtMidi static callback ───────────────────────────────────────────────
    static void rtCallback(double /*dt*/, std::vector<unsigned char>* msg, void* ud)
    {
        if (!msg || msg->size() < 2) return;
        auto* self = static_cast<ImMidiMapperRtMidi*>(ud);
        int ch   = ((*msg)[0] & 0x0F) + 1;
        int type =  (*msg)[0] & 0xF0;

        if (type == 0xB0 && msg->size() >= 3)
            self->pushCC(ch, (*msg)[1], (*msg)[2]);
        else if (type == 0x90 && msg->size() >= 3 && (*msg)[2] > 0)
            self->pushNoteOn(ch, (*msg)[1], (*msg)[2]);
    }

    // ── Port selector (drawn inside drawEditor via drawPortsUI hook) ─────────
    void drawSelector()
    {
        if (!m_initError.empty()) {
            ImGui::TextColored({1.f, 0.3f, 0.3f, 1.f},
                               "RtMidi init error: %s", m_initError.c_str());
            return;
        }

        if (m_portsDirty) {
            m_portsDirty = false;
            m_cacheIn  = inputPorts();
            m_cacheOut = outputPorts();
        }

        const char* ci = m_curIn .empty() ? "(closed)" : m_curIn .c_str();
        const char* co = m_curOut.empty() ? "(closed)" : m_curOut.c_str();

        ImGui::SetNextItemWidth(210.f);
        if (ImGui::BeginCombo("In##rtmidi_in", ci)) {
            if (ImGui::Selectable("(closed)", m_curIn.empty()))
                { if (m_in->isPortOpen()) { m_in->closePort(); m_curIn.clear(); } }
            for (unsigned i = 0; i < (unsigned)m_cacheIn.size(); ++i) {
                bool sel = (m_cacheIn[i] == m_curIn);
                if (ImGui::Selectable(m_cacheIn[i].c_str(), sel)) openInputPort(i);
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine(0, 8.f);

        ImGui::SetNextItemWidth(210.f);
        if (ImGui::BeginCombo("Out##rtmidi_out", co)) {
            if (ImGui::Selectable("(closed)", m_curOut.empty()))
                { if (m_out->isPortOpen()) { m_out->closePort(); m_curOut.clear(); } }
            for (unsigned i = 0; i < (unsigned)m_cacheOut.size(); ++i) {
                bool sel = (m_cacheOut[i] == m_curOut);
                if (ImGui::Selectable(m_cacheOut[i].c_str(), sel)) openOutputPort(i);
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh")) m_portsDirty = true;

        if (!m_lastError.empty())
            ImGui::TextColored({1.f, 0.45f, 0.45f, 1.f}, "%s", m_lastError.c_str());
    }
};

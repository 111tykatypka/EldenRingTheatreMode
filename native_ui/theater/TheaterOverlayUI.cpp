#include "TheaterOverlayUI.h"

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace TheaterUI
{
namespace
{
    using namespace Theme;

    constexpr ImGuiWindowFlags kRegion = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    constexpr std::uint64_t kSpeeds[] = { 10, 25, 50, 100, 200, 400 };
    constexpr const char* kSpeedLabels[] = { "0.10x", "0.25x", "0.50x", "1.00x", "2.00x", "4.00x" };
    constexpr size_t kMaxLog = 400;

    void FormatTime(double seconds, char* out, size_t n)
    {
        if (!std::isfinite(seconds) || seconds < 0) seconds = 0;
        const auto ms = (unsigned long long)(seconds * 1000.0 + 0.5);
        const auto m = ms / 60000, sec = (ms / 1000) % 60, milli = ms % 1000;
        snprintf(out, n, "%02llu:%02llu.%03llu", m, sec, milli);
    }

    // Same folder the recorder already uses. Kernel32 only, so the Rust DLL needs no extra link libraries.
    std::filesystem::path SettingsPath()
    {
        wchar_t base[MAX_PATH]{};
        const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
        if (!n || n >= MAX_PATH) return {};
        return std::filesystem::path(base) / L"EldenRingTheaterMode" / L"TheaterOverlay.ini";
    }

    // Badge for in_game_replay::Phase (inactive, starting, playing, paused,
    // finishing, finished, error, restarting, preparing).
    struct Badge { Str label; Tone tone; };
    Badge BadgeFor(const theater_ui::Snapshot& s, bool linked)
    {
        if (!linked) return { Str::BadgeOffline, Tone::Warning };
        switch (s.phase)
        {
        case 1: return { Str::BadgeStarting, Tone::Info };
        case 2: return { Str::BadgePlaying, Tone::Success };
        case 3: return { Str::BadgePaused, Tone::Neutral };
        case 4: return { Str::BadgeStopping, Tone::Neutral };
        case 5: return { Str::BadgeFinished, Tone::Neutral };
        case 6: return { Str::BadgeError, Tone::Error };
        case 7: return { Str::BadgeRestarting, Tone::Warning };
        case 8: return { Str::BadgePreparing, Tone::Info };
        default: return { s.loaded ? Str::BadgeReady : Str::BadgeIdle, s.loaded ? Tone::Success : Tone::Neutral };
        }
    }

    // A filled, rounded button with an optional icon; returns true when clicked.
    bool FlatButton(const char* id, const char* label, ImVec2 size, Rgba fill, Rgba text, bool enabled = true)
    {
        ImGui::BeginDisabled(!enabled);
        ImGui::PushStyleColor(ImGuiCol_Button, fill.Vec4());
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (fill.a ? Rgba{ (uint8_t)std::min(255, fill.r + 18), (uint8_t)std::min(255, fill.g + 18), (uint8_t)std::min(255, fill.b + 18), fill.a } : Color::HoverBg).Vec4());
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Color::ActiveBg.Vec4());
        ImGui::PushStyleColor(ImGuiCol_Text, text.Vec4());
        ImGui::PushID(id);
        const bool clicked = ImGui::Button(label, size);
        ImGui::PopID();
        ImGui::PopStyleColor(4);
        ImGui::EndDisabled();
        return clicked && enabled;
    }

    void Tooltip(const char* text)
    {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", text);
    }
}

const char* IconUtf8(std::uint16_t c, char (&out)[4])
{
    if (c < 0x80) { out[0] = (char)c; out[1] = 0; }
    else if (c < 0x800) { out[0] = (char)(0xC0 | (c >> 6)); out[1] = (char)(0x80 | (c & 0x3F)); out[2] = 0; }
    else { out[0] = (char)(0xE0 | (c >> 12)); out[1] = (char)(0x80 | ((c >> 6) & 0x3F)); out[2] = (char)(0x80 | (c & 0x3F)); out[3] = 0; }
    return out;
}

void Overlay::Init(ImGuiIO& io)
{
    // The overlay draws its own cursor; the platform backend must not call SetCursor.
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    ui_.fonts = LoadFonts(io);
    iconFont_ = LoadIconFont(io);
    ui_.visibility = UiVisibility::Hidden;
    ui_.layout.panelOpen = true;
    ui_.activeTool = Tool::Scene;
    LoadSettings();
    appliedScale_ = 0.0f;
}

void Overlay::LoadSettings()
{
    const auto path = SettingsPath();
    if (path.empty()) return;
    std::ifstream in(path);
    std::string key; float value = 0;
    while (in >> key >> value)
    {
        if (key == "language") language = value >= 1 ? Lang::Russian : Lang::English;
        else if (key == "ui_scale") ui_.layout.uiScaleUser = std::clamp(value, 0.75f, 1.5f);
        else if (key == "panel_open") ui_.layout.panelOpen = value != 0;
        else if (key == "tool" && value >= 0 && value <= (float)Tool::Settings) ui_.activeTool = (Tool)(int)value;
    }
}

void Overlay::SaveSettings() const
{
    const auto path = SettingsPath();
    if (path.empty()) return;
    std::error_code ec; std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::trunc);
    out << "language " << (language == Lang::Russian ? 1 : 0) << "\n"
        << "ui_scale " << ui_.layout.uiScaleUser << "\n"
        << "panel_open " << (ui_.layout.panelOpen ? 1 : 0) << "\n"
        << "tool " << (int)ui_.activeTool << "\n";
}

void Overlay::Emit(std::uint32_t command, std::uint64_t value)
{
    if (emit_) emit_(emitUser_, command, value);
}

void Overlay::PushFont(Font role, float extraScale)
{
    ImGui::PushFont(ui_.fonts[role], ui_.fonts.size[(int)role] * ui_.rects.uiScale * extraScale);
}

// Turn snapshot changes into chronological log lines. Only real transitions
// are logged; nothing here is inferred about the game.
void Overlay::Observe(const OverlayFrame& f)
{
    auto add = [&](Tone tone, Str id, std::string text = {})
    {
        LogLine line{ {}, tone, id, std::move(text) };
        SYSTEMTIME t; GetLocalTime(&t);
        snprintf(line.clock, sizeof(line.clock), "%02u:%02u:%02u", t.wHour, t.wMinute, t.wSecond);
        log_.push_back(std::move(line));
        while (log_.size() > kMaxLog) log_.pop_front();
    };
    const auto& s = f.snapshot;
    const int linked = f.hostLinked ? 1 : 0;
    if (linked != lastLinked_)
    {
        add(linked ? Tone::Success : Tone::Warning, linked ? Str::LogHostLinked : Str::LogHostLost);
        lastLinked_ = linked;
    }
    if (!f.hostLinked) return;
    if ((int)s.connected != lastConnected_) { add(s.connected ? Tone::Success : Tone::Warning, s.connected ? Str::LogGameConnected : Str::LogGameWaiting); lastConnected_ = (int)s.connected; }
    if ((int)s.player_found != lastPlayer_) { add(s.player_found ? Tone::Success : Tone::Neutral, s.player_found ? Str::LogPlayerFound : Str::LogPlayerLost); lastPlayer_ = (int)s.player_found; }
    if ((int)s.loaded != lastLoaded_) { add(Tone::Info, s.loaded ? Str::ReplayLoaded : Str::NoReplay); lastLoaded_ = (int)s.loaded; }
    const std::string diagnostic(s.diagnostic, strnlen(s.diagnostic, sizeof(s.diagnostic)));
    if (!diagnostic.empty() && diagnostic != lastDiagnostic_)
    {
        add(diagnostic.find("ERROR") != std::string::npos ? Tone::Error : Tone::Neutral, Str::Count, diagnostic);
        lastDiagnostic_ = diagnostic;
    }
    if (s.recording_state != lastRecording_)
    {
        static constexpr Str names[] = { Str::LogRecStopped, Str::LogRecStarted, Str::LogRecPaused, Str::LogRecSaving };
        add(s.recording_state == theater_ui::record_recording ? Tone::Live : Tone::Info, names[std::min<std::uint32_t>(s.recording_state, 3)]);
        lastRecording_ = s.recording_state;
    }
    if (f.nativeStatus != lastNative_)
    {
        if (!f.nativeStatus.empty())
            add(f.nativeStatus.find("ERROR") != std::string::npos ? Tone::Error : Tone::Accent, Str::Count, f.nativeStatus);
        lastNative_ = f.nativeStatus;
    }
}

const LayoutRects& Overlay::Draw(const OverlayFrame& f, EmitFn emit, void* user)
{
    emit_ = emit; emitUser_ = user;
    ImGuiIO& io = ImGui::GetIO();
    ui_.visibility = f.visibility;
    ui_.hiddenAt = f.hiddenAt;

    // The overlay does not composite the game picture yet, so the game always
    // renders full screen behind the panels. 16:9 matches vanilla Elden Ring.
    const float gameAspect = 16.0f / 9.0f;
    ui_.rects = SolveLayout(io.DisplaySize, gameAspect, ui_.layout, ui_.visibility);
    if (ui_.rects.uiScale != appliedScale_)
    {
        ApplyStyle(ImGui::GetStyle(), ui_.rects.uiScale);
        auto& st = ImGui::GetStyle();
        st.FontScaleDpi = 1.0f; st.FontScaleMain = 1.0f;
        st.Colors[ImGuiCol_WindowBg] = Color::PanelBgSolid.Vec4();
        st.Colors[ImGuiCol_TabSelected] = Color::SelectedBg.Vec4();
        st.Colors[ImGuiCol_NavCursor] = Color::BorderActive.Vec4();
        appliedScale_ = ui_.rects.uiScale;
    }

    Observe(f);
    if (ui_.visibility == UiVisibility::Shown)
    {
        PushFont(Font::Body);
        DrawRail(f);
        if (ui_.layout.panelOpen) DrawPanel(f);
        DrawSequencer(f);
        ImGui::PopFont();
        DrawCursor();
    }
    else DrawHiddenHint(f);
    if (ui_.visibility != UiVisibility::HiddenClean) DrawRecordingPill(f);
    return ui_.rects;
}

void Overlay::DrawRail(const OverlayFrame& f)
{
    const float s = ui_.rects.uiScale;
    ImGui::SetNextWindowPos(ui_.rects.railMin);
    ImGui::SetNextWindowSize(ImVec2(ui_.rects.railMax.x - ui_.rects.railMin.x, ui_.rects.railMax.y - ui_.rects.railMin.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Color::PanelBgSolid.Vec4());
    ImGui::Begin("##theater_rail", nullptr, kRegion);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetWindowPos();
    const float w = ui_.rects.railMax.x - ui_.rects.railMin.x, btn = Px(Metric::RailButton, s);
    dl->AddLine(ImVec2(o.x + w - 1, o.y), ImVec2(o.x + w - 1, ui_.rects.railMax.y), Color::BorderSubtle.U32());

    // Brand diamond.
    const ImVec2 c(o.x + w * 0.5f, o.y + Px(26, s));
    const float r = Px(9, s);
    dl->AddQuadFilled(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y), Color::AccentGold.U32());

    struct Item { Tool tool; std::uint16_t glyph; Str label; bool available; };
    const Item top[] = {
        { Tool::Scene, Glyph::Scene, Str::Scene, true }, { Tool::Camera, Glyph::Camera, Str::Camera, true },
        { Tool::Look, Glyph::Look, Str::Look, true }, { Tool::Replays, Glyph::Replays, Str::Replays, true },
        { Tool::Export, Glyph::Export, Str::Export, true } };
    const Item bottom[] = { { Tool::Debug, Glyph::Debug, Str::Debug, true }, { Tool::Settings, Glyph::Settings, Str::Settings, true } };

    auto railButton = [&](const Item& it, float y, bool warning)
    {
        const ImVec2 min(o.x + (w - btn) * 0.5f, y), max(min.x + btn, y + btn);
        ImGui::SetCursorScreenPos(min);
        ImGui::PushID((int)it.tool);
        const bool clicked = ImGui::InvisibleButton("##rail", ImVec2(btn, btn));
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        const bool active = ui_.layout.panelOpen && ui_.activeTool == it.tool;
        if (active) dl->AddRectFilled(min, max, Color::SelectedBg.U32(), Px(Metric::CornerRadius, s));
        else if (hovered) dl->AddRectFilled(min, max, Color::HoverBg.U32(), Px(Metric::CornerRadius, s));
        if (active) dl->AddRectFilled(ImVec2(o.x, min.y + Px(8, s)), ImVec2(o.x + Px(3, s), max.y - Px(8, s)), Color::AccentBlue.U32());
        const Rgba tint = active ? Color::TextPrimary : hovered ? Color::TextPrimary : Color::TextSecondary;
        char icon[4];
        IconUtf8(it.glyph, icon);
        const float iconSize = Px(FontSize::IconRail, s);
        ImFont* iconFont = iconFont_ ? iconFont_ : ui_.fonts[Font::Body];
        const ImVec2 isz = iconFont->CalcTextSizeA(iconSize, FLT_MAX, 0, icon);
        dl->AddText(iconFont, iconSize, ImVec2(min.x + (btn - isz.x) * 0.5f, min.y + Px(7, s)), tint.U32(), icon);
        const float labelSize = Px(FontSize::RailLabel, s);
        const char* label = T(it.label);
        const ImVec2 lsz = ui_.fonts[Font::RailLabel]->CalcTextSizeA(labelSize, FLT_MAX, 0, label);
        dl->AddText(ui_.fonts[Font::RailLabel], labelSize, ImVec2(min.x + (btn - std::min(lsz.x, btn)) * 0.5f, max.y - Px(16, s)), tint.U32(), label);
        if (warning) dl->AddCircleFilled(ImVec2(max.x - Px(9, s), min.y + Px(9, s)), Px(3.5f, s), Color::AccentAmber.U32());
        if (clicked)
        {
            if (active) ui_.layout.panelOpen = false;           // clicking the active tool closes the panel
            else { ui_.activeTool = it.tool; ui_.layout.panelOpen = true; }
            SaveSettings();
        }
    };

    float y = o.y + Px(52, s);
    for (const auto& it : top) { railButton(it, y, false); y += btn + Px(4, s); }

    const bool nativeWarning = f.nativeStatus.find("ERROR") != std::string::npos;
    float yb = ui_.rects.railMax.y - Px(12, s) - (btn + Px(4, s)) * 3;
    for (const auto& it : bottom) { railButton(it, yb, it.tool == Tool::Debug && nativeWarning); yb += btn + Px(4, s); }

    // Hide UI sits at the very bottom of the rail; the same action as F4.
    {
        const ImVec2 min(o.x + (w - btn) * 0.5f, yb), max(min.x + btn, yb + btn);
        ImGui::SetCursorScreenPos(min);
        const bool clicked = ImGui::InvisibleButton("##hide", ImVec2(btn, btn));
        const bool hovered = ImGui::IsItemHovered();
        if (hovered) dl->AddRectFilled(min, max, Color::HoverBg.U32(), Px(Metric::CornerRadius, s));
        char icon[4]; IconUtf8(Glyph::HideUi, icon);
        const float iconSize = Px(FontSize::IconRail, s);
        ImFont* iconFont = iconFont_ ? iconFont_ : ui_.fonts[Font::Body];
        const ImVec2 isz = iconFont->CalcTextSizeA(iconSize, FLT_MAX, 0, icon);
        const Rgba tint = hovered ? Color::TextPrimary : Color::TextSecondary;
        dl->AddText(iconFont, iconSize, ImVec2(min.x + (btn - isz.x) * 0.5f, min.y + Px(7, s)), tint.U32(), icon);
        const char* label = "F4";
        const float labelSize = Px(FontSize::RailLabel, s);
        const ImVec2 lsz = ui_.fonts[Font::RailLabel]->CalcTextSizeA(labelSize, FLT_MAX, 0, label);
        dl->AddText(ui_.fonts[Font::RailLabel], labelSize, ImVec2(min.x + (btn - lsz.x) * 0.5f, max.y - Px(16, s)), tint.U32(), label);
        if (hovered) ImGui::SetTooltip("%s", T(Str::HideUiHint));
        if (clicked) Emit(kCommandToggleUi); // backend handles the visibility toggle
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void Overlay::DrawPanel(const OverlayFrame& f)
{
    const float s = ui_.rects.uiScale;
    const auto& snap = f.snapshot;
    ImGui::SetNextWindowPos(ui_.rects.panelMin);
    ImGui::SetNextWindowSize(ImVec2(ui_.rects.panelMax.x - ui_.rects.panelMin.x, ui_.rects.panelMax.y - ui_.rects.panelMin.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(Space::LG, s), 0));
    ImGui::Begin("##theater_panel", nullptr, kRegion);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetWindowPos(), sz = ImGui::GetWindowSize();
    dl->AddLine(ImVec2(o.x + sz.x - 1, o.y), ImVec2(o.x + sz.x - 1, o.y + sz.y), Color::BorderSubtle.U32());

    // Header: tool name in PanelTitle caps.
    const Str titles[] = { Str::Scene, Str::Camera, Str::Look, Str::Replays, Str::Export, Str::Debug, Str::Settings };
    const float header = Px(Metric::PanelHeaderHeight, s);
    dl->AddRectFilled(o, ImVec2(o.x + sz.x - 1, o.y + header), Color::HeaderBg.U32());
    dl->AddLine(ImVec2(o.x, o.y + header), ImVec2(o.x + sz.x - 1, o.y + header), Color::BorderSubtle.U32());
    PushFont(Font::PanelTitle);
    ImGui::SetCursorScreenPos(ImVec2(o.x + Px(Space::LG, s), o.y + (header - ImGui::GetFontSize()) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
    ImGui::TextUnformatted(T(titles[std::min<int>((int)ui_.activeTool, 6)]));
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(o.x + Px(Space::LG, s), o.y + header + Px(Space::MD, s)));

    auto section = [&](const char* title)
    {
        ImGui::Dummy(ImVec2(0, Px(Space::SM, s)));
        PushFont(Font::PanelTitle);
        ImGui::PushStyleColor(ImGuiCol_Text, Color::TextMuted.Vec4());
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    };
    const float valueColumn = Px(Space::LG, s) + (sz.x - 2 * Px(Space::LG, s)) * Metric::PropertyLabelFrac;
    auto row = [&](const char* label, const char* value, Rgba valueColor)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::SameLine(valueColumn);
        ImGui::PushStyleColor(ImGuiCol_Text, valueColor.Vec4());
        ImGui::TextUnformatted(value);
        ImGui::PopStyleColor();
    };
    auto note = [&](Str text)
    {
        PushFont(Font::Meta);
        ImGui::PushStyleColor(ImGuiCol_Text, Color::TextMuted.Vec4());
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(T(text));
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::PopFont();
    };
    auto status = [&](Str label, bool ok, Str good, Str bad)
    {
        row(T(label), T(ok ? good : bad), ok ? Color::AccentGreen : Color::AccentAmber);
    };

    switch (ui_.activeTool)
    {
    case Tool::Scene:
    {
        section(T(Str::Connection));
        status(Str::Game, f.hostLinked && snap.connected, Str::Connected, Str::Waiting);
        status(Str::Player, f.hostLinked && snap.player_found, Str::Found, Str::Waiting);
        char pos[96];
        snprintf(pos, sizeof(pos), "%.2f  %.2f  %.2f", snap.live_position[0], snap.live_position[1], snap.live_position[2]);
        PushFont(Font::Mono); row(T(Str::LivePosition), pos, Color::TextPrimary); ImGui::PopFont();

        section(T(Str::Actors));
        const float listH = Px(Metric::TreeRowHeight, s) * std::min<std::uint32_t>(std::max<std::uint32_t>(snap.count, 1), 6);
        ImGui::BeginChild("##actors", ImVec2(0, listH));
        if (!snap.count) { PushFont(Font::Meta); ImGui::TextDisabled("-"); ImGui::PopFont(); }
        for (unsigned i = 0; i < std::min<std::uint32_t>(snap.count, 16); ++i)
        {
            const auto& a = snap.actors[i];
            char label[160];
            snprintf(label, sizeof(label), T(Str::ActorRow), (unsigned long long)a.id, a.entity, a.npc);
            ImGui::PushID((int)i);
            if (ImGui::Selectable(label, a.id == snap.selected, 0, ImVec2(0, Px(Metric::TreeRowHeight, s) - ImGui::GetStyle().ItemSpacing.y)))
                Emit(theater_ui::select, a.id);
            ImGui::PopID();
        }
        ImGui::EndChild();
        if (snap.total > 16)
        {
            ImGui::BeginDisabled(snap.offset == 0);
            if (ImGui::Button(T(Str::Previous))) Emit(theater_ui::page, snap.offset >= 16 ? snap.offset - 16 : 0);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(snap.offset + 16 >= snap.total);
            if (ImGui::Button(T(Str::NextPage))) Emit(theater_ui::page, std::min<std::uint64_t>(snap.offset + 16, snap.total));
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("%u-%u / %u", snap.offset + 1, snap.offset + snap.count, snap.total);
        }
        break;
    }
    case Tool::Camera: section(T(Str::NotYetAvailable)); note(Str::CameraNotes); break;
    case Tool::Look: section(T(Str::NotYetAvailable)); note(Str::LookNotes); break;
    case Tool::Export: section(T(Str::NotYetAvailable)); note(Str::ExportNotes); break;
    case Tool::Replays:
    {
        section(T(Str::Replays));
        row(T(Str::Replays), T(snap.loaded ? Str::ReplayLoaded : Str::NoReplay), snap.loaded ? Color::AccentGreen : Color::TextSecondary);
        char d[32]; FormatTime(snap.duration_ns / 1e9, d, sizeof(d));
        PushFont(Font::Mono); row(T(Str::Duration), d, Color::TextPrimary); ImGui::PopFont();
        note(Str::ReplaysNotes);
        const bool rec = IsRecording(snap);
        ImGui::Dummy(ImVec2(0, Px(Space::SM, s)));
        char label[96]; char icon[4];
        snprintf(label, sizeof(label), "%s  %s", IconUtf8(rec ? Glyph::Stop : Glyph::Record, icon), T(rec ? Str::StopRecording : Str::Record));
        if (FlatButton("rec_panel", label, ImVec2(-1, Px(Metric::TextButtonHeight, s)), rec ? Color::TintRed : Color::FrameBg, rec ? Color::AccentRed : Color::TextPrimary, f.hostLinked))
            Emit(rec ? theater_ui::record_stop : theater_ui::record_start);

        // Library: newest first, one page at a time from the host. Click a row to open it.
        section(T(Str::Library));
        const unsigned shown = std::min<std::uint32_t>(snap.replay_count, theater_ui::replay_page_size);
        if (!shown) note(Str::NoReplays);
        const float rowH = Px(Metric::ListRowHeight, s) + Px(8, s);
        const bool canOpen = f.hostLinked && !snap.active && !rec;
        for (unsigned i = 0; i < shown; ++i)
        {
            const auto& e = snap.replays[i];
            const ImVec2 p0 = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x;
            ImGui::PushID((int)e.index);
            ImGui::BeginDisabled(!canOpen);
            const bool clicked = ImGui::Selectable("##replay", e.loaded != 0, 0, ImVec2(w, rowH));
            ImGui::EndDisabled();
            ImGui::PopID();
            ImDrawList* dl2 = ImGui::GetWindowDrawList();
            dl2->PushClipRect(p0, ImVec2(p0.x + w, p0.y + rowH), true);
            PushFont(Font::BodyStrong);
            dl2->AddText(ImVec2(p0.x + Px(6, s), p0.y + Px(3, s)), (canOpen ? Color::TextPrimary : Color::TextMuted).U32(), e.name);
            ImGui::PopFont();
            char meta[96], dur[32];
            FormatTime(e.duration_ns / 1e9, dur, sizeof(dur));
            snprintf(meta, sizeof(meta), "%s   %.1f MB", dur, e.bytes / (1024.0 * 1024.0));
            PushFont(Font::MonoSmall);
            dl2->AddText(ImVec2(p0.x + Px(6, s), p0.y + rowH * 0.5f + Px(1, s)), Color::TextMuted.U32(), meta);
            if (e.loaded)
            {
                const char* tag = T(Str::LoadedTag);
                const ImVec2 tsz = ImGui::CalcTextSize(tag);
                dl2->AddText(ImVec2(p0.x + w - tsz.x - Px(8, s), p0.y + rowH * 0.5f + Px(1, s)), Color::AccentGreen.U32(), tag);
            }
            ImGui::PopFont();
            dl2->PopClipRect();
            if (clicked && canOpen && !e.loaded) { Emit(theater_ui::replay_open, e.index); }
        }
        if (snap.replay_total > theater_ui::replay_page_size)
        {
            ImGui::BeginDisabled(snap.replay_offset == 0);
            if (ImGui::Button(T(Str::Previous))) Emit(theater_ui::replay_page, snap.replay_offset >= theater_ui::replay_page_size ? snap.replay_offset - theater_ui::replay_page_size : 0);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(snap.replay_offset + theater_ui::replay_page_size >= snap.replay_total);
            if (ImGui::Button(T(Str::NextPage))) Emit(theater_ui::replay_page, snap.replay_offset + theater_ui::replay_page_size);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("%u-%u / %u", snap.replay_offset + 1, snap.replay_offset + shown, snap.replay_total);
        }
        break;
    }
    case Tool::Debug:
    {
        section(T(Str::NativeGhost));
        const std::string& n = f.nativeStatus;
        const Rgba nc = n.find("ERROR") != std::string::npos ? Color::AccentRed : n.find("waiting") != std::string::npos ? Color::AccentAmber : Color::AccentBlue;
        PushFont(Font::Mono);
        ImGui::PushStyleColor(ImGuiCol_Text, (n.empty() ? Color::TextMuted : nc).Vec4());
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(n.empty() ? T(Str::NativeGhostNone) : n.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::PopFont();
        section(T(Str::Diagnostic));
        PushFont(Font::Mono);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(snap.diagnostic[0] ? snap.diagnostic : "-");
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
        section(T(Str::Connection));
        status(Str::Game, f.hostLinked && snap.connected, Str::Connected, Str::Waiting);
        status(Str::Player, f.hostLinked && snap.player_found, Str::Found, Str::Waiting);
        break;
    }
    case Tool::Settings:
    default:
    {
        section(T(Str::Language));
        if (ImGui::RadioButton("English", language == Lang::English)) { language = Lang::English; SaveSettings(); }
        ImGui::SameLine();
        if (ImGui::RadioButton("Русский", language == Lang::Russian)) { language = Lang::Russian; SaveSettings(); }
        section(T(Str::UiScale));
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##scale", &ui_.layout.uiScaleUser, 0.75f, 1.5f, "%.2fx")) {}
        if (ImGui::IsItemDeactivatedAfterEdit()) SaveSettings();
        section(T(Str::HotkeysTitle));
        PushFont(Font::Meta);
        ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
        ImGui::TextUnformatted(T(Str::HotkeysBody));
        ImGui::PopStyleColor();
        ImGui::PopFont();
        break;
    }
    }

    // The event log fills whatever height is left, so the panel never shows empty space.
    ImGui::Dummy(ImVec2(0, Px(Space::MD, s)));
    section(T(Str::EventLog));
    const float remaining = ImGui::GetContentRegionAvail().y - Px(Space::MD, s);
    DrawEventLog(std::max(remaining, Px(60, s)));
    ImGui::End();
    ImGui::PopStyleVar();
}

void Overlay::DrawEventLog(float height)
{
    const float s = ui_.rects.uiScale;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color::ChildBg.Vec4());
    ImGui::BeginChild("##log", ImVec2(0, height), ImGuiChildFlags_Borders);
    PushFont(Font::MonoSmall);
    if (log_.empty()) ImGui::TextDisabled("%s", T(Str::EventLogEmpty));
    ImGuiListClipper clip;
    clip.Begin((int)log_.size(), Px(Metric::LogLineHeight, s));
    while (clip.Step())
        for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i)
        {
            const auto& l = log_[(size_t)i];
            ImGui::PushStyleColor(ImGuiCol_Text, Color::TextMuted.Vec4());
            ImGui::TextUnformatted(l.clock);
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ColorsFor(l.tone).fg.Vec4());
            ImGui::TextUnformatted(l.id == Str::Count ? l.text.c_str() : T(l.id));
            ImGui::PopStyleColor();
        }
    // Stay pinned to the newest line unless the user scrolled up.
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - Px(Metric::LogLineHeight, s)) ImGui::SetScrollHereY(1.0f);
    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void Overlay::DrawSequencer(const OverlayFrame& f)
{
    const float s = ui_.rects.uiScale;
    const ImVec2 min = ui_.rects.sequencerMin, max = ui_.rects.sequencerMax;
    ImGui::SetNextWindowPos(min);
    ImGui::SetNextWindowSize(ImVec2(max.x - min.x, max.y - min.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Color::TimelineBg.Alpha(255).Vec4());
    ImGui::Begin("##theater_sequencer", nullptr, kRegion);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddLine(min, ImVec2(max.x, min.y), Color::Border.U32());
    const float toolbar = Px(Layout.SequencerToolbar, s);
    DrawToolbar(f, toolbar);
    DrawTimeline(f, ImVec2(min.x, min.y + toolbar), max);
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void Overlay::DrawToolbar(const OverlayFrame& f, float height)
{
    const float s = ui_.rects.uiScale;
    const auto& snap = f.snapshot;
    const ImVec2 o = ImGui::GetWindowPos();
    const float w = ImGui::GetWindowSize().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(o, ImVec2(o.x + w, o.y + height), Color::HeaderBg.U32());
    dl->AddLine(ImVec2(o.x, o.y + height - 1), ImVec2(o.x + w, o.y + height - 1), Color::BorderSubtle.U32());

    const float bh = Px(30, s), y = o.y + (height - bh) * 0.5f;
    float x = o.x + Px(Space::LG, s);

    // State badge.
    const Badge badge = BadgeFor(snap, f.hostLinked);
    const ToneColors tc = ColorsFor(badge.tone);
    PushFont(Font::MonoSmall);
    const char* badgeText = T(badge.label);
    const ImVec2 bsz = ImGui::CalcTextSize(badgeText);
    const float badgeH = Px(Metric::BadgeHeight, s), badgeW = bsz.x + Px(16, s);
    const ImVec2 b0(x, o.y + (height - badgeH) * 0.5f), b1(x + badgeW, b0.y + badgeH);
    dl->AddRectFilled(b0, b1, tc.fill.U32(), Px(Radius::Badge, s));
    dl->AddRect(b0, b1, tc.border.U32(), Px(Radius::Badge, s));
    dl->AddText(ImVec2(b0.x + Px(8, s), b0.y + (badgeH - bsz.y) * 0.5f), tc.fg.U32(), badgeText);
    ImGui::PopFont();
    x += badgeW + Px(Space::LG, s);

    // Transport. The owner asked for Play/Stop without Pause.
    const bool canTransport = f.hostLinked && snap.loaded;
    char icon[4];
    auto iconButton = [&](const char* id, std::uint16_t glyph, const char* tip, bool enabled, Rgba fill, Rgba text, float width) -> bool
    {
        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::PushFont(iconFont_ ? iconFont_ : ui_.fonts[Font::Body], Px(FontSize::Icon, s));
        const bool clicked = FlatButton(id, IconUtf8(glyph, icon), ImVec2(width, bh), fill, text, enabled);
        ImGui::PopFont();
        Tooltip(tip);
        x += width + Px(Space::SM, s);
        return clicked;
    };
    const float ib = Px(Metric::IconButtonSize, s);
    if (iconButton("restart", Glyph::Restart, T(Str::Restart), canTransport, Rgba{ 0, 0, 0, 0 }, Color::TextPrimary, ib)) Emit(theater_ui::restart);
    if (iconButton("prev", Glyph::StepBack, T(Str::StepBack), canTransport, Rgba{ 0, 0, 0, 0 }, Color::TextPrimary, ib)) Emit(theater_ui::previous);
    const bool playing = snap.phase == 2;
    if (iconButton("play", Glyph::Play, T(Str::Play), canTransport && !playing, Color::AccentBlue, Color::TextOnAccent, Px(36, s))) Emit(theater_ui::play);
    if (iconButton("stop", Glyph::Stop, "Stop / F6", f.hostLinked, Rgba{ 0, 0, 0, 0 }, Color::TextPrimary, ib)) Emit(theater_ui::stop);
    if (iconButton("next", Glyph::StepFwd, T(Str::StepForward), canTransport, Rgba{ 0, 0, 0, 0 }, Color::TextPrimary, ib)) Emit(theater_ui::next);
    x += Px(Space::MD, s);

    // Timecode.
    char now[32], total[32], tcText[80];
    FormatTime(snap.time_ns / 1e9, now, sizeof(now));
    FormatTime(snap.duration_ns / 1e9, total, sizeof(total));
    PushFont(Font::Timecode);
    const ImVec2 tsz = ImGui::CalcTextSize(now);
    dl->AddText(ImVec2(x, o.y + (height - tsz.y) * 0.5f), Color::TextPrimary.U32(), now);
    x += tsz.x;
    ImGui::PopFont();
    PushFont(Font::Mono);
    snprintf(tcText, sizeof(tcText), " / %s", total);
    const ImVec2 dsz = ImGui::CalcTextSize(tcText);
    dl->AddText(ImVec2(x, o.y + (height - dsz.y) * 0.5f + Px(1, s)), Color::TextMuted.U32(), tcText);
    x += dsz.x + Px(Space::LG, s);
    ImGui::PopFont();

    // Speed.
    int selected = 3;
    for (int i = 0; i < 6; ++i) if (std::fabs(snap.playback_speed * 100.0 - (double)kSpeeds[i]) < 0.5) selected = i;
    ImGui::SetCursorScreenPos(ImVec2(x, o.y + (height - ImGui::GetFrameHeight()) * 0.5f));
    ImGui::SetNextItemWidth(Px(84, s));
    ImGui::BeginDisabled(!canTransport);
    if (ImGui::Combo("##speed", &selected, kSpeedLabels, 6)) Emit(theater_ui::speed, kSpeeds[selected]);
    ImGui::EndDisabled();
    Tooltip(T(Str::Speed));

    // Right side: Record, then Hide UI.
    const bool rec = IsRecording(snap);
    char recLabel[96], hideLabel[96];
    snprintf(recLabel, sizeof(recLabel), "%s  %s", IconUtf8(rec ? Glyph::Stop : Glyph::Record, icon), rec ? T(Str::StopRecording) : T(Str::Record));
    char hideIcon[4];
    snprintf(hideLabel, sizeof(hideLabel), "%s  %s", IconUtf8(Glyph::HideUi, hideIcon), T(Str::HideUiHint));
    const float pad = ImGui::GetStyle().FramePadding.x * 2;
    const float hideW = ImGui::CalcTextSize(hideLabel).x + pad, recW = ImGui::CalcTextSize(recLabel).x + pad;
    float rx = o.x + w - Px(Space::LG, s) - hideW;
    ImGui::SetCursorScreenPos(ImVec2(rx, y));
    if (FlatButton("hide", hideLabel, ImVec2(hideW, bh), Color::FrameBg, Color::TextSecondary)) Emit(kCommandToggleUi);
    rx -= recW + Px(Space::MD, s);
    ImGui::SetCursorScreenPos(ImVec2(rx, y));
    if (FlatButton("rec", recLabel, ImVec2(recW, bh), rec ? Color::TintRed : Color::FrameBg, rec ? Color::AccentRed : Color::TextPrimary, f.hostLinked))
        Emit(rec ? theater_ui::record_stop : theater_ui::record_start);
    Tooltip(rec ? "F6" : "F5");
}

void Overlay::DrawTimeline(const OverlayFrame& f, ImVec2 min, ImVec2 max)
{
    const float s = ui_.rects.uiScale;
    const auto& snap = f.snapshot;
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto& tv = ui_.timeline;

    const float tree = Px(ui_.layout.trackTreeWidth, s), ruler = Px(Layout.RulerHeight, s), nav = Px(Layout.NavigatorHeight, s);
    const float rowH = Px(Metric::TrackRowHeight, s);
    const ImVec2 laneMin(min.x + tree, min.y + ruler), laneMax(max.x, max.y - nav);
    const float laneW = std::max(1.0f, laneMax.x - laneMin.x);
    const double duration = snap.duration_ns / 1e9, now = snap.time_ns / 1e9;

    // Fit the whole replay when a new one appears.
    if (snap.duration_ns != lastDuration_)
    {
        lastDuration_ = snap.duration_ns;
        tv.viewStart = 0.0;
        tv.pixelsPerSecond = duration > 0 ? laneW / (duration * 1.02) : 40.0 * s;
        tv.scrollY = 0;
    }
    const double minPps = duration > 0 ? laneW / (duration * 1.5) : 1.0, maxPps = 2000.0 * s;
    tv.pixelsPerSecond = std::clamp(tv.pixelsPerSecond, std::min(minPps, maxPps), maxPps);
    auto clampView = [&]
    {
        const double span = laneW / tv.pixelsPerSecond;
        tv.viewStart = std::clamp(tv.viewStart, std::min(0.0, duration - span), std::max(0.0, duration - span * 0.5));
    };

    // Rows: honest content only. Rows without data stay empty.
    struct Row { std::string name; Rgba color; int depth; bool replayBar; std::uint64_t actor; bool group; };
    std::vector<Row> rows;
    rows.push_back({ T(Str::TrackReplay), Color::AccentGreen, 0, snap.loaded != 0, 0, false });
    rows.push_back({ T(Str::TrackCamera), Color::AccentBlue, 0, false, 0, false });
    rows.push_back({ T(Str::TrackActors), Color::AccentAmber, 0, false, 0, true });
    for (unsigned i = 0; i < std::min<std::uint32_t>(snap.count, 16); ++i)
    {
        char name[96];
        snprintf(name, sizeof(name), "Actor %llu", (unsigned long long)snap.actors[i].id);
        rows.push_back({ name, Color::AccentAmber, 1, false, snap.actors[i].id, false });
    }
    rows.push_back({ T(Str::TrackBookmarks), Color::EventNeutral, 0, false, 0, false });

    const float rowsH = laneMax.y - laneMin.y;
    const float maxScroll = std::max(0.0f, rows.size() * rowH - rowsH);

    // Input over the timeline. Wheel zooms around the mouse; Shift+Wheel scrolls tracks.
    const bool hovered = ImGui::IsWindowHovered() && io.MousePos.x >= min.x && io.MousePos.x < max.x && io.MousePos.y >= min.y && io.MousePos.y < max.y;
    if (hovered && io.MouseWheel != 0.0f)
    {
        if (io.KeyShift) tv.scrollY -= io.MouseWheel * rowH * 2;
        else if (io.MousePos.x >= laneMin.x)
        {
            const double anchor = tv.XToTime(io.MousePos.x, laneMin.x);
            tv.pixelsPerSecond = std::clamp(tv.pixelsPerSecond * std::pow(1.2, (double)io.MouseWheel), std::min(minPps, maxPps), maxPps);
            tv.viewStart = anchor - (io.MousePos.x - laneMin.x) / tv.pixelsPerSecond;
        }
    }
    tv.scrollY = std::clamp(tv.scrollY, 0.0f, maxScroll);

    // Follow the playhead during playback.
    if (snap.phase == 2 && !scrubbing_)
    {
        const double span = laneW / tv.pixelsPerSecond;
        if (now < tv.viewStart || now > tv.viewStart + span * 0.92) tv.viewStart = now - span * 0.1;
    }
    clampView();

    // Track tree header and rows.
    dl->AddRectFilled(min, ImVec2(min.x + tree, max.y), Color::PanelBgSolid.U32());
    dl->AddLine(ImVec2(min.x + tree, min.y), ImVec2(min.x + tree, max.y), Color::BorderSubtle.U32());
    PushFont(Font::PanelTitle);
    dl->AddText(ImVec2(min.x + Px(Space::LG, s), min.y + (ruler - ImGui::GetFontSize()) * 0.5f), Color::TextMuted.U32(), T(Str::Tracks));
    ImGui::PopFont();

    dl->PushClipRect(ImVec2(min.x, laneMin.y), ImVec2(max.x, laneMax.y), true);
    PushFont(Font::Body);
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const float y0 = laneMin.y + i * rowH - tv.scrollY, y1 = y0 + rowH;
        if (y1 < laneMin.y || y0 > laneMax.y) continue;
        const auto& r = rows[i];
        if (i % 2) dl->AddRectFilled(ImVec2(min.x, y0), ImVec2(max.x, y1), Color::TimelineRowAlt.U32());
        const bool selected = r.actor && r.actor == snap.selected;
        if (selected) dl->AddRectFilled(ImVec2(min.x, y0), ImVec2(min.x + tree, y1), Color::SelectedBg.U32());
        dl->AddRectFilled(ImVec2(min.x + Px(4, s) + r.depth * Px(Metric::IndentPerLevel, s), y0 + Px(6, s)),
            ImVec2(min.x + Px(7, s) + r.depth * Px(Metric::IndentPerLevel, s), y1 - Px(6, s)), r.color.U32());
        const float tx = min.x + Px(14, s) + r.depth * Px(Metric::IndentPerLevel, s);
        dl->PushClipRect(ImVec2(min.x, y0), ImVec2(min.x + tree - Px(4, s), y1), true);
        dl->AddText(ImVec2(tx, y0 + (rowH - ImGui::GetFontSize()) * 0.5f), (r.group ? Color::TextSecondary : Color::TextPrimary).U32(), r.name.c_str());
        dl->PopClipRect();
        dl->AddLine(ImVec2(min.x, y1), ImVec2(max.x, y1), Color::BorderSubtle.U32());
        if (r.replayBar && duration > 0)
        {
            const float x0 = (float)tv.TimeToX(0, laneMin.x), x1 = (float)tv.TimeToX(duration, laneMin.x);
            dl->PushClipRect(ImVec2(laneMin.x, y0), ImVec2(laneMax.x, y1), true);
            dl->AddRectFilled(ImVec2(x0, y0 + Px(6, s)), ImVec2(x1, y1 - Px(6, s)), r.color.Alpha(70).U32(), Px(2, s));
            dl->AddRect(ImVec2(x0, y0 + Px(6, s)), ImVec2(x1, y1 - Px(6, s)), r.color.Alpha(160).U32(), Px(2, s));
            dl->PopClipRect();
        }
        // Selecting an actor row uses the existing select command.
        if (r.actor && ImGui::IsWindowHovered() && io.MouseClicked[0] && io.MousePos.x < min.x + tree && io.MousePos.y >= y0 && io.MousePos.y < y1)
            Emit(theater_ui::select, r.actor);
    }
    ImGui::PopFont();
    dl->PopClipRect();

    // Ruler.
    dl->AddRectFilled(ImVec2(laneMin.x, min.y), ImVec2(laneMax.x, laneMin.y), Color::RulerBg.U32());
    dl->AddLine(ImVec2(laneMin.x, laneMin.y), ImVec2(laneMax.x, laneMin.y), Color::BorderSubtle.U32());
    static constexpr double steps[] = { 0.1, 0.25, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1800, 3600 };
    double major = steps[std::size(steps) - 1];
    for (double st : steps) if (st * tv.pixelsPerSecond >= Px(90, s)) { major = st; break; }
    const double minor = major / 5.0;
    const double t0 = std::floor(tv.viewStart / minor) * minor, t1 = tv.viewStart + laneW / tv.pixelsPerSecond;
    dl->PushClipRect(ImVec2(laneMin.x, min.y), laneMax, true);
    PushFont(Font::MonoSmall);
    for (double t = t0; t <= t1 + minor; t += minor)
    {
        const float x = (float)tv.TimeToX(t, laneMin.x);
        const bool isMajor = std::fabs(std::remainder(t, major)) < minor * 0.01;
        dl->AddLine(ImVec2(x, laneMin.y - Px(isMajor ? Metric::RulerMajorTick : Metric::RulerMinorTick, s)), ImVec2(x, laneMin.y),
            (isMajor ? Color::RulerTickMajor : Color::RulerTickMinor).U32());
        if (isMajor)
        {
            dl->AddLine(ImVec2(x, laneMin.y), ImVec2(x, laneMax.y), Color::GridLine.U32());
            char label[32]; FormatTime(std::max(0.0, t), label, sizeof(label));
            dl->AddText(ImVec2(x + Px(3, s), min.y + Px(2, s)), Color::RulerText.U32(), label);
        }
    }
    // Area past the end of the replay is dimmed.
    if (duration > 0)
    {
        const float xe = (float)tv.TimeToX(duration, laneMin.x);
        if (xe < laneMax.x) dl->AddRectFilled(ImVec2(std::max(xe, laneMin.x), min.y), laneMax, Color::InOutDim.U32());
    }

    // Playhead.
    if (snap.loaded)
    {
        const double shown = scrubbing_ ? scrubTime_ : now;
        const float x = (float)tv.TimeToX(shown, laneMin.x);
        if (x >= laneMin.x - 1 && x <= laneMax.x + 1)
        {
            dl->AddLine(ImVec2(x, min.y), ImVec2(x, laneMax.y), Color::Playhead.U32(), Px(Metric::PlayheadWidth, s));
            char label[32]; FormatTime(shown, label, sizeof(label));
            const ImVec2 lsz = ImGui::CalcTextSize(label);
            const ImVec2 p0(x - lsz.x * 0.5f - Px(4, s), min.y + Px(1, s)), p1(x + lsz.x * 0.5f + Px(4, s), laneMin.y - Px(3, s));
            dl->AddRectFilled(p0, p1, Color::PlayheadLabelBg.U32(), Px(2, s));
            dl->AddText(ImVec2(p0.x + Px(4, s), p0.y + (p1.y - p0.y - lsz.y) * 0.5f), Color::PlayheadLabelText.U32(), label);
        }
    }
    ImGui::PopFont();
    dl->PopClipRect();

    // Click or drag on the ruler or lanes seeks. Seeks are coalesced by the
    // backend queue, and sent at most every 80 ms while dragging.
    ImGui::SetCursorScreenPos(ImVec2(laneMin.x, min.y));
    ImGui::InvisibleButton("##scrub", ImVec2(laneW, std::max(1.0f, laneMax.y - min.y)));
    const bool canSeek = f.hostLinked && snap.loaded && duration > 0;
    if (canSeek && ImGui::IsItemActive())
    {
        scrubbing_ = true;
        scrubTime_ = std::clamp(tv.XToTime(io.MousePos.x, laneMin.x), 0.0, duration);
        if (f.now - lastScrubSent_ > 0.08) { Emit(theater_ui::seek, (std::uint64_t)(scrubTime_ * 1e9)); lastScrubSent_ = f.now; }
    }
    else if (scrubbing_)
    {
        scrubbing_ = false;
        if (canSeek) Emit(theater_ui::seek, (std::uint64_t)(scrubTime_ * 1e9));
    }

    // Navigator: the whole replay, the visible window, and the playhead. Drag to scroll.
    const ImVec2 n0(laneMin.x, laneMax.y), n1(laneMax.x, max.y);
    dl->AddRectFilled(n0, n1, Color::RulerBg.U32());
    dl->AddLine(n0, ImVec2(n1.x, n0.y), Color::BorderSubtle.U32());
    if (duration > 0)
    {
        const double k = laneW / duration;
        const float v0 = n0.x + (float)(std::max(0.0, tv.viewStart) * k);
        const float v1 = n0.x + (float)(std::min(duration, tv.viewStart + laneW / tv.pixelsPerSecond) * k);
        dl->AddRectFilled(ImVec2(v0, n0.y + Px(3, s)), ImVec2(std::max(v1, v0 + Px(6, s)), n1.y - Px(3, s)), Color::BorderStrong.U32(), Px(2, s));
        const float px = n0.x + (float)(now * k);
        dl->AddLine(ImVec2(px, n0.y + Px(2, s)), ImVec2(px, n1.y - Px(2, s)), Color::Playhead.U32(), Px(2, s));
        ImGui::SetCursorScreenPos(n0);
        ImGui::InvisibleButton("##navigator", ImVec2(laneW, std::max(1.0f, n1.y - n0.y)));
        if (ImGui::IsItemActivated())
        {
            draggingNavigator_ = true;
            navigatorGrab_ = (io.MousePos.x >= v0 && io.MousePos.x <= v1) ? io.MousePos.x - v0 : (v1 - v0) * 0.5f;
        }
        if (draggingNavigator_ && ImGui::IsItemActive())
            tv.viewStart = (io.MousePos.x - navigatorGrab_ - n0.x) / k;
        if (!ImGui::IsItemActive()) draggingNavigator_ = false;
        clampView();
    }
    dl->AddRectFilled(ImVec2(min.x, n0.y), ImVec2(min.x + tree, max.y), Color::PanelBgSolid.U32());
}

void Overlay::DrawRecordingPill(const OverlayFrame& f)
{
    const auto& snap = f.snapshot;
    if (!IsRecording(snap)) return;
    const float s = ui_.rects.uiScale;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const Str label = snap.recording_state == theater_ui::record_saving ? Str::RecordingSaving
                    : snap.recording_state == theater_ui::record_paused ? Str::RecordingPaused : Str::Recording;
    char elapsed[32], text[128];
    FormatTime(snap.recording_ns / 1e9, elapsed, sizeof(elapsed));
    snprintf(text, sizeof(text), "%s  %s  %llu", T(label), elapsed, (unsigned long long)snap.recording_samples);
    ImFont* font = ui_.fonts[Font::Mono];
    const float size = Px(FontSize::Mono, s) * 1.1f;
    const ImVec2 tsz = font->CalcTextSizeA(size, FLT_MAX, 0, text);
    const float h = Px(Metric::RecPillHeight, s), dot = Px(5, s);
    const ImVec2 p0(ui_.rects.gameMin.x + Px(24, s), ui_.rects.gameMin.y + Px(24, s));
    const ImVec2 p1(p0.x + h * 0.5f + dot * 2 + Px(10, s) + tsz.x + Px(16, s), p0.y + h);
    dl->AddRectFilled(p0, p1, Color::OverlayBg.Alpha(210).U32(), h * 0.5f);
    dl->AddRect(p0, p1, Color::AccentRed.Alpha(150).U32(), h * 0.5f);
    const bool blink = snap.recording_state != theater_ui::record_recording || std::fmod(f.now, 1.0) < 0.6;
    if (blink) dl->AddCircleFilled(ImVec2(p0.x + h * 0.5f + dot * 0.5f, p0.y + h * 0.5f), dot, Color::AccentRed.U32());
    dl->AddText(font, size, ImVec2(p0.x + h * 0.5f + dot * 2 + Px(8, s), p0.y + (h - tsz.y) * 0.5f), Color::TextPrimary.U32(), text);
}

// The overlay's own arrow cursor. The game hides the Windows cursor and can hold the mouse
// through DirectInput, so neither the OS cursor nor ImGui's software cursor is reliable here.
void Overlay::DrawCursor()
{
    const ImVec2 p = ImGui::GetIO().MousePos;
    if (!ImGui::IsMousePosValid(&p)) return;
    const float s = std::max(1.0f, ui_.rects.uiScale * 1.25f);
    // Classic arrow, tip at the mouse position.
    const ImVec2 pts[] = { {0, 0}, {0, 17}, {4.5f, 13}, {7.5f, 20}, {10.5f, 18.8f}, {7.5f, 12}, {12.5f, 12} };
    ImVec2 poly[7];
    for (int i = 0; i < 7; ++i) poly[i] = ImVec2(p.x + pts[i].x * s, p.y + pts[i].y * s);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 shadow[7];
    for (int i = 0; i < 7; ++i) shadow[i] = ImVec2(poly[i].x + 1.5f, poly[i].y + 1.5f);
    dl->AddConcavePolyFilled(shadow, 7, IM_COL32(0, 0, 0, 90));
    dl->AddConcavePolyFilled(poly, 7, Color::TextPrimary.U32());
    dl->AddPolyline(poly, 7, Color::AppBg.U32(), ImDrawFlags_Closed, std::max(1.0f, s));
}

void Overlay::DrawHiddenHint(const OverlayFrame& f)
{
    const double age = f.now - f.hiddenAt;
    if (ui_.visibility != UiVisibility::Hidden || age < 0 || age > 2.4) return;
    const float alpha = age < 2.0 ? 1.0f : (float)(1.0 - (age - 2.0) / 0.4);
    const float s = ui_.rects.uiScale;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const char* text = T(Str::ShowUiHint);
    ImFont* font = ui_.fonts[Font::Body];
    const float size = Px(FontSize::Body, s);
    const ImVec2 tsz = font->CalcTextSizeA(size, FLT_MAX, 0, text);
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImVec2 p0(display.x * 0.5f - tsz.x * 0.5f - Px(14, s), display.y - Px(72, s));
    const ImVec2 p1(display.x * 0.5f + tsz.x * 0.5f + Px(14, s), p0.y + Px(Metric::ToastHeight, s));
    dl->AddRectFilled(p0, p1, Color::OverlayBg.Fade(alpha).U32(), Px(Radius::Toast, s));
    dl->AddText(font, size, ImVec2(p0.x + Px(14, s), p0.y + (p1.y - p0.y - tsz.y) * 0.5f), Color::TextPrimary.Fade(alpha).U32(), text);
}
}

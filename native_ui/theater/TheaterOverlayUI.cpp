#include "TheaterOverlayUI.h"
#include "TheaterSounds.h"
#include "imgui_internal.h"

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iterator>
#include <vector>

namespace TheaterUI
{
namespace
{
    using namespace Theme;

    // Movable, resizable panels with a title bar (the grab area). Scrolling is handled inside.
    constexpr ImGuiWindowFlags kPanel = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    constexpr ImGuiWindowFlags kRegion = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

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
    // Panel positions, sizes, collapsed and docked state are saved by ImGui in their own file.
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    {
        static std::string layoutIni;
        const auto settings = SettingsPath();
        wchar_t off[4]{};
        const bool disabled = GetEnvironmentVariableW(L"THEATER_OVERLAY_NO_LAYOUT_FILE", off, 4) > 0; // tests
        if (disabled) io.IniFilename = nullptr;
        else if (!settings.empty()) { layoutIni = (settings.parent_path() / L"TheaterOverlayLayout.ini").string(); io.IniFilename = layoutIni.c_str(); }
    }
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
        else if (key == "show_tools") showTools_ = value != 0;
        else if (key == "show_timeline") showTimeline_ = value != 0;
        else if (key == "sound_enabled") Sound::SetEnabled(value != 0);
        else if (key == "sound_volume") Sound::SetVolume(std::clamp(value, 0.0f, 1.0f));
        else if (key == "replay_options") gReplayOptions = (std::uint32_t)value;
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
        << "tool " << (int)ui_.activeTool << "\n"
        << "show_tools " << (showTools_ ? 1 : 0) << "\n"
        << "show_timeline " << (showTimeline_ ? 1 : 0) << "\n"
        << "sound_enabled " << (Sound::Enabled() ? 1 : 0) << "\n"
        << "sound_volume " << Sound::Volume() << "\n"
        << "replay_options " << gReplayOptions.load() << "\n";
}

void Overlay::Emit(std::uint32_t command, std::uint64_t value, const char* text)
{
    if (emit_) emit_(emitUser_, command, value, text);
    // UI sound for the action (overlay show/hide plays from the backend; recording start/stop from the
    // snapshot, so F5/F6 sound the same as the buttons). Seek and timescale are continuous drags: silent.
    using S = Sound::Cue;
    switch (command)
    {
    case theater_ui::play: case theater_ui::restart: case theater_ui::replay_open: case theater_ui::toggle_playback: Cue(S::Ok); break;
    case theater_ui::pause: case theater_ui::stop: case theater_ui::replay_unload: case theater_ui::replay_delete: Cue(S::Cancel); break;
    case theater_ui::previous: case theater_ui::next: case theater_ui::page: case theater_ui::replay_page: Cue(S::PrevNext); break;
    case theater_ui::replay_sort: case theater_ui::select: case theater_ui::replay_rename: Cue(S::Bracket); break;
    default: break;
    }
}

void Overlay::Cue(Sound::Cue cue) { Sound::Play(cue); cuedThisFrame_ = true; }

// Hover and plain clicks on any control that did not already make a sound (menus, dialogs, tabs).
void Overlay::UiSoundsAfterFrame()
{
    const ImGuiID hovered = ImGui::GetHoveredID();
    const double now = ImGui::GetTime();
    if (hovered && hovered != lastHoverId_ && now - hoverSoundAt_ > 0.08) { Sound::Play(Sound::Cue::Focus); hoverSoundAt_ = now; }
    lastHoverId_ = hovered;
    if (!cuedThisFrame_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hovered) Sound::Play(Sound::Cue::Ok);
    cuedThisFrame_ = false;
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
        if (s.recording_state == theater_ui::record_recording) Sound::Play(Sound::Cue::Ok);
        else if (lastRecording_ == theater_ui::record_recording) Sound::Play(Sound::Cue::Cancel);
        static constexpr Str names[] = { Str::LogRecStopped, Str::LogRecStarted, Str::LogRecPaused, Str::LogRecSaving };
        add(s.recording_state == theater_ui::record_recording ? Tone::Live : Tone::Info, names[std::min<std::uint32_t>(s.recording_state, 3)]);
        lastRecording_ = s.recording_state;
    }
    if (!f.events.empty() && f.now - messageSoundAt_ > 1.0) { Sound::Play(Sound::Cue::Message); messageSoundAt_ = f.now; }
    for (const auto& e : f.events)
    {
        const bool error = e.find("ERROR") != std::string::npos || e.find("could not") != std::string::npos;
        eventError_ |= error;
        add(error ? Tone::Error : Tone::Accent, Str::Count, e);
    }
}

const LayoutRects& Overlay::Draw(const OverlayFrame& f, EmitFn emit, void* user)
{
    emit_ = emit; emitUser_ = user;
    const std::string replayPath = f.snapshot.loaded
        ? std::string(f.snapshot.loaded_path, strnlen(f.snapshot.loaded_path, sizeof(f.snapshot.loaded_path))) : std::string{};
    if (replayPath != lastReplayPath_)
    {
        lastReplayPath_ = replayPath;
        ui_.timeline = {};
        ui_.trackViews.clear();
        ui_.selection = {};
        ui_.gizmo = {};
        lastDuration_ = 0;
        scrubbing_ = draggingNavigator_ = false;
        lastScrubSent_ = scrubTime_ = 0;
        navigatorGrab_ = 0;
        selectedReplay_ = -1;
        pendingDialog_ = 0;
        pendingTimescale_ = 0;
        timescaleInput_[0] = 0;
    }
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
    // F5 on the host asks for a name; the first linked snapshot only sets the baseline. Works while
    // hidden too: the name box shows the overlay, and hides it again when it closes.
    if (!nameRequestSeen_) { if (f.hostLinked) { nameRequestSeen_ = true; lastNameRequest_ = f.snapshot.name_request; } }
    else if (f.snapshot.name_request != lastNameRequest_)
    {
        lastNameRequest_ = f.snapshot.name_request;
        if (!IsRecording(f.snapshot)) OpenNameDialog(f, f.visibility != UiVisibility::Shown);
    }
    if (ui_.visibility == UiVisibility::Shown)
    {
        PushFont(Font::Body);
        DrawMenuBar();
        if (showTools_) DrawRail(f);
        if (ui_.layout.panelOpen) DrawPanel(f);
        if (showTimeline_) DrawSequencer(f);
        DrawDialogs(f);
        UiSoundsAfterFrame();
        if (resetLayout_) { resetLayout_ = false; SaveSettings(); }
        if (showTools_ != savedTools_ || showTimeline_ != savedTimeline_ || ui_.layout.panelOpen != savedPanel_)
        { savedTools_ = showTools_; savedTimeline_ = showTimeline_; savedPanel_ = ui_.layout.panelOpen; SaveSettings(); }
        ImGui::PopFont();
        DrawCursor();
    }
    else DrawHiddenHint(f);
    if (ui_.visibility != UiVisibility::HiddenClean) DrawRecordingPill(f);
    return ui_.rects;
}

// Panels start at their solved default rect (below the menu bar) the first time, or every time
// Layout > Reset Layout is chosen. After Begin the window is clamped so its title bar stays
// reachable, which also repairs saved layouts after a resolution change.
bool Overlay::BeginPanel(const char* id, const char* title, ImVec2 defMin, ImVec2 defMax, ImVec2 minSize, bool* open)
{
    ImVec2 pos = defMin, size(defMax.x - defMin.x, defMax.y - defMin.y);
    if (pos.y < menuH_) { size.y -= menuH_ - pos.y; pos.y = menuH_; }
    size.x = std::max(size.x, minSize.x); size.y = std::max(size.y, minSize.y);
    const ImGuiCond cond = resetLayout_ ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
    ImGui::SetNextWindowPos(pos, cond);
    ImGui::SetNextWindowSize(size, cond);
    if (resetLayout_) { ImGui::SetNextWindowDockID(0, ImGuiCond_Always); ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always); }
    ImGui::SetNextWindowSizeConstraints(minSize, ImVec2(FLT_MAX, FLT_MAX));
    char label[128];
    snprintf(label, sizeof(label), "%s%s", title, id);
    const bool shown = ImGui::Begin(label, open, kPanel);
    if (!ImGui::IsWindowDocked())
    {
        const ImVec2 display = ImGui::GetIO().DisplaySize, p = ImGui::GetWindowPos(), sz = ImGui::GetWindowSize();
        // At least `keep` px of the title bar stay on screen (the whole width for narrow panels).
        const float keep = std::min(sz.x, Px(96, ui_.rects.uiScale)), titleH = ImGui::GetFrameHeight();
        ImVec2 c(std::clamp(p.x, keep - sz.x, std::max(keep - sz.x, display.x - keep)),
                 std::clamp(p.y, menuH_, std::max(menuH_, display.y - titleH)));
        if (c.x != p.x || c.y != p.y) ImGui::SetWindowPos(c);
    }
    return shown;
}

void Overlay::DrawMenuBar()
{
    menuH_ = 0.0f;
    if (!ImGui::BeginMainMenuBar()) return;
    menuH_ = ImGui::GetWindowHeight();
    if (ImGui::BeginMenu(T(Str::MenuLayout)))
    {
        if (ImGui::MenuItem(T(Str::ResetLayout)))
        { resetLayout_ = true; showTools_ = true; showTimeline_ = true; ui_.layout.panelOpen = true; }
        ImGui::Separator();
        ImGui::MenuItem(T(Str::PanelTools), nullptr, &showTools_);
        ImGui::MenuItem(T(Str::PanelSide), nullptr, &ui_.layout.panelOpen);
        ImGui::MenuItem(T(Str::PanelTimeline), nullptr, &showTimeline_);
        ImGui::EndMenu();
    }
    const char* hint = T(Str::HideUiHint);
    const float w = ImGui::CalcTextSize(hint).x + ImGui::GetStyle().ItemSpacing.x * 2;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - w);
    ImGui::TextDisabled("%s", hint);
    ImGui::EndMainMenuBar();
}

void Overlay::DrawRail(const OverlayFrame& f)
{
    const float s = ui_.rects.uiScale;
    const float btn = Px(Metric::RailButton, s);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Color::PanelBgSolid.Vec4());
    const bool visible = BeginPanel("###tools", T(Str::PanelTools), ui_.rects.railMin, ui_.rects.railMax,
        ImVec2(btn + Px(8, s), (btn + Px(4, s)) * 9 + Px(60, s)), &showTools_);
    if (!visible) { ImGui::End(); ImGui::PopStyleColor(); ImGui::PopStyleVar(); return; }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o(ImGui::GetWindowPos().x, ImGui::GetCursorScreenPos().y);
    const float w = ImGui::GetWindowWidth(), railBottom = ImGui::GetWindowPos().y + ImGui::GetWindowHeight();

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
            Cue(Sound::Cue::Tab);
            SaveSettings();
        }
    };

    float y = o.y + Px(52, s);
    for (const auto& it : top) { railButton(it, y, false); y += btn + Px(4, s); }

    const bool nativeWarning = eventError_;
    float yb = std::max(y + Px(8, s), railBottom - Px(12, s) - (btn + Px(4, s)) * 3);
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(Space::LG, s), Px(Space::MD, s)));
    // The title bar shows the tool name and is the grab area; the window id stays the same per tool.
    const Str titles[] = { Str::Scene, Str::Camera, Str::Look, Str::Replays, Str::Export, Str::Debug, Str::Settings };
    const bool visible = BeginPanel("###panel", T(titles[std::min<int>((int)ui_.activeTool, 6)]), ui_.rects.panelMin, ui_.rects.panelMax,
        ImVec2(Px(260, s), Px(320, s)), &ui_.layout.panelOpen);
    if (!visible) { ImGui::End(); ImGui::PopStyleVar(); return; }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 sz = ImGui::GetWindowSize();
    (void)dl;

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
        DrawLibrary(f);
        break;
    case Tool::Debug:
    {
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
        section(T(Str::UiSounds));
        {
            bool on = Sound::Enabled();
            if (ImGui::Checkbox(T(Str::UiSoundsOn), &on)) { Sound::SetEnabled(on); if (on) Sound::Play(Sound::Cue::Ok); SaveSettings(); }
            ImGui::BeginDisabled(!on);
            float volume = Sound::Volume() * 100.0f;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##volume", &volume, 0.0f, 100.0f, "%.0f%%")) Sound::SetVolume(volume / 100.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) { Sound::Play(Sound::Cue::Focus); SaveSettings(); }
            ImGui::EndDisabled();
        }
        section(T(Str::ReplayWorld));
        {
            bool flags = false; // unsupported overrides must not appear enabled from old settings
            ImGui::BeginDisabled();ImGui::Checkbox(T(Str::ReplayWorldFlags), &flags);ImGui::EndDisabled();
            ImGui::TextWrapped("World flags and time-of-day are captured read-only. Replay overrides are disabled until autosave isolation is verified.");
            // Experimental: stand-in enemies made by the game's own debug character creator, bit 1.
            bool puppets = (gReplayOptions.load() & 2) != 0;
            if (ImGui::Checkbox(T(Str::ReplayPuppets), &puppets)) { gReplayOptions = puppets ? (gReplayOptions | 2u) : (gReplayOptions & ~2u); SaveSettings(); }
            PushFont(Font::Meta);
            ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(T(Str::ReplayPuppetsNote));
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::PopFont();
            PushFont(Font::Meta);
            ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(T(Str::ReplayWorldFlagsNote));
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }
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
    auto lineText = [&](const LogLine& l) {
        return std::string(l.clock) + "  " + (l.id == Str::Count ? l.text : std::string(T(l.id)));
    };
    auto copy = [&](bool errorsOnly) {
        std::string all;
        for (const auto& l : log_)
            if (!errorsOnly || l.tone == Tone::Error || l.tone == Tone::Warning) { all += lineText(l); all += "\r\n"; }
        ImGui::SetClipboardText(all.c_str());
        copiedUntil_ = ImGui::GetTime() + 2.0;
    };
    // Copy buttons: the whole log, or only warnings and errors (for pasting into a bug report).
    if (ImGui::SmallButton(T(Str::CopyAll))) copy(false);
    ImGui::SameLine();
    if (ImGui::SmallButton(T(Str::CopyErrors))) copy(true);
    if (ImGui::GetTime() < copiedUntil_) { ImGui::SameLine(); ImGui::TextColored(Color::AccentGreen.Vec4(), "%s", T(Str::Copied)); }
    height = std::max(Px(40, s), height - ImGui::GetFrameHeightWithSpacing());

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color::ChildBg.Vec4());
    ImGui::BeginChild("##log", ImVec2(0, height), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    PushFont(Font::MonoSmall);
    if (log_.empty()) ImGui::TextDisabled("%s", T(Str::EventLogEmpty));
    ImGuiListClipper clip;
    clip.Begin((int)log_.size(), Px(Metric::LogLineHeight, s));
    while (clip.Step())
        for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i)
        {
            const auto& l = log_[(size_t)i];
            ImGui::PushID(i);
            // Each line is selectable: click copies it to the clipboard.
            const ImVec2 start = ImGui::GetCursorPos();
            if (ImGui::Selectable("##line", false, ImGuiSelectableFlags_AllowOverlap,
                                  ImVec2(std::max(ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(lineText(l).c_str()).x + Px(8, s)), 0)))
            { ImGui::SetClipboardText(lineText(l).c_str()); copiedUntil_ = ImGui::GetTime() + 2.0; }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("%s", T(Str::ClickToCopy));
            ImGui::SetCursorPos(start);
            ImGui::PushStyleColor(ImGuiCol_Text, Color::TextMuted.Vec4());
            ImGui::TextUnformatted(l.clock);
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ColorsFor(l.tone).fg.Vec4());
            ImGui::TextUnformatted(l.id == Str::Count ? l.text.c_str() : T(l.id));
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    // Stay pinned to the newest line unless the user scrolled up.
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - Px(Metric::LogLineHeight, s)) ImGui::SetScrollHereY(1.0f);
    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------
// Replay Library and dialogs
// ---------------------------------------------------------------------------
namespace
{
    void FormatDate(std::uint64_t unix, char* out, size_t n)
    {
        if (!unix) { snprintf(out, n, "-"); return; }
        const std::time_t t = (std::time_t)unix;
        std::tm local{};
        localtime_s(&local, &t);
        std::strftime(out, n, "%Y-%m-%d %H:%M", &local);
    }
    void FormatSize(std::uint64_t bytes, char* out, size_t n)
    {
        if (bytes >= 1024ull * 1024ull) snprintf(out, n, "%.1f MB", bytes / (1024.0 * 1024.0));
        else snprintf(out, n, "%.0f KB", bytes / 1024.0);
    }
    const theater_ui::Replay* FindReplay(const theater_ui::Snapshot& s, int index)
    {
        for (unsigned i = 0; i < std::min<std::uint32_t>(s.replay_count, theater_ui::replay_page_size); ++i)
            if ((int)s.replays[i].index == index) return &s.replays[i];
        return nullptr;
    }
    std::uint64_t RecordedUnix(const theater_ui::Replay& r) { return r.recorded_unix ? r.recorded_unix : r.modified_unix; }
}

void Overlay::OpenNameDialog(const OverlayFrame& f, bool restoreHidden)
{
    if (nameDialog_) return;
    nameDialog_ = true;
    nameFocus_ = true;
    restoreHidden_ = restoreHidden;
    const char* suggested = f.snapshot.default_name[0] ? f.snapshot.default_name : "Replay";
    strncpy_s(nameBuf_, suggested, _TRUNCATE);
    if (restoreHidden) Emit(kCommandSetVisibility, 0); // typing needs the overlay (and its input block)
}

void Overlay::DrawLibrary(const OverlayFrame& f)
{
    const float s = ui_.rects.uiScale;
    const auto& snap = f.snapshot;
    const bool rec = IsRecording(snap);

    // Record (with the name box first).
    char label[96]; char icon[4];
    snprintf(label, sizeof(label), "%s  %s", IconUtf8(rec ? Glyph::Stop : Glyph::Record, icon), T(rec ? Str::StopRecording : Str::Record));
    if (FlatButton("rec_panel", label, ImVec2(-1, Px(Metric::TextButtonHeight, s)), rec ? Color::TintRed : Color::FrameBg, rec ? Color::AccentRed : Color::TextPrimary, f.hostLinked))
    {
        if (rec) Emit(theater_ui::record_stop);
        else OpenNameDialog(f, false);
    }
    if (rec && snap.recording_name[0])
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Color::AccentRed.Vec4());
        ImGui::TextUnformatted(snap.recording_name);
        ImGui::PopStyleColor();
    }
    ImGui::Dummy(ImVec2(0, Px(Space::SM, s)));

    if (FlatButton("unload_replay", T(Str::UnloadReplay), ImVec2(-1, Px(Metric::TextButtonHeight, s)),
                   Color::FrameBg, Color::TextPrimary, f.hostLinked && snap.loaded && !rec))
        Emit(theater_ui::replay_unload);
    ImGui::Dummy(ImVec2(0, Px(Space::SM, s)));

    // Sortable list. Sorting happens on the host so paging stays consistent.
    const unsigned shown = std::min<std::uint32_t>(snap.replay_count, theater_ui::replay_page_size);
    if (!shown)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
        ImGui::TextWrapped("%s", T(Str::NoReplays));
        ImGui::PopStyleColor();
    }
    const float listH = std::max(Px(140, s), ImGui::GetContentRegionAvail().y * 0.55f);
    const ImGuiTableFlags flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
    if (shown && ImGui::BeginTable("##library", 5, flags, ImVec2(0, listH)))
    {
        const auto defaultSort = [&](int column) {
            const bool active = (int)snap.sort_key == column;
            return (active ? ImGuiTableColumnFlags_DefaultSort : 0) |
                   (active && !snap.sort_descending ? ImGuiTableColumnFlags_PreferSortAscending : ImGuiTableColumnFlags_PreferSortDescending);
        };
        // Columns map to the host sort keys: 0 date, 1 size, 2 name, 3 duration; area is not sortable yet.
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn(T(Str::ColName), defaultSort(2) | ImGuiTableColumnFlags_WidthFixed, Px(170, s), 2);
        ImGui::TableSetupColumn(T(Str::ColDate), defaultSort(0), Px(120, s), 0);
        ImGui::TableSetupColumn(T(Str::ColDuration), defaultSort(3), Px(70, s), 3);
        ImGui::TableSetupColumn(T(Str::ColSize), defaultSort(1), Px(70, s), 1);
        ImGui::TableSetupColumn(T(Str::ColArea), ImGuiTableColumnFlags_NoSort, Px(110, s), 4);
        ImGui::TableHeadersRow();
        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty && specs->SpecsCount > 0)
        {
            const auto& spec = specs->Specs[0];
            const std::uint32_t key = spec.ColumnUserID;
            const bool descending = spec.SortDirection == ImGuiSortDirection_Descending;
            if (key < theater_ui::sort_key_count && (key != snap.sort_key || descending != (snap.sort_descending != 0)))
                Emit(theater_ui::replay_sort, key * 2 + (descending ? 1 : 0));
            specs->SpecsDirty = false;
        }
        for (unsigned i = 0; i < shown; ++i)
        {
            const auto& e = snap.replays[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID((int)e.index);
            const bool selected = selectedReplay_ == (int)e.index;
            if (ImGui::Selectable("##row", selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
            {
                selectedReplay_ = (int)e.index;
                if (ImGui::IsMouseDoubleClicked(0)) pendingDialog_ = 1;
            }
            ImGui::SameLine(0, 0);
            if (e.loaded) ImGui::PushStyleColor(ImGuiCol_Text, Color::AccentGreen.Vec4());
            ImGui::TextUnformatted(e.name);
            if (e.loaded) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s", e.name, e.file);
            ImGui::PopID();
            char text[48];
            ImGui::TableSetColumnIndex(1); FormatDate(RecordedUnix(e), text, sizeof(text)); ImGui::TextUnformatted(text);
            ImGui::TableSetColumnIndex(2); FormatTime(e.duration_ns / 1e9, text, sizeof(text)); ImGui::TextUnformatted(text);
            ImGui::TableSetColumnIndex(3); FormatSize(e.bytes, text, sizeof(text)); ImGui::TextUnformatted(text);
            ImGui::TableSetColumnIndex(4); ImGui::TextDisabled("%s", e.area[0] ? e.area : "-");
        }
        ImGui::EndTable();
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

    // Actions for the selected replay. The loaded or recording replay can't be renamed or deleted.
    const theater_ui::Replay* sel = FindReplay(snap, selectedReplay_);
    const bool busy = rec || snap.active;
    ImGui::BeginDisabled(!sel || !f.hostLinked || busy);
    if (ImGui::Button(T(Str::Load))) pendingDialog_ = 1;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!sel || !f.hostLinked);
    if (ImGui::Button(T(Str::Rename)))
    {
        if (sel->loaded) { message_ = T(Str::LoadedBlocked); messageError_ = true; messageUntil_ = f.now + 6; }
        else { strncpy_s(renameBuf_, sel->name, _TRUNCATE); pendingDialog_ = 2; }
    }
    ImGui::SameLine();
    if (ImGui::Button(T(Str::Delete)))
    {
        if (sel->loaded) { message_ = T(Str::LoadedBlocked); messageError_ = true; messageUntil_ = f.now + 6; }
        else pendingDialog_ = 3;
    }
    ImGui::EndDisabled();

    // Result of the last host action, or a local refusal.
    if (snap.library_message_id != lastMessageId_)
    {
        lastMessageId_ = snap.library_message_id;
        if (snap.library_message[0]) { message_ = snap.library_message; messageError_ = snap.library_message_error != 0; messageUntil_ = f.now + 8; }
    }
    if (!message_.empty() && f.now < messageUntil_)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, (messageError_ ? Color::AccentRed : Color::AccentGreen).Vec4());
        ImGui::TextWrapped("%s", message_.c_str());
        ImGui::PopStyleColor();
    }
}

void Overlay::DrawDialogs(const OverlayFrame& f)
{
    const float s = ui_.rects.uiScale;
    const auto& snap = f.snapshot;

    const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.45f);
    auto centerNext = [&](float width) {
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(Px(width, s), 0), ImGuiCond_Appearing);
    };
    auto closeName = [&](bool start) {
        if (start)
        {
            std::string name(nameBuf_);
            if (name.find_first_not_of(" \t") == std::string::npos) name = snap.default_name;
            Emit(theater_ui::record_named, 0, name.c_str());
        }
        nameDialog_ = false;
        ImGui::CloseCurrentPopup();
        if (restoreHidden_) { restoreHidden_ = false; Emit(kCommandSetVisibility, 1); }
    };

    // Name this replay.
    if (nameDialog_ && !ImGui::IsPopupOpen("###name")) ImGui::OpenPopup("###name");
    centerNext(420);
    char title[96];
    snprintf(title, sizeof(title), "%s###name", T(Str::NameTitle));
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
        ImGui::TextUnformatted(T(Str::NameHint));
        ImGui::PopStyleColor();
        if (nameFocus_) { ImGui::SetKeyboardFocusHere(); nameFocus_ = false; }
        ImGui::SetNextItemWidth(Px(380, s));
        const bool enter = ImGui::InputText("##name", nameBuf_, sizeof(nameBuf_), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (enter) closeName(true);
        else if (ImGui::Button(T(Str::StartRecordingBtn), ImVec2(Px(180, s), 0))) closeName(true);
        else
        {
            ImGui::SameLine();
            if (ImGui::Button(T(Str::Cancel), ImVec2(Px(120, s), 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) closeName(false);
        }
        ImGui::EndPopup();
    }

    // Library dialogs, opened from the Replays panel (next frame, outside its window).
    const char* ids[] = { "", "###load", "###rename", "###delete" };
    if (pendingDialog_) { ImGui::OpenPopup(ids[pendingDialog_]); pendingDialog_ = 0; }
    const theater_ui::Replay* sel = FindReplay(snap, selectedReplay_);

    centerNext(460);
    snprintf(title, sizeof(title), "%s###load", T(Str::LoadTitle));
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (!sel) ImGui::CloseCurrentPopup();
        else
        {
            char date[48], dur[32], size[32];
            FormatDate(RecordedUnix(*sel), date, sizeof(date)); FormatTime(sel->duration_ns / 1e9, dur, sizeof(dur)); FormatSize(sel->bytes, size, sizeof(size));
            PushFont(Font::BodyStrong); ImGui::TextUnformatted(sel->name); ImGui::PopFont();
            auto row = [&](Str label, const char* value) {
                ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4()); ImGui::TextUnformatted(T(label)); ImGui::PopStyleColor();
                ImGui::SameLine(Px(150, s)); ImGui::TextUnformatted(value);
            };
            row(Str::ColDate, date); row(Str::ColDuration, dur); row(Str::ColSize, size);
            row(Str::ColArea, sel->area[0] ? sel->area : T(Str::AreaUnknown));
            row(Str::GameVersion, sel->game_version[0] ? sel->game_version : "-");
            row(Str::FileLabel, sel->file);
            ImGui::Dummy(ImVec2(0, Px(4, s)));
            ImGui::PushStyleColor(ImGuiCol_Text, Color::AccentAmber.Vec4());
            ImGui::PushTextWrapPos(Px(440, s));
            ImGui::TextUnformatted(T(Str::LoadTeleportNote));
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            if (ImGui::Button(T(Str::Load), ImVec2(Px(140, s), 0))) { Emit(theater_ui::replay_open, (std::uint64_t)sel->index); ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (ImGui::Button(T(Str::Cancel), ImVec2(Px(120, s), 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    centerNext(420);
    snprintf(title, sizeof(title), "%s###rename", T(Str::RenameTitle));
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (!sel) ImGui::CloseCurrentPopup();
        else
        {
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(Px(380, s));
            const bool enter = ImGui::InputText("##rename", renameBuf_, sizeof(renameBuf_), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            const bool empty = std::string(renameBuf_).find_first_not_of(" \t") == std::string::npos;
            if (empty) { ImGui::PushStyleColor(ImGuiCol_Text, Color::AccentRed.Vec4()); ImGui::TextUnformatted(T(Str::NameEmpty)); ImGui::PopStyleColor(); }
            ImGui::BeginDisabled(empty);
            if ((enter && !empty) || ImGui::Button(T(Str::Save), ImVec2(Px(140, s), 0)))
            { Emit(theater_ui::replay_rename, (std::uint64_t)sel->index, renameBuf_); ImGui::CloseCurrentPopup(); }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button(T(Str::Cancel), ImVec2(Px(120, s), 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    centerNext(420);
    snprintf(title, sizeof(title), "%s###delete", T(Str::DeleteTitle));
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (!sel) ImGui::CloseCurrentPopup();
        else
        {
            char body[256];
            snprintf(body, sizeof(body), T(Str::DeleteBody), sel->name);
            ImGui::PushTextWrapPos(Px(400, s));
            ImGui::TextUnformatted(body);
            ImGui::PopTextWrapPos();
            ImGui::PushStyleColor(ImGuiCol_Button, Color::TintRed.Vec4());
            ImGui::PushStyleColor(ImGuiCol_Text, Color::AccentRed.Vec4());
            if (ImGui::Button(T(Str::Delete), ImVec2(Px(140, s), 0)))
            { Emit(theater_ui::replay_delete, (std::uint64_t)sel->index); selectedReplay_ = -1; ImGui::CloseCurrentPopup(); }
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            if (ImGui::Button(T(Str::Cancel), ImVec2(Px(120, s), 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Overlay::DrawSequencer(const OverlayFrame& f)
{
    const float s = ui_.rects.uiScale;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Color::TimelineBg.Alpha(255).Vec4());
    const float toolbar = Px(Layout.SequencerToolbar, s);
    const bool visible = BeginPanel("###timeline", T(Str::PanelTimeline), ui_.rects.sequencerMin, ui_.rects.sequencerMax,
        ImVec2(Px(820, s), toolbar + Px(Layout.RulerHeight + Layout.NavigatorHeight + Metric::TrackRowHeight * 2, s) + ImGui::GetFrameHeight()), &showTimeline_);
    if (!visible) { ImGui::End(); ImGui::PopStyleColor(); ImGui::PopStyleVar(); return; }
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight());
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
    const ImVec2 o(ImGui::GetWindowPos().x, ImGui::GetCursorScreenPos().y); // below the title bar
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

    // One authoritative toggle shared with the configured Play/Pause hotkey.
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
    if (iconButton("play", playing ? Glyph::Pause : Glyph::Play, playing ? "Pause / Play-Pause hotkey" : T(Str::Play), canTransport, Color::AccentBlue, Color::TextOnAccent, Px(36, s))) Emit(theater_ui::toggle_playback);
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

    // One continuous timescale; the host owns the value and clock. UI time is unscaled.
    double value = snap.timescale;
    if (pendingTimescale_ == value || f.now - timescaleSentAt_ > 1.0) pendingTimescale_ = 0;
    if (pendingTimescale_ > 0) value = pendingTimescale_;
    const float controlY = o.y + (height - ImGui::GetFrameHeight()) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(x, controlY));
    ImGui::BeginDisabled(!canTransport);
    ImGui::TextUnformatted(T(Str::Speed));
    ImGui::SameLine(0, Px(6,s));
    const float sliderWidth = std::clamp(w - (x-o.x) - Px(400,s), Px(45,s), Px(160,s));
    const ImVec2 sliderMin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##timescale_slider", ImVec2(sliderWidth, ImGui::GetFrameHeight()));
    const bool sliderHovered = ImGui::IsItemHovered(), sliderActive = ImGui::IsItemActive();
    const double previousValue = value;
    bool changed = false;
    auto& io = ImGui::GetIO();
    const double precision = io.KeyCtrl ? 0.002 : io.KeyShift ? 0.02 : 1.0;
    if (canTransport && sliderActive)
    {
        if (ImGui::IsItemActivated() && !io.KeyShift && !io.KeyCtrl)
            value = theater_timescale::from_normalized((io.MousePos.x-sliderMin.x)/sliderWidth);
        else if (io.MouseDelta.x != 0)
            value = theater_timescale::adjust(value, io.MouseDelta.x/sliderWidth * precision);
        // Quick-set marks are magnetic within a few pixels (hold Shift or Ctrl to place freely).
        if (!io.KeyShift && !io.KeyCtrl)
            for (double m : theater_timescale::marks)
                if (std::abs((theater_timescale::normalized(value) - theater_timescale::normalized(m)) * sliderWidth) < Px(4, s)) value = m;
        changed = true;
    }
    // Double-click the slider: back to 1x.
    if (canTransport && sliderHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { value = 1.0; changed = true; timescaleInputInvalid_ = false; }
    const float cy = sliderMin.y + ImGui::GetFrameHeight()*0.5f;
    dl->AddLine(ImVec2(sliderMin.x,cy), ImVec2(sliderMin.x+sliderWidth,cy), Color::TextMuted.U32(), Px(3,s));
    for (double m : theater_timescale::marks)
    {
        const float mx = sliderMin.x + sliderWidth * (float)theater_timescale::normalized(m);
        const bool normal = m == theater_timescale::normal;
        dl->AddLine(ImVec2(mx, cy - Px(normal ? 6 : 4, s)), ImVec2(mx, cy + Px(normal ? 6 : 4, s)), (normal ? Color::AccentAmber : Color::TextMuted).U32(), Px(1, s));
    }
    const float knobX=sliderMin.x+sliderWidth*(float)theater_timescale::normalized(value);
    dl->AddCircleFilled(ImVec2(knobX,cy),Px(5,s),Color::AccentBlue.U32());
    if (sliderHovered) ImGui::SetTooltip("Replay speed 0.01x to 4x. Marks: 0.1, 0.25, 0.5, 1 (amber), 2 (they snap; Shift/Ctrl for fine control).\nDouble-click or right-click: back to 1x. Mouse wheel adjusts. The game itself keeps full speed.");
    ImGui::SameLine(0,Px(6,s));
    if (timescaleInput_[0]==0) theater_timescale::format(value,timescaleInput_,sizeof(timescaleInput_));
    ImGui::SetNextItemWidth(Px(98,s));
    if (ImGui::InputText("##timescale_exact",timescaleInput_,sizeof(timescaleInput_),ImGuiInputTextFlags_EnterReturnsTrue))
    {
        double parsed=0;
        timescaleInputInvalid_=!theater_timescale::parse(timescaleInput_,parsed);
        if (!timescaleInputInvalid_) {value=parsed;changed=true;}
    }
    const bool inputHovered=ImGui::IsItemHovered(), inputActive=ImGui::IsItemActive();
    if (timescaleInputInvalid_ && inputHovered) ImGui::SetTooltip("Enter a finite positive value, optionally ending in x. Range: 0.01x to 4x.");
    if (canTransport && (sliderHovered||inputHovered))
    {
        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
        if (io.MouseWheel!=0) {value=theater_timescale::adjust(value,io.MouseWheel*0.025*precision);changed=true;io.MouseWheel=0;}
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)||ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {value=1.0;changed=true;timescaleInputInvalid_=false;}
    }
    if (changed && value != previousValue)
    {
        pendingTimescale_=value;timescaleSentAt_=f.now;
        Emit(theater_ui::timescale,theater_timescale::encode(value));
        theater_timescale::format(value,timescaleInput_,sizeof(timescaleInput_));
    }
    else if (!inputActive) theater_timescale::format(value,timescaleInput_,sizeof(timescaleInput_));
    ImGui::EndDisabled();

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
    {
        if (rec) Emit(theater_ui::record_stop);
        else OpenNameDialog(f, false);
    }
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
    // Permanent while hidden (Shift+F4 clean mode hides it too), bottom-left, so the key is always known.
    if (ui_.visibility != UiVisibility::Hidden) return;
    const double age = f.now - f.hiddenAt;
    const float alpha = (age >= 0 && age < 2.0) ? 1.0f : 0.75f;
    const float s = ui_.rects.uiScale;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const char* text = T(Str::ShowUiHint);
    ImFont* font = ui_.fonts[Font::Body];
    const float size = Px(FontSize::Body, s);
    const ImVec2 tsz = font->CalcTextSizeA(size, FLT_MAX, 0, text);
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float h = Px(Metric::ToastHeight, s);
    const ImVec2 p0(Px(24, s), display.y - Px(24, s) - h);
    const ImVec2 p1(p0.x + tsz.x + Px(28, s), p0.y + h);
    dl->AddRectFilled(p0, p1, Color::OverlayBg.Fade(alpha).U32(), Px(Radius::Toast, s));
    dl->AddText(font, size, ImVec2(p0.x + Px(14, s), p0.y + (p1.y - p0.y - tsz.y) * 0.5f), Color::TextPrimary.Fade(alpha).U32(), text);
}
}

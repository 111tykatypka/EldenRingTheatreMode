#include "../NativeLightBackend.h"
#include "TheaterOverlayUI.h"
#include "TheaterSounds.h"
#include "imgui_internal.h"
#include "../CinematicCameraRuntime.h"
#include "../EldenRingTimingAdapter.h"
#include "../EldenRingWeatherAdapter.h"
#include "../WindController.h"
#include "../EldenRingLightAdapter.h"
#include "../LightEditor.h"
#include "../VideoExport.h"
#include "../ParticleEditor.h"
#include "../ParticleCatalog.h"
#include "../ColorGrading.h"
#include "../NativeParticleBackend.h"
#include "../EldenRingHudAdapter.h"
#include "CameraViewport.h"

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
#include <map>

namespace TheaterUI
{
namespace
{
    // Index of the bone whose name equals `key` (ignoring case), else the first one containing it; -1 if none.
    int FindBone(const std::vector<std::string>& names, const char* key){
        auto lower=[](std::string s){for(auto& c:s)c=(char)std::tolower((unsigned char)c);return s;};
        const std::string k=lower(key);
        for(int i=0;i<(int)names.size();++i)if(lower(names[i])==k)return i;
        for(int i=0;i<(int)names.size();++i)if(lower(names[i]).find(k)!=std::string::npos)return i;
        return -1;
    }
    // The bone camera needs a bone; when none was picked yet use the head.
    bool AutoSelectBone(){
        const auto names=camera_runtime::bone_names();const int head=FindBone(names,"Head");
        if(head<0)return false;camera_runtime::bone_attach(head);return true;
    }
}
void Overlay::CameraHotkey(theater_hotkeys::Action action)
{
    if(action==theater_hotkeys::Action::PlayDollyPath){playDollyRequested_=true;return;}
    if(action==theater_hotkeys::Action::ToggleDollyControls){if(enableDollyVisibilityKey_){showDollyMarkers_=!showDollyMarkers_;gizmoDragging_=false;SaveSettings();}return;}
    ui_.activeTool=Tool::Camera;ui_.layout.panelOpen=true;
    if(action==theater_hotkeys::Action::CycleCamera){
        const auto cam=camera_runtime::view(false);cameraSelection_=cam.mode>=3?0:cam.mode+1;
        if(cameraSelection_==3&&cam.bone_index<0&&!AutoSelectBone())cameraSelection_=0; // no bone list yet: skip the bone camera
        camera_runtime::mode(cameraSelection_);
        camera_runtime::enable(cameraSelection_!=0);
    } else if(action==theater_hotkeys::Action::AddDollyKey){
        camera_runtime::add_key();
    } else if(action==theater_hotkeys::Action::ClearDollyKeys) clearDollyDialog_=true;
}
namespace
{
    using namespace Theme;

    // Movable, resizable panels with a title bar (the grab area). Scrolling is handled inside.
    constexpr ImGuiWindowFlags kPanel = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    constexpr ImGuiWindowFlags kRegion = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    std::string KeyName(theater_hotkeys::Action action){
        const auto vk=theater_hotkeys::Key(action);wchar_t wide[80]{};char text[256]{};
        LONG scan=MapVirtualKeyW(vk,MAPVK_VK_TO_VSC)<<16;if(vk>=VK_PRIOR&&vk<=VK_DOWN)scan|=1<<24;
        GetKeyNameTextW(scan,wide,80);WideCharToMultiByte(CP_UTF8,0,wide,-1,text,sizeof(text),nullptr,nullptr);
        return text[0]?text:std::to_string(vk);
    }
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
        wchar_t isolated[4]{};if(GetEnvironmentVariableW(L"THEATER_OVERLAY_NO_SETTINGS_FILE",isolated,4)>0)return {};
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
    auto effects=camera_runtime::view(false);
    auto grade=color_grading::settings();
    auto wind=game_wind::view();
    std::string key; float value = 0;
    while (in >> key >> value)
    {
        if (!std::isfinite(value)) continue;
        if (key == "camera_info") showCameraInfo_=value!=0;
        else if (key == "bone_dots") showBoneDots_=value!=0;
        else if (key == "bone_dots_major") boneDotsMajorOnly_=value!=0;
        else if (key == "camera_modes_x") cameraModesX_=value;
        else if (key == "camera_modes_y") cameraModesY_=value;
        else if (key == "camera_modes_scale") cameraModesScale_=std::clamp(value,.5f,3.f);
        else if (key == "dolly_markers") showDollyMarkers_=value!=0;
        else if(key=="light_markers") showLightMarkers_=value!=0;
        else if(key=="dolly_visibility_key") enableDollyVisibilityKey_=value!=0;
        else if(key=="curve_fraction") curveFraction_=std::clamp(value,.15f,.85f);
        else if(key=="game_view_fit") gameViewFit_=value!=0;
        else if(key=="compact_tracks") compactTracks_=value!=0;
        else if(key=="actor_tracks") expandActorTracks_=value!=0;
        else if(key=="dolly_curves") showDollyCurves_=value!=0;
        else if(key=="dolly_smoothing") effects.dolly_smoothing_seconds=std::clamp(double(value),0.,2.);
        else if(key=="shake_position") effects.shake_position=std::clamp(double(value),0.,5.);
        else if(key=="shake_rotation") effects.shake_rotation=std::clamp(double(value),0.,30.);
        else if(key=="shake_frequency") effects.shake_frequency=std::clamp(double(value),0.,30.);
        else if(key=="shake_speed") effects.shake_speed=std::clamp(double(value),0.,10.);
        else if(key=="shake_smoothing") effects.shake_smoothing_seconds=std::clamp(double(value),0.,2.);
        else if(key=="shake_dolly") effects.shake_dolly=value!=0;
        else if(key=="look_exposure")grade.exposure_ev=std::clamp(value,-5.f,5.f);
        else if(key=="look_contrast")grade.contrast=std::clamp(value,0.f,2.f);
        else if(key=="look_saturation")grade.saturation=std::clamp(value,0.f,2.f);
        else if(key=="look_vibrance")grade.vibrance=value;
        else if(key=="look_grain")grade.grain=value;
        else if(key=="look_grain_size")grade.grain_size=value;
        else if(key=="look_grain_speed")grade.grain_speed=value;
        else if(key=="look_sharpen")grade.sharpen=value;
        else if(key=="look_vignette")grade.vignette=value;
        else if(key=="look_vignette_radius")grade.vignette_radius=value;
        else if(key=="look_vignette_softness")grade.vignette_softness=value;
        else if(key=="look_aberration")grade.aberration=value;
        else if(key=="look_distortion")grade.distortion=value;
        else if(key=="look_lut_blend")grade.lut_blend=value;
        else if(key=="wind_strength")wind.strength=std::clamp(value,0.f,3.f);
        else if(key=="high_quality_lods")effects.high_quality_lods=value!=0;
        else if(key=="prevent_asset_fade")effects.prevent_asset_fade=value!=0;
        else if(key=="camera_near_plane")effects.near_plane=std::clamp(double(value),.001,1.);
        else if (key == "language") language = value >= 1 ? Lang::Russian : Lang::English;
        else if (key == "ui_scale") ui_.layout.uiScaleUser = std::clamp(value, 0.75f, 1.5f);
        else if (key == "panel_open") ui_.layout.panelOpen = value != 0;
        else if (key == "particle_markers") showParticleMarkers_=value!=0;
        else if (key == "tool" && value >= 0 && value <= (float)Tool::Bones) ui_.activeTool = (Tool)(int)value;
        else if (key == "show_tools") showTools_ = value != 0;
        else if (key == "show_timeline") showTimeline_ = value != 0;
        else if (key == "show_event_log") showEventLog_ = value != 0;
        else if (key == "sound_enabled") Sound::SetEnabled(value != 0);
        else if (key == "sound_volume") Sound::SetVolume(std::clamp(value, 0.0f, 1.0f));
        else if (key == "replay_options") gReplayOptions = (std::uint32_t)value;
        else if (key.rfind("gfx",0)==0&&key.size()>3&&std::isdigit((unsigned char)key[3])){const int slot=std::atoi(key.c_str()+3);if(slot>=0&&slot<16)gGfx[slot]=value<=-2147483000.f?kGfxDefault:(int)value;}
    }
    // Persist only values; enabled defaults off and survives renderer resize.
    color_grading::configure(grade);
    game_wind::configure(false,wind.strength);
    camera_runtime::dolly_smoothing(effects.dolly_smoothing_seconds);
    camera_runtime::close_up(effects.prevent_asset_fade,effects.near_plane);
    camera_runtime::high_quality_lods(effects.high_quality_lods);
    camera_runtime::shake(effects.shake_position,effects.shake_rotation,effects.shake_frequency,effects.shake_speed,effects.shake_smoothing_seconds,effects.shake_dolly);
}

void Overlay::SaveSettings() const
{
    const auto path = SettingsPath();
    if (path.empty()) return;
    std::error_code ec; std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::trunc);
    const auto effects=camera_runtime::view(false);
    const auto grade=color_grading::settings();
    out << "wind_strength " << game_wind::view().strength << "\n";
    out << "look_exposure " << grade.exposure_ev << "\n" << "look_contrast " << grade.contrast << "\n" << "look_saturation " << grade.saturation << "\n";
    out << "look_vibrance " << grade.vibrance << "\nlook_grain " << grade.grain << "\nlook_grain_size " << grade.grain_size << "\nlook_grain_speed " << grade.grain_speed << "\nlook_sharpen " << grade.sharpen
        << "\nlook_vignette " << grade.vignette << "\nlook_vignette_radius " << grade.vignette_radius << "\nlook_vignette_softness " << grade.vignette_softness
        << "\nlook_aberration " << grade.aberration << "\nlook_distortion " << grade.distortion << "\nlook_lut_blend " << grade.lut_blend << "\n";
    out << "curve_fraction " << curveFraction_ << "\n";
    out << "game_view_fit " << gameViewFit_ << "\n";
    out << "compact_tracks " << compactTracks_ << "\n" << "actor_tracks " << expandActorTracks_ << "\n";
    out << "high_quality_lods " << effects.high_quality_lods << "\n";
    out << "light_markers " << showLightMarkers_ << "\n";
    out << "particle_markers " << showParticleMarkers_ << "\n";
    out << "camera_info " << showCameraInfo_ << "\n";
    out << "bone_dots " << showBoneDots_ << "\nbone_dots_major " << boneDotsMajorOnly_ << "\n";
    out << "camera_modes_x " << cameraModesX_ << "\ncamera_modes_y " << cameraModesY_ << "\ncamera_modes_scale " << cameraModesScale_ << "\n";
    out << "dolly_markers " << showDollyMarkers_ << "\n"
        << "dolly_visibility_key " << enableDollyVisibilityKey_ << "\n"
        << "dolly_curves " << showDollyCurves_ << "\n"
        << "dolly_smoothing " << effects.dolly_smoothing_seconds << "\n"
        << "shake_position " << effects.shake_position << "\n"
        << "shake_rotation " << effects.shake_rotation << "\n"
        << "shake_frequency " << effects.shake_frequency << "\n"
        << "shake_speed " << effects.shake_speed << "\n"
        << "shake_smoothing " << effects.shake_smoothing_seconds << "\n"
        << "shake_dolly " << effects.shake_dolly << "\n";
    out << "prevent_asset_fade " << effects.prevent_asset_fade << "\n" << "camera_near_plane " << effects.near_plane << "\n";
    out << "language " << (language == Lang::Russian ? 1 : 0) << "\n"
        << "ui_scale " << ui_.layout.uiScaleUser << "\n"
        << "panel_open " << (ui_.layout.panelOpen ? 1 : 0) << "\n"
        << "tool " << (int)ui_.activeTool << "\n"
        << "show_tools " << (showTools_ ? 1 : 0) << "\n"
        << "show_timeline " << (showTimeline_ ? 1 : 0) << "\n"
        << "show_event_log " << (showEventLog_ ? 1 : 0) << "\n"
        << "sound_enabled " << (Sound::Enabled() ? 1 : 0) << "\n"
        << "sound_volume " << Sound::Volume() << "\n"
        << "replay_options " << gReplayOptions.load() << "\n";
    for(int i=0;i<16;++i)out << "gfx" << i << " " << gGfx[i].load() << "\n";
}

void Overlay::Emit(std::uint32_t command, std::uint64_t value, const char* text)
{
    if(!inputFocused_)return;
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
    if (f.visibility != UiVisibility::Shown || !ui_.layout.panelOpen || ui_.activeTool != Tool::Settings) {
        bindingWaiting_ = -1;
        theater_hotkeys::rebinding = false;
    }
    const std::string replayPath = f.snapshot.loaded
        ? std::string(f.snapshot.loaded_path, strnlen(f.snapshot.loaded_path, sizeof(f.snapshot.loaded_path))) : std::string{};
    if (replayPath != lastReplayPath_)
    {
        selectedDollyKeys_.clear();selectedDollyKey_=dollySelectionAnchor_=0;
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
    inputFocused_=f.focused;
    const auto selectionTrack=camera_runtime::view();
    if(cameraHistoryGeneration_!=selectionTrack.history_generation){cameraHistoryGeneration_=selectionTrack.history_generation;gizmoDragging_=curveDragging_=curveBoxSelecting_=false;}
    std::erase_if(selectedDollyKeys_,[&](auto id){return !selectionTrack.track_current||std::none_of(selectionTrack.keys.begin(),selectionTrack.keys.end(),[&](const auto&key){return key.id==id;});});
    if(!selectedDollyKeys_.contains(selectedDollyKey_))selectedDollyKey_=selectedDollyKeys_.empty()?0:*selectedDollyKeys_.begin();
    if(!inputFocused_){lightGizmoDragging_=false;gizmoDragging_=false;curveDragging_=false;curveBoxSelecting_=false;scrubbing_=false;draggingNavigator_=false;camera_runtime::end_edit();}
    if(playDollyRequested_){playDollyRequested_=false;if(f.focused)PlayDollyPath(f);}
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
        sequencerTop_=io.DisplaySize.y;sequencerRight_=io.DisplaySize.x;
        if (showTimeline_) DrawSequencer(f);
        DrawGameViewport(f);
        DrawDollyViewport(f);
        DrawCameraModes(f);
        DrawCameraInfo(f);
        if(auto t=camera_runtime::replay_time())light_editor::evaluate_animation(*t,lightGizmoDragging_?light_editor::view().selected:0);
        if(f.focused&&!io.WantTextInput&&bindingWaiting_<0&&!ImGui::IsAnyItemActive()&&ImGui::IsKeyPressed(ImGuiKey_Delete,false)){
            if(viewportLightSelected_){light_editor::remove(light_editor::view().selected);lightGizmoDragging_=false;}
            else DeleteSelectedDollyKeys();
        }
        DrawDialogs(f);
        UiSoundsAfterFrame();
        if (resetLayout_) { resetLayout_ = false; SaveSettings(); }
        if (showTools_ != savedTools_ || showTimeline_ != savedTimeline_ || ui_.layout.panelOpen != savedPanel_)
        { savedTools_ = showTools_; savedTimeline_ = showTimeline_; savedPanel_ = ui_.layout.panelOpen; SaveSettings(); }
        ImGui::PopFont();
        DrawCursor();
    }
    else {gameViewInitialized_=false;DrawHiddenHint(f);PushFont(Font::Body);DrawDollyViewport(f);DrawCameraModes(f);DrawCameraInfo(f);if(auto t=camera_runtime::replay_time())light_editor::evaluate_animation(*t,lightGizmoDragging_?light_editor::view().selected:0);ImGui::PopFont();}
    if(cameraSettingsDirty_&&!ImGui::IsMouseDown(ImGuiMouseButton_Left)&&f.now-cameraSettingsChangedAt_>.4){SaveSettings();cameraSettingsDirty_=false;}
    if(!gizmoDragging_&&!curveDragging_)camera_runtime::end_edit();
    if (ui_.visibility != UiVisibility::HiddenClean) DrawRecordingPill(f);
    DrawExportBanner(f); // always visible while exporting, in every visibility mode; never part of the exported picture
    return ui_.rects;
}

void Overlay::SelectDollyKey(std::uint64_t id,bool toggle,bool range)
{
    viewportLightSelected_=false;lightGizmoDragging_=false;
    auto camera=camera_runtime::view();
    auto hit=std::find_if(camera.keys.begin(),camera.keys.end(),[&](const auto&key){return key.id==id;});
    if(hit==camera.keys.end())return;
    auto anchor=std::find_if(camera.keys.begin(),camera.keys.end(),[&](const auto&key){return key.id==dollySelectionAnchor_;});
    if(range&&anchor!=camera.keys.end()){
        if(!toggle)selectedDollyKeys_.clear();
        const auto lo=std::min(anchor->time_ns,hit->time_ns),hi=std::max(anchor->time_ns,hit->time_ns);
        for(const auto&key:camera.keys)if(key.time_ns>=lo&&key.time_ns<=hi)selectedDollyKeys_.insert(key.id);
    }else{
        if(!toggle)selectedDollyKeys_.clear();
        if(toggle&&selectedDollyKeys_.contains(id))selectedDollyKeys_.erase(id);else selectedDollyKeys_.insert(id);
        dollySelectionAnchor_=id;
    }
    selectedDollyKey_=selectedDollyKeys_.contains(id)?id:(selectedDollyKeys_.empty()?0:*selectedDollyKeys_.begin());
    gizmoDragging_=curveDragging_=false;
}
void Overlay::DeleteSelectedDollyKeys()
{
    if(!inputFocused_||selectedDollyKeys_.empty())return;
    camera_runtime::delete_keys(std::vector<std::uint64_t>(selectedDollyKeys_.begin(),selectedDollyKeys_.end()));
    selectedDollyKeys_.clear();selectedDollyKey_=dollySelectionAnchor_=0;gizmoDragging_=curveDragging_=false;
}
void Overlay::PlayDollyPath(const OverlayFrame& f)
{
    auto camera=camera_runtime::view();
    if(!f.focused||!f.snapshot.loaded||!camera.track_current||camera.keys.size()<2)return;
    camera_runtime::mode(2);camera_runtime::enable(true);
    if(!camera_runtime::view(false).enabled)return;
    camera_runtime::preview(true);
    // Start inside the authored range, rather than at a clamped endpoint.
    Emit(theater_ui::seek,camera.keys.front().time_ns);Emit(theater_ui::play);
}

// Export status at the top of the screen. Drawn after the export copy was taken, so it is never in the exported video or images.
void Overlay::DrawExportBanner(const OverlayFrame& f)
{
    if(!video_export::banner_visible())return;
    const auto st=video_export::status();if(st.text.empty())return;
    auto&io=ImGui::GetIO();const float s=std::max(.6f,ui_.rects.uiScale);
    const bool shown=ui_.visibility==UiVisibility::Shown;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*.5f,(shown?menuH_:0.f)+Px(6,s)),ImGuiCond_Always,ImVec2(.5f,0));
    ImGui::SetNextWindowBgAlpha(.72f);
    const ImGuiWindowFlags flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoMove;
    if(ImGui::Begin("##export-banner",nullptr,flags)){
        const bool failed=!st.active&&!st.error.empty();
        const ImU32 dot=st.active?(std::fmod(f.now,1.0)<.6?IM_COL32(235,50,50,255):IM_COL32(120,25,25,255)):failed?IM_COL32(255,170,60,255):IM_COL32(90,200,110,255);
        const ImVec2 p=ImGui::GetCursorScreenPos();const float r=Px(6,s);
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x+r,p.y+ImGui::GetTextLineHeight()*.5f),r,dot);
        ImGui::Dummy(ImVec2(r*2+Px(6,s),0));ImGui::SameLine();
        ImGui::TextUnformatted(st.text.c_str());
        if(st.active)ImGui::TextDisabled("%s to stop", KeyName(theater_hotkeys::Action::ToggleExport).c_str());
    }
    ImGui::End();
}

// Small camera readout in the lower left corner of the picture: mode, FOV, roll, speed and the keys that matter right now.
void Overlay::DrawCameraInfo(const OverlayFrame& f)
{
    if(!showCameraInfo_||ui_.visibility!=UiVisibility::Shown)return; // only while the F4 menu is open
    const auto camera=camera_runtime::view(false);
    if(camera.mode==0||!camera.enabled||!camera.observed)return;
    const bool shown=ui_.visibility==UiVisibility::Shown;
    const auto pictureMin=shown?ui_.rects.gameMin:ImVec2(0,0),pictureMax=shown?ui_.rects.gameMax:ImGui::GetIO().DisplaySize;
    if(pictureMax.x<=pictureMin.x||pictureMax.y<=pictureMin.y)return;
    const float s=std::min(ui_.rects.uiScale,(pictureMax.x-pictureMin.x)/420.f);
    static const char* modeNames[]={"Default","Free","Dolly","Bone"};
    const double roll=cinematic::split_roll(camera.pose.orientation).second;
    using theater_hotkeys::Action;
    auto K=[&](Action a){return KeyName(a);};
    char line1[160],line2[256],line3[256];
    snprintf(line1,sizeof(line1),"%s camera   FOV %.1f   Roll %.1f   Speed %.2g",modeNames[std::min<unsigned>(camera.mode,3)],camera.pose.fov_degrees,roll,camera.movement_speed);
    snprintf(line2,sizeof(line2),"Move %s%s%s%s  Up/Down %s/%s   Hold %s: x5 faster   Hold %s: slow   %s/%s: speed",K(Action::Forward).c_str(),K(Action::Left).c_str(),K(Action::Backward).c_str(),K(Action::Right).c_str(),K(Action::Up).c_str(),K(Action::Down).c_str(),K(Action::Fast).c_str(),K(Action::Slow).c_str(),K(Action::SpeedUp).c_str(),K(Action::SpeedDown).c_str());
    snprintf(line3,sizeof(line3),"Look: mouse or %s%s%s%s   Roll %s/%s (reset %s)   FOV %s/%s or wheel (reset %s)",K(Action::PitchUp).c_str(),K(Action::YawLeft).c_str(),K(Action::PitchDown).c_str(),K(Action::YawRight).c_str(),K(Action::RollLeft).c_str(),K(Action::RollRight).c_str(),K(Action::ResetRoll).c_str(),K(Action::FovUp).c_str(),K(Action::FovDown).c_str(),K(Action::ResetFov).c_str());
    ImGui::SetNextWindowPos(ImVec2(pictureMin.x+Px(10,s),pictureMax.y-Px(10,s)),ImGuiCond_Always,ImVec2(0,1));
    ImGui::SetNextWindowBgAlpha(.45f);
    const ImGuiWindowFlags flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoMove;
    if(ImGui::Begin("##camera-info",nullptr,flags)){
        ImGui::SetWindowFontScale(.85f*std::max(.6f,s/std::max(.01f,ui_.rects.uiScale)));
        ImGui::TextUnformatted(line1);
        ImGui::PushStyleColor(ImGuiCol_Text,Color::TextSecondary.Vec4());
        ImGui::TextUnformatted(line2);ImGui::TextUnformatted(line3);
        if(camera.mode==2)ImGui::Text("Dolly: %s add key, %s play path",K(Action::AddDollyKey).c_str(),K(Action::PlayDollyPath).c_str());
        if(camera.mode==3)ImGui::TextUnformatted("Bone camera: move and turn relative to the chosen bone");
        ImGui::TextUnformatted(("Cycle camera: "+K(Action::CycleCamera)).c_str());
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

// Compact mode selector remains a read-only badge while the main overlay is hidden.
// Geometry is drawn independently; no third-party icon assets are copied.
void Overlay::DrawCameraModes(const OverlayFrame& f)
{
    if(ui_.visibility==UiVisibility::HiddenClean)return;
    const bool shown=ui_.visibility==UiVisibility::Shown;
    camera_runtime::set_bone_dots(showBoneDots_&&ui_.activeTool==Tool::Bones&&shown);
    const auto pictureMin=shown?ui_.rects.gameMin:ImVec2(0,0),pictureMax=shown?ui_.rects.gameMax:ImGui::GetIO().DisplaySize;
    if(pictureMax.x<=pictureMin.x||pictureMax.y<=pictureMin.y)return;
    const float s=std::min(ui_.rects.uiScale,(pictureMax.x-pictureMin.x)/340.f)*cameraModesScale_;auto camera=camera_runtime::view(false);
    // Movable and scalable: drag it by its grip strip or any empty part, Ctrl + mouse wheel (or right-click) changes its size.
    if(cameraModesX_>=0&&!cameraModesDragging_)ImGui::SetNextWindowPos(ImVec2(cameraModesX_,cameraModesY_),ImGuiCond_Always);
    else if(cameraModesX_<0)ImGui::SetNextWindowPos(ImVec2((pictureMin.x+pictureMax.x)*.5f,pictureMin.y+Px(8,s)),ImGuiCond_Always,ImVec2(.5f,0));
    ImGui::SetNextWindowSizeConstraints(ImVec2(0,0),ImVec2(pictureMax.x-pictureMin.x,std::max(1.f,pictureMax.y-pictureMin.y-Px(8,s))));
    ImGuiWindowFlags flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoNavFocus;
    if(!shown)flags|=ImGuiWindowFlags_NoInputs;
    if(cameraModesGrip_||cameraModesResizing_)flags|=ImGuiWindowFlags_NoMove; // the corner grip resizes, it must not drag the window
    ImGui::PushStyleColor(ImGuiCol_WindowBg,Color::OverlayBg.Alpha(235).Vec4());
    const bool visible=ImGui::Begin("##camera-modes",nullptr,flags);
    if(visible){
        if(shown){
            // Grip strip: empty space the window can be dragged by.
            const ImVec2 g=ImGui::GetCursorScreenPos();const float gw=Px(72,s)*4+ImGui::GetStyle().ItemSpacing.x*3;
            ImGui::Dummy(ImVec2(gw,Px(9,s)));
            auto*gd=ImGui::GetWindowDrawList();for(int d=-3;d<=3;++d)gd->AddCircleFilled(ImVec2(g.x+gw*.5f+d*Px(7,s),g.y+Px(4,s)),Px(1.6f,s),Color::TextSecondary.U32());
            if(ImGui::IsItemHovered())ImGui::SetTooltip("Drag to move. Drag the lower right corner, use Ctrl + mouse wheel, or right-click to resize.");
            const auto&io=ImGui::GetIO();
            if(ImGui::IsWindowHovered()&&io.KeyCtrl&&io.MouseWheel!=0){cameraModesScale_=std::clamp(cameraModesScale_+io.MouseWheel*.1f,.5f,3.f);cameraModesDirty_=true;}
            if(ImGui::BeginPopupContextWindow("##camera-modes-menu")){
                if(ImGui::SliderFloat("Size",&cameraModesScale_,.5f,3.f,"%.2f"))cameraModesDirty_=true;
                if(ImGui::MenuItem("Reset position")){cameraModesX_=-1;cameraModesY_=-1;cameraModesDirty_=true;}
                ImGui::EndPopup();}
            const bool dragNow=ImGui::IsWindowHovered()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left,3.f)&&!ImGui::IsAnyItemActive()&&!cameraModesGrip_&&!cameraModesResizing_;
            if(dragNow)cameraModesDragging_=true;
            if(cameraModesDragging_){const ImVec2 p=ImGui::GetWindowPos();cameraModesX_=p.x;cameraModesY_=p.y;
                if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)){cameraModesDragging_=false;cameraModesDirty_=true;}}
            if(cameraModesDirty_&&!cameraModesDragging_){cameraModesDirty_=false;SaveSettings();}
        }
        const char*names[]={"Default","Free","Dolly","Bone"};
        for(int mode=0;mode<4;++mode){if(mode)ImGui::SameLine();ImGui::PushID(mode);
            ImVec2 start=ImGui::GetCursorScreenPos();const float w=Px(72,s),h=Px(48,s);
            if(ImGui::InvisibleButton("mode",ImVec2(w,h))&&shown){if(mode==3&&camera.bone_index<0)AutoSelectBone();camera_runtime::mode(mode);camera_runtime::enable(mode!=0);}
            auto*draw=ImGui::GetWindowDrawList();const bool active=camera.mode==unsigned(mode);
            draw->AddRectFilled(start,ImVec2(start.x+w,start.y+h),(active?Color::SelectedBg:Color::ChildBg).U32(),Px(5,s));
            const ImU32 color=(active?Color::AccentBlue:Color::TextPrimary).U32();float x=start.x+w*.5f,y=start.y+Px(15,s);
            if(mode==0){draw->AddCircle(ImVec2(x,y-Px(5,s)),Px(4,s),color);draw->AddLine(ImVec2(x,y),ImVec2(x,y+Px(8,s)),color,2);draw->AddLine(ImVec2(x-Px(7,s),y+Px(4,s)),ImVec2(x+Px(7,s),y+Px(4,s)),color,2);}
            else if(mode==3){ // bone with a camera dot
                draw->AddLine(ImVec2(x-Px(9,s),y+Px(6,s)),ImVec2(x+Px(7,s),y-Px(6,s)),color,2);
                draw->AddCircle(ImVec2(x-Px(10,s),y+Px(7,s)),Px(3,s),color,0,2);draw->AddCircle(ImVec2(x+Px(9,s),y-Px(8,s)),Px(3,s),color,0,2);
                draw->AddCircleFilled(ImVec2(x+Px(9,s),y+Px(7,s)),Px(2.5f,s),color);}
            else {draw->AddRect(ImVec2(x-Px(10,s),y-Px(5,s)),ImVec2(x+Px(3,s),y+Px(5,s)),color,2);
                draw->AddTriangle(ImVec2(x+Px(3,s),y),ImVec2(x+Px(11,s),y-Px(6,s)),ImVec2(x+Px(11,s),y+Px(6,s)),color,2);
                if(mode==2){draw->AddLine(ImVec2(x-Px(12,s),y+Px(9,s)),ImVec2(x+Px(12,s),y+Px(9,s)),color);for(int j=-1;j<=1;++j)draw->AddCircleFilled(ImVec2(x+j*Px(10,s),y+Px(9,s)),Px(2,s),color);}}
            const auto label=ImGui::CalcTextSize(names[mode]);draw->AddText(ImVec2(x-label.x*.5f,start.y+h-label.y-Px(3,s)),Color::TextPrimary.U32(),names[mode]);
            if(shown&&ImGui::IsItemHovered())ImGui::SetTooltip("%s camera. %s",names[mode],mode==3?"Attached to the bone chosen in the Bones tab. Move and turn like the free camera; it follows the bone.":mode==2?"Author keys: move and capture with K. Play path: follow recorded keys at ReplayTime.":mode==1?"Move independently from the player; mouse wheel adjusts FOV.":"Native player camera; Theater releases camera ownership.");
            ImGui::PopID();
        }
        const auto cycleKey=KeyName(theater_hotkeys::Action::CycleCamera);
        ImGui::TextDisabled("%s | %s",cycleKey.c_str(),camera.mode==0?"Player":camera.enabled?"Active":"Not armed");
        const auto visibilityKey=KeyName(theater_hotkeys::Action::ToggleDollyControls);
        ImGui::TextDisabled("Dolly controls: %s | %s",showDollyMarkers_?"visible":"hidden",enableDollyVisibilityKey_?(visibilityKey+": toggle").c_str():"shortcut disabled");
        if(camera.mode==2){
            ImGui::TextDisabled("%s | %zu keys | %s: play path",camera.dolly_preview?"Preview":"Authoring",camera.track_current?camera.key_count:0,KeyName(theater_hotkeys::Action::PlayDollyPath).c_str());
            if(shown){
                ImGui::BeginDisabled(!camera.track_current||camera.key_count<2);
                if(ImGui::Button("Play path"))PlayDollyPath(f);
                ImGui::EndDisabled();ImGui::SameLine();
                if(ImGui::Button("Author keys"))camera_runtime::preview(false);
                const auto capture="Capture ["+KeyName(theater_hotkeys::Action::AddDollyKey)+"]";
                if(ImGui::Button(capture.c_str()))camera_runtime::add_key();ImGui::SameLine();
                const auto clear="Clear ["+KeyName(theater_hotkeys::Action::ClearDollyKeys)+"]";
                if(ImGui::Button(clear.c_str()))clearDollyDialog_=true;
            }
        }
    }
    if(visible&&shown){
        // Resize grip in the lower right corner: drag it to make the whole widget bigger or smaller.
        const ImVec2 wp=ImGui::GetWindowPos(),ws=ImGui::GetWindowSize();const float g=Px(16,s);
        const ImVec2 a(wp.x+ws.x-g,wp.y+ws.y-g),b(wp.x+ws.x,wp.y+ws.y);auto&io=ImGui::GetIO();
        cameraModesGrip_=io.MousePos.x>=a.x&&io.MousePos.x<=b.x&&io.MousePos.y>=a.y&&io.MousePos.y<=b.y;
        if(cameraModesGrip_&&ImGui::IsMouseClicked(ImGuiMouseButton_Left))cameraModesResizing_=true;
        if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)){if(cameraModesResizing_)cameraModesDirty_=true;cameraModesResizing_=false;}
        if(cameraModesResizing_){cameraModesScale_=std::clamp(cameraModesScale_*(1.f+io.MouseDelta.x/std::max(40.f,ws.x)),.5f,3.f);}
        if(cameraModesGrip_||cameraModesResizing_)ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
        const ImU32 col=(cameraModesGrip_||cameraModesResizing_?Color::AccentAmber:Color::TextSecondary).U32();auto*d=ImGui::GetWindowDrawList();
        for(int i=0;i<3;++i){const float o=Px(4.f+i*4.f,s);d->AddLine(ImVec2(b.x-o,b.y-Px(2,s)),ImVec2(b.x-Px(2,s),b.y-o),col,Px(1.4f,s));}
        if(cameraModesDirty_&&!cameraModesResizing_&&!cameraModesDragging_){cameraModesDirty_=false;SaveSettings();}
    }
    ImGui::End();ImGui::PopStyleColor();
}

void Overlay::DrawGameViewport(const OverlayFrame& f)
{
    auto&io=ImGui::GetIO();if(!f.game_texture){gameViewInitialized_=false;return;}
    const float s=ui_.rects.uiScale,gap=Px(6,s),top=menuH_+gap;
    const float minHeight=Px(80,s);
    const float bottom=std::max(top+minHeight,sequencerTop_-gap);
    const float right=std::min(io.DisplaySize.x,sequencerRight_);
    ImVec2 pos(std::min(ui_.rects.areaMin.x,std::max(0.f,right-Px(240,s))),top),size(std::max(Px(240,s),right-pos.x),std::max(minHeight,bottom-top));
    ImGui::SetNextWindowPos(pos,resetLayout_?ImGuiCond_Always:ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(size,resetLayout_?ImGuiCond_Always:ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(Px(240,s),minHeight),ImVec2(std::max(Px(240,s),right),bottom-top));
    ImGui::Begin("Game viewport###game-viewport",nullptr,ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoCollapse);
    auto p=ImGui::GetWindowPos(),sz=ImGui::GetWindowSize();
    auto*window=ImGui::GetCurrentWindow();bool resizing=window->ResizeBorderHeld>=0;
    for(int corner=0;corner<4;++corner)resizing|=ImGui::GetCurrentContext()->ActiveId==ImGui::GetWindowResizeCornerID(window,corner);
    if(resizing&&gameViewFit_){gameViewFit_=false;SaveSettings();}
    p.y=std::clamp(p.y,top,std::max(top,bottom-minHeight));p.x=std::clamp(p.x,0.f,std::max(0.f,right-Px(240,s)));
    if(gameViewFit_)sz=ImVec2(right-p.x,bottom-p.y);
    sz.x=std::min(sz.x,right-p.x);
    sz.y=std::min(sz.y,bottom-p.y);ImGui::SetWindowPos(p);ImGui::SetWindowSize(sz);
    ImGui::TextDisabled("Gizmo: %s | Middle click in picture to switch",gizmoOperation_==0?"Move":"Rotate");
    ImGui::SameLine();if(ImGui::Checkbox("Fit sequencer",&gameViewFit_))SaveSettings();
    ImVec2 content=ImGui::GetCursorScreenPos(),end(p.x+sz.x-ImGui::GetStyle().WindowPadding.x,bottom);
    end.y=std::min(end.y,p.y+sz.y-ImGui::GetStyle().WindowPadding.y);
    if(content.y>=end.y){ui_.rects.gameMin=ui_.rects.gameMax=end;gameViewInitialized_=false;ImGui::End();return;}
    ImVec2 targetMin,targetMax;FitAspect(content.x,content.y,std::max(1.f,end.x-content.x),std::max(1.f,end.y-content.y),io.DisplaySize.x/io.DisplaySize.y,targetMin,targetMax);
    if(!gameViewInitialized_){ui_.rects.gameMin=targetMin;ui_.rects.gameMax=targetMax;gameViewInitialized_=true;}
    else {const float weight=float(-std::expm1(-std::max(0.f,io.DeltaTime)/.12));
        // Keep animation history outside SolveLayout's per-frame defaults.
        ui_.rects.gameMin=ImVec2(gameViewMin_.x+(targetMin.x-gameViewMin_.x)*weight,gameViewMin_.y+(targetMin.y-gameViewMin_.y)*weight);
        ui_.rects.gameMax=ImVec2(gameViewMax_.x+(targetMax.x-gameViewMax_.x)*weight,gameViewMax_.y+(targetMax.y-gameViewMax_.y)*weight);
        ImVec2 a=ui_.rects.gameMin,b=ui_.rects.gameMax;
        a.x=std::clamp(a.x,content.x,end.x);a.y=std::clamp(a.y,content.y,end.y);b.x=std::clamp(b.x,a.x,end.x);b.y=std::clamp(b.y,a.y,end.y);
        FitAspect(a.x,a.y,std::max(1.f,b.x-a.x),std::max(1.f,b.y-a.y),io.DisplaySize.x/io.DisplaySize.y,ui_.rects.gameMin,ui_.rects.gameMax);
    }
    gameViewMin_=ui_.rects.gameMin;gameViewMax_=ui_.rects.gameMax;
    if(f.focused&&!ImGui::GetIO().WantTextInput&&ImGui::IsWindowHovered()&&io.MousePos.x>=gameViewMin_.x&&io.MousePos.x<=gameViewMax_.x&&io.MousePos.y>=gameViewMin_.y&&io.MousePos.y<=gameViewMax_.y&&ImGui::IsMouseClicked(ImGuiMouseButton_Middle)){gizmoOperation_=1-gizmoOperation_;gizmoDragging_=false;lightGizmoDragging_=false;}
    auto*draw=ImGui::GetBackgroundDrawList();draw->AddRectFilled(ImVec2(0,0),io.DisplaySize,Color::TimelineBg.U32());
    draw->AddImage(ImTextureRef(f.game_texture),ui_.rects.gameMin,ui_.rects.gameMax);
    ImGui::End();
}

void Overlay::DrawDollyViewport(const OverlayFrame& f)
{
    using namespace cinematic;using namespace cinematic::viewport;
    if(ui_.visibility==UiVisibility::HiddenClean||(!showDollyMarkers_&&!showLightMarkers_)){gizmoDragging_=lightGizmoDragging_=false;return;}
    auto camera=camera_runtime::view();const auto lights=light_editor::view();
    const bool hasDolly=showDollyMarkers_&&camera.track_current&&!camera.keys.empty();
    const bool hasLights=showLightMarkers_&&!lights.lights.empty();
    const bool hasBones=showBoneDots_&&ui_.activeTool==Tool::Bones&&ui_.visibility==UiVisibility::Shown;
    if(!camera.observed||(!hasDolly&&!hasLights&&!hasBones)){gizmoDragging_=lightGizmoDragging_=false;return;}
    if(!hasDolly)gizmoDragging_=false;
    auto&io=ImGui::GetIO();const float s=ui_.rects.uiScale;const auto display=io.DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0,menuH_),ImGuiCond_Always);ImGui::SetNextWindowSize(ImVec2(display.x,std::max(1.f,display.y-menuH_)),ImGuiCond_Always);
    const bool interactive=ui_.visibility==UiVisibility::Shown&&f.focused&&!io.WantTextInput&&bindingWaiting_<0;
    if(!interactive)gizmoDragging_=lightGizmoDragging_=false;
    auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoFocusOnAppearing;
    if(!interactive||f.game_texture)flags|=ImGuiWindowFlags_NoInputs;
    ImGui::Begin("##dolly-viewport",nullptr,flags);auto*draw=ImGui::GetBackgroundDrawList();
    const bool scaled=ui_.visibility==UiVisibility::Shown&&f.game_texture;
    const auto pictureMin=scaled?ui_.rects.gameMin:ImVec2(0,0),pictureMax=scaled?ui_.rects.gameMax:display;
    draw->PushClipRect(pictureMin,pictureMax,true);
    if(pictureMax.x<=pictureMin.x||pictureMax.y<=pictureMin.y){gizmoDragging_=lightGizmoDragging_=false;draw->PopClipRect();ImGui::End();return;}
    bool hovered=interactive&&(ImGui::IsWindowHovered()||(scaled&&ImGui::GetCurrentContext()->HoveredWindow==ImGui::FindWindowByName("###game-viewport")))&&io.MousePos.x>=pictureMin.x&&io.MousePos.x<=pictureMax.x&&io.MousePos.y>=pictureMin.y&&io.MousePos.y<=pictureMax.y;
    auto projectPoint=[&](Vec p){auto result=project(camera.pose,p,display.x,display.y);if(result&&scaled){result->x=pictureMin.x+result->x/display.x*(pictureMax.x-pictureMin.x);result->y=pictureMin.y+result->y/display.y*(pictureMax.y-pictureMin.y);}return result;};
    auto mousePoint=[&](){return ImVec2((io.MousePos.x-pictureMin.x)*display.x/(pictureMax.x-pictureMin.x),(io.MousePos.y-pictureMin.y)*display.y/(pictureMax.y-pictureMin.y));};
    auto line=[&](Vec a,Vec b,ImU32 color,float width=1.f){auto pa=projectPoint(a),pb=projectPoint(b);if(pa&&pb)draw->AddLine(ImVec2(float(pa->x),float(pa->y)),ImVec2(float(pb->x),float(pb->y)),color,width);};
    const bool lightInteraction=hasLights&&DrawLightViewport(f,camera.pose,pictureMin,pictureMax,hovered,scaled);
    const bool particleInteraction=showParticleMarkers_&&DrawParticleViewport(f,camera.pose,pictureMin,pictureMax,hovered&&!lightInteraction,scaled);
    const bool boneInteraction=hasBones&&DrawBoneViewport(f,camera.pose,pictureMin,pictureMax,hovered&&!lightInteraction&&!particleInteraction,scaled);
    hovered=hovered&&!lightInteraction&&!particleInteraction&&!boneInteraction;
    if(hasDolly){
    if(curveGeneration_!=camera.project_generation){curveTrack_.replace(camera.keys,camera.track_settings);curveGeneration_=camera.project_generation;}
    if(camera.keys.size()>1){
        auto first=camera.keys.front().time_ns,last=camera.keys.back().time_ns;auto previous=cinematic::dolly_evaluate(curveTrack_,first,camera.dolly_smoothing_seconds);
        for(int i=1;i<=128;++i){auto t=first+std::uint64_t(double(last-first)*i/128);auto current=cinematic::dolly_evaluate(curveTrack_,t,camera.dolly_smoothing_seconds);if(previous&&current)line(previous->position,current->position,Color::AccentBlue.Alpha(140).U32());previous=current;}
    }
    if(std::none_of(camera.keys.begin(),camera.keys.end(),[&](auto&k){return k.id==selectedDollyKey_;})){selectedDollyKey_=0;gizmoDragging_=false;}
    for(const auto&key:camera.keys){
        const auto center=projectPoint(key.state.position);if(!center)continue;
        const ImU32 color=(selectedDollyKeys_.contains(key.id)?Color::AccentAmber:Color::AccentBlue).U32();
        const auto p=key.state.position;const auto right=basis(key.state.orientation,0),up=basis(key.state.orientation,1),forward=basis(key.state.orientation,2);
        Vec corner[4];for(int j=0;j<4;++j)corner[j]=add(add(add(p,mul(forward,.65)),mul(right,(j==0||j==3?-.4:.4))),mul(up,(j<2?.25:-.25)));
        for(int j=0;j<4;++j){line(p,corner[j],color);line(corner[j],corner[(j+1)%4],color);}
        line(add(p,mul(up,.42)),add(p,mul(forward,.35)),color,2);
        ImVec2 pixel(float(center->x),float(center->y));draw->AddCircleFilled(pixel,Px(4,s),color);
        char name[48];snprintf(name,sizeof(name),"Camera %llu",static_cast<unsigned long long>(key.id));draw->AddText(ImVec2(pixel.x+Px(8,s),pixel.y),color,name);
        if(hovered&&!gizmoDragging_&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&std::hypot(io.MousePos.x-pixel.x,io.MousePos.y-pixel.y)<Px(14,s))SelectDollyKey(key.id,io.KeyCtrl,io.KeyShift);
    }
    auto selected=std::find_if(camera.keys.begin(),camera.keys.end(),[&](auto&k){return k.id==selectedDollyKey_;});
    if(!viewportLightSelected_&&selected!=camera.keys.end())if(auto center=projectPoint(selected->state.position)){
        const auto origin=selected->state.position;const ImU32 colors[]={IM_COL32(245,85,85,255),IM_COL32(100,220,110,255),IM_COL32(95,150,255,255)};
        const double extent=std::max(.1,center->depth*std::tan(camera.pose.fov_degrees*3.141592653589793/360)*.18);
        for(int axis=0;axis<3;++axis){
            Vec direction{};direction[axis]=1;
            if(gizmoOperation_==0){auto end=projectPoint(add(origin,mul(direction,extent)));if(!end)continue;
                const double pixels=std::hypot(end->x-center->x,end->y-center->y);if(pixels<12)continue;
                line(origin,add(origin,mul(direction,extent)),colors[axis],Px(3,s));draw->AddCircleFilled(ImVec2(float(end->x),float(end->y)),Px(5,s),colors[axis]);
                if(hovered&&!gizmoDragging_&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&std::hypot(io.MousePos.x-center->x,io.MousePos.y-center->y)>Px(14,s)&&segment_distance(io.MousePos.x,io.MousePos.y,*center,*end)<Px(7,s)){
                    camera_runtime::begin_edit();gizmoDragging_=true;gizmoAxis_=axis;gizmoStart_=*selected;gizmoMouseStart_=io.MousePos;gizmoPixelsPerUnit_=pixels/extent;gizmoScreenAxis_={float((end->x-center->x)/pixels),float((end->y-center->y)/pixels)};}
            }else {
                double hit=1e9;for(int j=0;j<64;++j){auto point=[&](int n){Vec v=origin;const double a=n*6.283185307179586/64;v[(axis+1)%3]+=extent*std::cos(a);v[(axis+2)%3]+=extent*std::sin(a);return v;};
                    auto a=projectPoint(point(j)),b=projectPoint(point(j+1));if(a&&b){draw->AddLine(ImVec2(float(a->x),float(a->y)),ImVec2(float(b->x),float(b->y)),colors[axis],Px(2,s));hit=std::min(hit,segment_distance(io.MousePos.x,io.MousePos.y,*a,*b));}}
                if(hovered&&!gizmoDragging_&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&hit<Px(7,s))if(auto angle=plane_angle(camera.pose,mousePoint().x,mousePoint().y,display.x,display.y,origin,axis)){
                    camera_runtime::begin_edit();gizmoDragging_=true;gizmoAxis_=axis;gizmoStart_=*selected;gizmoCenter_={float(center->x),float(center->y)};gizmoAngle_=*angle;gizmoMouseStart_=io.MousePos;}
            }
        }
    }
    if(gizmoDragging_){
        auto draft=gizmoStart_;
        if(gizmoOperation_==0){double delta=((io.MousePos.x-gizmoMouseStart_.x)*gizmoScreenAxis_.x+(io.MousePos.y-gizmoMouseStart_.y)*gizmoScreenAxis_.y)/gizmoPixelsPerUnit_;draft.state.position[gizmoAxis_]+=delta;}
        else if(auto angle=plane_angle(camera.pose,mousePoint().x,mousePoint().y,display.x,display.y,gizmoStart_.state.position,gizmoAxis_)){if(auto rotation=rotate_world(draft.state.orientation,gizmoAxis_,std::remainder(*angle-gizmoAngle_,6.283185307179586)))draft.state.orientation=*rotation;}
        if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){camera_runtime::edit_key(gizmoStart_);gizmoDragging_=false;}
        else if(f.now-gizmoLastCommit_>=1./30||!io.MouseDown[0]){camera_runtime::edit_key(draft);gizmoLastCommit_=f.now;}
        if(!io.MouseDown[0])gizmoDragging_=false;
    }
    } // dolly markers
    draw->PopClipRect();ImGui::End();
}

// Light definitions share the camera projection and gizmo conventions; this draws editor handles,
// Small dots on the character's bones; click one to attach the bone camera to it.
bool Overlay::DrawBoneViewport(const OverlayFrame& f,const cinematic::State& camera,ImVec2 min,ImVec2 max,bool hovered,bool scaled)
{
    using namespace cinematic; using namespace cinematic::viewport;
    const auto dots=camera_runtime::bone_dots();if(dots.empty())return false;
    const auto names=camera_runtime::bone_names();const auto cam=camera_runtime::view(false);
    auto& io=ImGui::GetIO(); const float s=ui_.rects.uiScale; const auto display=io.DisplaySize; auto* draw=ImGui::GetBackgroundDrawList();
    auto minor=[&](const std::string& n){
        static const char* skip[]={"twist","finger","thumb","index","middle","ring","pinky","toe","roll","helper","dummy","cloth","cape","skirt","hair","prop","scabbard","sheath"};
        std::string l=n;for(auto&c:l)c=(char)std::tolower((unsigned char)c);
        for(const char*k:skip)if(l.find(k)!=std::string::npos)return true;return false;};
    int hit=-1;double nearest=Px(13,s);struct Dot{int i;ImVec2 p;};std::vector<Dot> shown;
    for(int i=0;i<(int)dots.size();++i){
        const std::string name=i<(int)names.size()?names[i]:std::string("bone ")+std::to_string(i);
        if(boneDotsMajorOnly_&&minor(name)&&cam.bone_index!=i)continue;
        auto r=project(camera,Vec{dots[i][0],dots[i][1],dots[i][2]},display.x,display.y);if(!r)continue;
        if(scaled){r->x=min.x+r->x/display.x*(max.x-min.x);r->y=min.y+r->y/display.y*(max.y-min.y);}
        shown.push_back({i,ImVec2(float(r->x),float(r->y))});
        const double d=std::hypot(io.MousePos.x-r->x,io.MousePos.y-r->y);if(hovered&&d<nearest){nearest=d;hit=i;}
    }
    for(const auto& d:shown){
        const bool selected=cam.bone_index==d.i,over=hit==d.i;
        const ImU32 col=selected?Color::AccentAmber.U32():over?IM_COL32(255,255,255,255):IM_COL32(120,205,255,200);
        draw->AddCircleFilled(d.p,Px(over||selected?5.f:3.2f,s),col);
        if(over||selected)draw->AddCircle(d.p,Px(9,s),col,16,Px(1.2f,s));
        if(over||selected){const std::string n=d.i<(int)names.size()?names[d.i]:std::to_string(d.i);draw->AddText(ImVec2(d.p.x+Px(11,s),d.p.y-Px(6,s)),col,n.c_str());}
    }
    if(hit>=0&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
        camera_runtime::bone_attach(hit);camera_runtime::mode(3);camera_runtime::enable(true);return true;}
    return false;
}

bool Overlay::DrawParticleViewport(const OverlayFrame& f,const cinematic::State& camera,ImVec2 min,ImVec2 max,bool hovered,bool scaled)
{
    using namespace cinematic; using namespace cinematic::viewport;
    auto editor=particle_editor::view(); if(editor.emitters.empty()) return false;
    auto& io=ImGui::GetIO(); const float s=ui_.rects.uiScale; const auto display=io.DisplaySize; auto* draw=ImGui::GetBackgroundDrawList();
    auto projectPoint=[&](Vec p){auto r=project(camera,p,display.x,display.y);if(r&&scaled){r->x=min.x+r->x/display.x*(max.x-min.x);r->y=min.y+r->y/display.y*(max.y-min.y);}return r;};
    bool consumed=false;
    for(const auto& e:editor.emitters){auto c=projectPoint(e.transform.position);if(!c)continue;ImVec2 p(float(c->x),float(c->y));
        const ImU32 col=e.id==editor.selected?IM_COL32(255,180,75,255):IM_COL32(165,225,255,e.enabled?220:90);
        draw->AddCircleFilled(p,Px(5,s),col); draw->AddCircle(p,Px(10,s),col,16,Px(1,s));
        char label[64];snprintf(label,sizeof(label),"Particle %llu",static_cast<unsigned long long>(e.id));draw->AddText(ImVec2(p.x+Px(9,s),p.y),col,label);
        if(hovered&&!particleGizmoDragging_&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&std::hypot(io.MousePos.x-p.x,io.MousePos.y-p.y)<Px(14,s)){particle_editor::select(e.id);viewportParticleSelected_=true;particleGizmoStart_=e;particleGizmoMouseStart_=io.MousePos;particleGizmoPixelsPerUnit_=std::max(1.,double(20*display.y/std::max(1.f,max.y-min.y)));particleGizmoDragging_=true;consumed=true;}
    }
    if(particleGizmoDragging_){auto current=particle_editor::view();auto it=std::find_if(current.emitters.begin(),current.emitters.end(),[&](auto&e){return e.id==current.selected;});if(it!=current.emitters.end()){
        auto draft=*it; draft.transform=particleGizmoStart_.transform; const double dx=(io.MousePos.x-particleGizmoMouseStart_.x)/particleGizmoPixelsPerUnit_,dy=(io.MousePos.y-particleGizmoMouseStart_.y)/particleGizmoPixelsPerUnit_;
        auto right=basis(camera.orientation,0),up=basis(camera.orientation,1);for(int i=0;i<3;++i)draft.transform.position[i]+=right[i]*dx-up[i]*dy;
        if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){draft.transform=particleGizmoStart_.transform;particle_editor::edit(draft);particleGizmoDragging_=false;}else {particle_editor::edit(draft);if(!io.MouseDown[0])particleGizmoDragging_=false;}consumed=true;}}
    return consumed;
}

// not native illumination. Called inside the same clipped viewport window as dolly markers.
bool Overlay::DrawLightViewport(const OverlayFrame& f,const cinematic::State& camera,ImVec2 min,ImVec2 max,bool hovered,bool scaled)
{
    using namespace cinematic;using namespace cinematic::viewport;
    auto editor=light_editor::view();auto& io=ImGui::GetIO();const auto display=io.DisplaySize;
    const float s=ui_.rects.uiScale;auto* draw=ImGui::GetBackgroundDrawList();
    auto projectPoint=[&](Vec p){auto result=project(camera,p,display.x,display.y);if(result&&scaled){result->x=min.x+result->x/display.x*(max.x-min.x);result->y=min.y+result->y/display.y*(max.y-min.y);}return result;};
    auto mousePoint=[&](){return ImVec2((io.MousePos.x-min.x)*display.x/(max.x-min.x),(io.MousePos.y-min.y)*display.y/(max.y-min.y));};
    auto line=[&](Vec a,Vec b,ImU32 color,float width=1.f){auto pa=projectPoint(a),pb=projectPoint(b);if(pa&&pb)draw->AddLine(ImVec2(float(pa->x),float(pa->y)),ImVec2(float(pb->x),float(pb->y)),color,width);};
    bool consumed=lightGizmoDragging_;
    std::uint64_t hitId=0;double nearest=Px(14,s);
    for(const auto& light:editor.lights){
        auto center=projectPoint(light.transform.position);if(!center)continue;
        ImVec2 pixel(float(center->x),float(center->y));
        ImU32 color=viewportLightSelected_&&editor.selected==light.id?Color::AccentAmber.U32():IM_COL32(255,221,135,light.enabled?230:100);
        if(light.type==light_editor::Type::Point){
            draw->AddCircle(pixel,Px(6,s),color,16,Px(1.5f,s));
            for(int i=0;i<8;++i){double a=i*6.283185307179586/8;draw->AddLine(ImVec2(pixel.x+float(std::cos(a))*Px(9,s),pixel.y+float(std::sin(a))*Px(9,s)),ImVec2(pixel.x+float(std::cos(a))*Px(13,s),pixel.y+float(std::sin(a))*Px(13,s)),color,Px(1.5f,s));}
        }else{
            draw->AddTriangle(ImVec2(pixel.x,pixel.y-Px(8,s)),ImVec2(pixel.x-Px(8,s),pixel.y+Px(8,s)),ImVec2(pixel.x+Px(8,s),pixel.y+Px(8,s)),color,Px(2,s));
            const auto p=light.transform.position,forward=basis(light.transform.orientation,2),right=basis(light.transform.orientation,0),up=basis(light.transform.orientation,1);
            const double length=std::max(.2,center->depth*.035),radius=std::min(length*2,length*std::tan(light.cone_degrees*3.141592653589793/360));
            for(int i=0;i<24;++i){auto rim=[&](int j){double a=j*6.283185307179586/24;return add(add(add(p,mul(forward,length)),mul(right,radius*std::cos(a))),mul(up,radius*std::sin(a)));};line(rim(i),rim(i+1),color);if(i%6==0)line(p,rim(i),color);}
        }
        draw->AddText(ImVec2(pixel.x+Px(17,s),pixel.y-Px(6,s)),color,light.name.c_str());
        const double distance=std::hypot(io.MousePos.x-center->x,io.MousePos.y-center->y);
        if(hovered&&distance<nearest){nearest=distance;hitId=light.id;}
    }
    if(hitId&&hovered&&!gizmoDragging_&&!lightGizmoDragging_&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
        light_editor::select(hitId);editor.selected=hitId;viewportLightSelected_=true;consumed=true;
    }
    auto selected=std::find_if(editor.lights.begin(),editor.lights.end(),[&](const auto& l){return l.id==editor.selected;});
    if(selected==editor.lights.end()||(lightGizmoDragging_&&selected->id!=lightGizmoStart_.id)){lightGizmoDragging_=false;return consumed;}
    if(!viewportLightSelected_)return consumed;
    if(auto center=projectPoint(selected->transform.position)){
        const auto origin=selected->transform.position;
        const ImU32 colors[]={IM_COL32(245,85,85,255),IM_COL32(100,220,110,255),IM_COL32(95,150,255,255)};
        const double extent=std::max(.1,center->depth*std::tan(camera.fov_degrees*3.141592653589793/360)*.18);
        for(int axis=0;axis<3;++axis){Vec direction{};direction[axis]=1;
            if(gizmoOperation_==0){auto end=projectPoint(add(origin,mul(direction,extent)));if(!end)continue;
                const double pixels=std::hypot(end->x-center->x,end->y-center->y);if(pixels<12)continue;
                line(origin,add(origin,mul(direction,extent)),colors[axis],Px(3,s));draw->AddCircleFilled(ImVec2(float(end->x),float(end->y)),Px(5,s),colors[axis]);
                if(hovered&&!consumed&&!gizmoDragging_&&!lightGizmoDragging_&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&std::hypot(io.MousePos.x-center->x,io.MousePos.y-center->y)>Px(14,s)&&segment_distance(io.MousePos.x,io.MousePos.y,*center,*end)<Px(7,s)){
                    lightGizmoDragging_=true;lightGizmoAxis_=axis;lightGizmoStart_=*selected;lightGizmoMouseStart_=io.MousePos;lightGizmoPixelsPerUnit_=pixels/extent;lightGizmoScreenAxis_={float((end->x-center->x)/pixels),float((end->y-center->y)/pixels)};consumed=true;
                }
            }else{
                double hit=1e9;for(int i=0;i<64;++i){auto ring=[&](int j){Vec p=origin;double a=j*6.283185307179586/64;p[(axis+1)%3]+=extent*std::cos(a);p[(axis+2)%3]+=extent*std::sin(a);return p;};auto a=projectPoint(ring(i)),b=projectPoint(ring(i+1));if(a&&b){draw->AddLine(ImVec2(float(a->x),float(a->y)),ImVec2(float(b->x),float(b->y)),colors[axis],Px(2,s));hit=std::min(hit,segment_distance(io.MousePos.x,io.MousePos.y,*a,*b));}}
                if(hovered&&!consumed&&!gizmoDragging_&&!lightGizmoDragging_&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&hit<Px(7,s))if(auto angle=plane_angle(camera,mousePoint().x,mousePoint().y,display.x,display.y,origin,axis)){
                    lightGizmoDragging_=true;lightGizmoAxis_=axis;lightGizmoStart_=*selected;lightGizmoMouseStart_=io.MousePos;lightGizmoAngle_=*angle;consumed=true;
                }
            }
        }
    }
    if(lightGizmoDragging_){
        auto draft=*selected; // Preserve color/intensity and all non-transform edits during the gesture.
        draft.transform=lightGizmoStart_.transform;
        if(gizmoOperation_==0){double delta=((io.MousePos.x-lightGizmoMouseStart_.x)*lightGizmoScreenAxis_.x+(io.MousePos.y-lightGizmoMouseStart_.y)*lightGizmoScreenAxis_.y)/lightGizmoPixelsPerUnit_;draft.transform.position[lightGizmoAxis_]+=delta;}
        else if(auto angle=plane_angle(camera,mousePoint().x,mousePoint().y,display.x,display.y,lightGizmoStart_.transform.position,lightGizmoAxis_))if(auto q=rotate_world(draft.transform.orientation,lightGizmoAxis_,std::remainder(*angle-lightGizmoAngle_,6.283185307179586)))draft.transform.orientation=*q;
        if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){draft.transform=lightGizmoStart_.transform;light_editor::edit(draft);lightGizmoDragging_=false;}
        else if(f.now-lightGizmoLastCommit_>=1./30||!io.MouseDown[0]){light_editor::edit(draft);lightGizmoLastCommit_=f.now;}
        if(!io.MouseDown[0])lightGizmoDragging_=false;
        consumed=true;
    }
    return consumed;
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
        if(ImGui::MenuItem(T(Str::EventLog), nullptr, &showEventLog_))SaveSettings();
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
    const float btn = std::min(Px(Metric::RailButton, s), std::max(Px(32,s),(ImGui::GetIO().DisplaySize.y-Px(110,s))/11.f-Px(4,s)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Color::PanelBgSolid.Vec4());
    const bool visible = BeginPanel("###tools", T(Str::PanelTools), ui_.rects.railMin, ui_.rects.railMax,
        ImVec2(btn + Px(8, s), (btn + Px(4, s)) * 11 + Px(60, s)), &showTools_);
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
        { Tool::Look, Glyph::Look, Str::Look, true }, { Tool::Weather, Glyph::Globe, Str::Weather, true }, { Tool::Lights, Glyph::Look, Str::Lights, true }, { Tool::Particles, Glyph::Globe, Str::Particles, true }, { Tool::Bones, Glyph::Scene, Str::Bones, true }, { Tool::Replays, Glyph::Replays, Str::Replays, true },
        { Tool::Export, Glyph::Export, Str::Export, true } };
    const Item bottom[] = { { Tool::Debug, Glyph::Debug, Str::Debug, true }, { Tool::Settings, Glyph::Settings, Str::Settings, true } };

    auto railButton = [&](const Item& it, float y, bool warning)
    {
        const ImVec2 min(o.x + (w - btn) * 0.5f, y), max(min.x + btn, y + btn);
        ImGui::SetCursorScreenPos(min);
        ImGui::PushID((int)it.tool);
        const bool clicked = ImGui::InvisibleButton("##rail", ImVec2(btn, btn));
        const bool hovered = ImGui::IsItemHovered();
        if(hovered&&it.tool==Tool::Weather)ImGui::SetTooltip("%s",T(Str::WeatherEditor));
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
    const Str titles[] = { Str::Scene, Str::Camera, Str::Look, Str::Replays, Str::Export, Str::Debug, Str::Settings, Str::Weather, Str::Lights, Str::Particles, Str::Bones };
    const bool visible = BeginPanel("###panel", T(titles[std::min<int>((int)ui_.activeTool, 10)]), ui_.rects.panelMin, ui_.rects.panelMax,
        ImVec2(Px(260, s), Px(320, s)), &ui_.layout.panelOpen);
    if (!visible) { ImGui::End(); ImGui::PopStyleVar(); return; }
    // Hiding the log returns its space to the independently scrolling tool.
    const float availableH=std::max(1.f,ImGui::GetContentRegionAvail().y);
    const float logReserve=showEventLog_?std::min(Px(150,s),availableH*.4f):0.f;
    ImGui::PushID(static_cast<int>(ui_.activeTool));
    ImGui::BeginChild("##tool-content",ImVec2(0,std::max(1.f,availableH-logReserve)),ImGuiChildFlags_None,ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::PushTextWrapPos(0.f);
    auto labelAbove=[&](const char* label){ImGui::TextWrapped("%s",label);ImGui::SetNextItemWidth(-FLT_MIN);};
    auto number=[&](const char* label,double* value,double step=0,double fast=0,const char* format="%.6f"){
        ImGui::PushID(label);labelAbove(label);bool changed=ImGui::InputDouble("##value",value,step,fast,format);ImGui::PopID();return changed;};
    auto slider=[&](const char* label,float* value,float min,float max,const char* format="%.3f",ImGuiSliderFlags flags=0,float resetValue=0){
        ImGui::PushID(label);labelAbove(label);bool changed=ImGui::SliderFloat("##value",value,min,max,format,flags);
        if(ImGui::IsItemHovered()&&(ImGui::IsMouseClicked(ImGuiMouseButton_Middle)||ImGui::IsMouseClicked(ImGuiMouseButton_Right))){*value=resetValue;changed=true;}
        if(ImGui::IsItemHovered())ImGui::SetTooltip("Middle / right click: default %.5g. Ctrl+click: exact value.",double(resetValue));
        ImGui::PopID();return changed;};
    auto doubleSlider=[&](const char* label,double* value,double min,double max,const char* format="%.3f",double resetValue=0){
        ImGui::PushID(label);labelAbove(label);bool changed=ImGui::SliderScalar("##value",ImGuiDataType_Double,value,&min,&max,format);
        if(ImGui::IsItemHovered()&&(ImGui::IsMouseClicked(ImGuiMouseButton_Middle)||ImGui::IsMouseClicked(ImGuiMouseButton_Right))){*value=resetValue;changed=true;}
        if(ImGui::IsItemHovered())ImGui::SetTooltip("Middle / right click: default %.5g. Ctrl+click: exact value.",resetValue);
        ImGui::PopID();return changed;};
    auto checkbox=[&](const char* label,bool* value){
        ImGui::PushID(label);bool changed=ImGui::Checkbox("##value",value);ImGui::SameLine();ImGui::TextWrapped("%s",label);ImGui::PopID();return changed;};
    auto combo=[&](const char* label,int* value,const char* choices){ImGui::PushID(label);labelAbove(label);bool changed=ImGui::Combo("##value",value,choices);ImGui::PopID();return changed;};
    ImDrawList* dl = ImGui::GetWindowDrawList();
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
    auto row = [&](const char* label, const char* value, Rgba valueColor)
    {
        ImGui::TextDisabled("%s",label);
        ImGui::PushStyleColor(ImGuiCol_Text,valueColor.Vec4());
        ImGui::TextWrapped("%s",value);ImGui::PopStyleColor();
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
        for (unsigned i = 0; i < (expandActorTracks_?std::min<std::uint32_t>(snap.count, 16):0u); ++i)
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
    case Tool::Camera:
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(Px(4,s),Px(2,s)));
        section("CAMERA");
        auto runtime=camera_runtime::view();
        if(runtime.status.find("rejected")!=std::string::npos||runtime.status.find("Invalid")!=std::string::npos||runtime.status.find("failed")!=std::string::npos)
            ImGui::TextColored(Color::AccentAmber.Vec4(),"%s",runtime.status.c_str());
        auto compactSlider=[&](const char* label,float* value,float min,float max,const char* format="%.3f",ImGuiSliderFlags flags=0,float resetValue=0){
            ImGui::PushID(label);ImGui::SetNextItemWidth(std::max(Px(85,s),ImGui::GetContentRegionAvail().x*.55f));
            bool changed=ImGui::SliderFloat("##value",value,min,max,format,flags);
            if(ImGui::IsItemHovered()&&(ImGui::IsMouseClicked(ImGuiMouseButton_Middle)||ImGui::IsMouseClicked(ImGuiMouseButton_Right))){*value=resetValue;changed=true;}
            if(ImGui::IsItemHovered())ImGui::SetTooltip("Right / middle: reset. Ctrl+click: type value.");
            ImGui::SameLine();ImGui::TextUnformatted(label);ImGui::PopID();return changed;};
        auto compactDouble=[&](const char* label,double* value,double min,double max,const char* format="%.3f",double resetValue=0){
            ImGui::PushID(label);ImGui::SetNextItemWidth(std::max(Px(85,s),ImGui::GetContentRegionAvail().x*.55f));
            bool changed=ImGui::SliderScalar("##value",ImGuiDataType_Double,value,&min,&max,format);
            if(ImGui::IsItemHovered()&&(ImGui::IsMouseClicked(ImGuiMouseButton_Middle)||ImGui::IsMouseClicked(ImGuiMouseButton_Right))){*value=resetValue;changed=true;}
            if(ImGui::IsItemHovered())ImGui::SetTooltip("Right / middle: reset. Ctrl+click: type value.");
            ImGui::SameLine();ImGui::TextUnformatted(label);ImGui::PopID();return changed;};
        bool pathPreview=runtime.dolly_preview;if(checkbox("Preview path",&pathPreview))camera_runtime::preview(pathPreview);
        ImGui::BeginDisabled(!runtime.track_current||runtime.key_count<2);
        if(ImGui::Button("Play path"))PlayDollyPath(f);
        ImGui::EndDisabled();
        if(checkbox("Show camera handles",&showDollyMarkers_))SaveSettings();
        if(checkbox("Visibility hotkey (P)",&enableDollyVisibilityKey_))SaveSettings();
        if(checkbox("Show curves",&showDollyCurves_))SaveSettings();
        bool armed=runtime.enabled;
        if(checkbox("Enable camera control",&armed))camera_runtime::enable(armed);
        float fov=static_cast<float>(runtime.pose.fov_degrees);ImGui::BeginDisabled(!runtime.enabled);
        if(compactSlider("FOV",&fov,1.f,178.f,"%.2f",0,60.f))camera_runtime::fov(fov);
        ImGui::EndDisabled();
        float movement=static_cast<float>(runtime.movement_speed),sensitivity=static_cast<float>(runtime.mouse_sensitivity),smooth=static_cast<float>(runtime.smoothing_seconds);
        bool changed=compactSlider("Move speed",&movement,.01f,1000.f,"%.3f",ImGuiSliderFlags_Logarithmic,3.f);
        changed|=compactSlider("Sensitivity",&sensitivity,.00001f,.05f,"%.5f",ImGuiSliderFlags_Logarithmic,.0025f);
        changed|=compactSlider("Move smooth",&smooth,0.f,2.f,"%.3f");
        float rotationSmooth=static_cast<float>(runtime.rotation_smoothing_seconds);
        changed|=compactSlider("Rotate smooth",&rotationSmooth,0.f,2.f,"%.3f");
        if(changed)camera_runtime::movement(movement,sensitivity,smooth,rotationSmooth);
        double dollySmooth=runtime.dolly_smoothing_seconds;
        if(compactDouble("Path filter",&dollySmooth,0.,2.)){camera_runtime::dolly_smoothing(dollySmooth);cameraSettingsDirty_=true;cameraSettingsChangedAt_=f.now;}
        if(ImGui::CollapsingHeader("Dolly motion",ImGuiTreeNodeFlags_DefaultOpen)){
            auto settings=runtime.track_settings;bool edited=false;
            int timing=int(settings.timing);if(combo("Timing",&timing,"Keyframe time\0Constant world speed\0Time remap\0")){settings.timing=cinematic::TimingMode(timing);edited=true;}
            if(settings.timing==cinematic::TimingMode::ConstantSpeed)edited|=number("Speed (units/s, 0 = fit duration)",&settings.world_units_per_second,.1,1,"%.4f");
            int rotation=int(settings.rotation);if(combo("Aim",&rotation,"Keyframed\0Look at target\0Look along path\0Target + roll\0")){settings.rotation=cinematic::RotationMode(rotation);edited=true;}
            if(settings.rotation==cinematic::RotationMode::LookAt||settings.rotation==cinematic::RotationMode::LookAtRoll){
                int target=int(settings.target);if(combo("Target",&target,"World point\0Recorded player\0Recorded actor (API only)\0Target keys\0")){settings.target=cinematic::TargetType(target);edited=true;}
                if(settings.target==cinematic::TargetType::RecordedActor)ImGui::TextDisabled("Actor target transport not implemented.");
                if(settings.target==cinematic::TargetType::World||settings.target==cinematic::TargetType::Keyframed)for(int i=0;i<3;++i){ImGui::PushID(i);edited|=number("Target XYZ",&settings.target_position[i],.1,1,"%.3f");ImGui::PopID();}
                for(int i=0;i<3;++i){ImGui::PushID(i);edited|=number("Target offset",&settings.target_offset[i],.1,1,"%.3f");ImGui::PopID();}
            }
            if(settings.timing==cinematic::TimingMode::TimeRemap){
                int ease=int(settings.time_remap.mode);if(combo("Distance easing",&ease,"Linear\0Ease in\0Ease out\0Ease in/out\0Smoothstep\0Smootherstep\0Cubic Bezier\0")){settings.time_remap.mode=cinematic::Easing(ease);edited=true;}
                if(settings.time_remap.mode==cinematic::Easing::CubicBezier){edited|=number("Time X1",&settings.time_remap.x1,.01,.1,"%.4f");edited|=number("Distance Y1",&settings.time_remap.y1,.01,.1,"%.4f");edited|=number("Time X2",&settings.time_remap.x2,.01,.1,"%.4f");edited|=number("Distance Y2",&settings.time_remap.y2,.01,.1,"%.4f");}
            }
            if(edited)camera_runtime::track_settings(settings);
            static int capture=0;combo("Capture channel",&capture,"Pose (K)\0Position\0Rotation\0FOV\0Roll\0Focus metadata\0Target point\0");
            const unsigned channels[]={cinematic::PoseChannels,cinematic::Position,cinematic::Rotation,cinematic::Fov,cinematic::Roll,cinematic::Focus,cinematic::Target};
            if(ImGui::Button("Add channel key"))camera_runtime::add_key(channels[capture]);
            if(ImGui::IsItemHovered())ImGui::SetTooltip("Keys use the replay cursor. Different channels may have different key times.");
        }
        if(ImGui::CollapsingHeader("Shake")){
        double shakePosition=runtime.shake_position,shakeRotation=runtime.shake_rotation,shakeFrequency=runtime.shake_frequency,shakeSpeed=runtime.shake_speed,shakeSmooth=runtime.shake_smoothing_seconds;
        bool shakeChanged=compactDouble("Position",&shakePosition,0.,5.);
        shakeChanged|=compactDouble("Rotation",&shakeRotation,0.,30.);
        shakeChanged|=compactDouble("Frequency",&shakeFrequency,0.,30.,"%.3f",1.);
        shakeChanged|=compactDouble("Speed",&shakeSpeed,0.,10.,"%.3f",1.);
        shakeChanged|=compactDouble("Smoothing",&shakeSmooth,0.,2.);
        bool shakeDolly=runtime.shake_dolly;shakeChanged|=checkbox("Dolly shake",&shakeDolly);
        if(shakeChanged){camera_runtime::shake(shakePosition,shakeRotation,shakeFrequency,shakeSpeed,shakeSmooth,shakeDolly);cameraSettingsDirty_=true;cameraSettingsChangedAt_=f.now;}
        ImGui::TextDisabled("Real-time shake, including paused replay.");
        }
        if(ImGui::CollapsingHeader("Close-up visibility")){
        double nearPlane=runtime.near_plane;bool preventFade=runtime.prevent_asset_fade;
        bool closeChanged=compactDouble("Near-Z",&nearPlane,.001,1.,"%.4f",.01);
        closeChanged|=checkbox("Reduce close-camera fading",&preventFade);
        if(closeChanged){camera_runtime::close_up(preventFade,nearPlane);SaveSettings();}
        ImGui::TextWrapped("Reversible asset/model fade settings. Dedicated grass coverage needs verification. Near-Z controls clipping separately.");
        }
        if(ImGui::CollapsingHeader("Bone camera (advanced)")){
        if(ImGui::Button("Use Bone camera")){camera_runtime::mode(3);camera_runtime::enable(true);}
        int boneIndex=runtime.bone_index;double offset[3]={runtime.bone_offset[0],runtime.bone_offset[1],runtime.bone_offset[2]};
        labelAbove("Player bone index (-1 disabled)");bool boneChanged=ImGui::InputInt("##bone-index",&boneIndex);
        for(int i=0;i<3;++i){const char*labels[]={"Bone offset right","Bone offset up","Bone offset forward"};boneChanged|=number(labels[i],&offset[i],.01,.1,"%.3f");}
        if(boneChanged)camera_runtime::bone(boneIndex,{offset[0],offset[1],offset[2]});
        ImGui::Text("Bone source: %s",runtime.bone_available?"AVAILABLE":"UNAVAILABLE");
        }
        if(ImGui::Button("Save path"))camera_runtime::save_path();ImGui::SameLine();if(ImGui::Button("Load path"))camera_runtime::load_path();
        ImGui::Text("Dolly keys: %zu",runtime.keys.size());
        if(ImGui::CollapsingHeader("Camera cuts (session only)")){
        bool cutsEnabled=runtime.cuts_enabled;if(checkbox("Enable cuts",&cutsEnabled))camera_runtime::cuts(cutsEnabled,runtime.cuts);
        static double cutStart=0,cutEnd=5;static int cutMode=1;
        number("Cut start (s)",&cutStart,.1,1);number("Cut end (s)",&cutEnd,.1,1);combo("Cut camera",&cutMode,"Player\0Current Dolly path\0");
        if(ImGui::Button("Add hard cut segment")&&std::isfinite(cutStart)&&std::isfinite(cutEnd)&&cutStart>=0&&cutEnd>cutStart&&cutEnd<double(UINT64_MAX)/1e9){auto cuts=runtime.cuts;std::uint64_t id=1;for(auto&c:cuts)id=std::max(id,c.id+1);cuts.push_back({id,static_cast<std::uint64_t>(cutStart*1e9),static_cast<std::uint64_t>(cutEnd*1e9),cutMode?cinematic::CutMode::Dolly:cinematic::CutMode::Player});camera_runtime::cuts(runtime.cuts_enabled,std::move(cuts));}
        for(auto cut:runtime.cuts){ImGui::PushID(static_cast<int>(cut.id));ImGui::Text("%.3f - %.3f: %s",double(cut.start_ns)/1e9,double(cut.end_ns)/1e9,cut.mode==cinematic::CutMode::Player?"Player":"Dolly");ImGui::NewLine();if(ImGui::Button("Remove cut")){auto cuts=runtime.cuts;std::erase_if(cuts,[&](auto&c){return c.id==cut.id;});camera_runtime::cuts(runtime.cuts_enabled,std::move(cuts));}ImGui::PopID();}
        }
        if(ImGui::CollapsingHeader("Keyframe details")){
        for(auto key:runtime.keys){ImGui::PushID(static_cast<int>(key.id));
            if(key.id==selectedDollyKey_)ImGui::SetNextItemOpen(true,ImGuiCond_Always);
            if(ImGui::TreeNode("edit","Key %llu at %.3fs",static_cast<unsigned long long>(key.id),double(key.time_ns)/1e9)){
                // Edits apply explicitly; live camera continues until Apply is clicked.
                static std::map<std::uint64_t,cinematic::Key> drafts;
                static std::uint64_t draftGeneration=UINT64_MAX;
                if(draftGeneration!=runtime.project_generation){drafts.clear();draftGeneration=runtime.project_generation;}
                auto& draft=drafts.try_emplace(key.id,key).first->second;
                if(ImGui::Button("Select and seek this camera key")){SelectDollyKey(key.id,ImGui::GetIO().KeyCtrl,ImGui::GetIO().KeyShift);Emit(theater_ui::seek,key.time_ns);}
                if(ImGui::Button("Revert draft to saved key"))draft=key;
                double seconds=double(draft.time_ns)/1e9;number("Timestamp (s)",&seconds,.01,1,"%.6f");
                if(std::isfinite(seconds)&&seconds>=0&&seconds<double(UINT64_MAX)/1e9)draft.time_ns=static_cast<std::uint64_t>(seconds*1e9);
                for(int i=0;i<3;++i){const char* names[]={"Position X","Position Y","Position Z"};number(names[i],&draft.state.position[i],.01,1,"%.5f");}
                auto angles=cinematic::viewport::angles(draft.state.orientation);bool rotationChanged=false;
                for(int i=0;i<3;++i){const char*names[]={"Pitch (degrees)","Yaw (degrees)","Roll (degrees)"};angles[i]*=180./3.141592653589793;rotationChanged|=number(names[i],&angles[i],.1,1,"%.3f");}
                if(rotationChanged)if(auto rotation=cinematic::mouse_look({0,0,0,1},angles[1]*3.141592653589793/180,angles[0]*3.141592653589793/180,angles[2]*3.141592653589793/180))draft.state.orientation=*rotation;
                if(ImGui::TreeNode("Raw quaternion (advanced)")){for(int i=0;i<4;++i){const char* names[]={"Quaternion X","Quaternion Y","Quaternion Z","Quaternion W"};number(names[i],&draft.state.orientation[i],.001,.01,"%.6f");}ImGui::TreePop();}
                number("FOV degrees",&draft.state.fov_degrees,.1,1,"%.3f");
                number("Roll (degrees)",&draft.state.roll_degrees,.1,1,"%.3f");
                number("Focus (metadata only)",&draft.state.focus_distance,.1,1,"%.3f");
                if(ImGui::TreeNode("Channels")){
                    const char* labels[]={"Position","Rotation","FOV","Roll","Focus","Target"};
                    for(int i=0;i<6;++i){bool on=(draft.channels&(1u<<i))!=0;if(checkbox(labels[i],&on)){if(on)draft.channels|=1u<<i;else draft.channels&=~(1u<<i);}}
                    for(int i=0;i<3;++i){ImGui::PushID(i);number("Target coordinate",&draft.state.look_at_target[i],.1,1,"%.3f");ImGui::PopID();}
                    ImGui::TreePop();
                }
                int rotationMode=int(draft.rotation_interpolation);if(combo("Rotation interpolation",&rotationMode,"SLERP\0Smooth SQUAD\0Step\0"))draft.rotation_interpolation=cinematic::RotationInterpolation(rotationMode);
                int scalarMode=int(draft.scalar_interpolation);if(combo("FOV / roll curve",&scalarMode,"Linear\0Smooth cubic\0Step\0"))draft.scalar_interpolation=cinematic::ScalarInterpolation(scalarMode);
                auto editEase=[&](const char* label,cinematic::EaseCurve& curve){
                    if(!ImGui::TreeNode(label))return;
                    int mode=int(curve.mode);if(combo("Easing",&mode,"Linear\0Ease in\0Ease out\0Ease in/out\0Smoothstep\0Smootherstep\0Cubic Bezier\0"))curve.mode=cinematic::Easing(mode);
                    if(curve.mode==cinematic::Easing::CubicBezier){number("X1",&curve.x1,.01,.1,"%.4f");number("Y1",&curve.y1,.01,.1,"%.4f");number("X2",&curve.x2,.01,.1,"%.4f");number("Y2",&curve.y2,.01,.1,"%.4f");}
                    ImGui::TreePop();
                };
                editEase("Position timing",draft.position_ease);editEase("Rotation timing",draft.rotation_ease);editEase("FOV / roll timing",draft.scalar_ease);
                int interpolation=static_cast<int>(draft.outgoing);if(combo("Outgoing interpolation",&interpolation,"Linear\0Smooth\0Bezier\0Ease curve\0Catmull-Rom spline\0Step\0"))draft.outgoing=cinematic::Interpolation(interpolation);
                checkbox("Constant position speed",&draft.constant_speed);
                compactDouble("Ease in",&draft.ease_in,0.,1.);compactDouble("Ease out",&draft.ease_out,0.,1.);
                int tangentMode=std::min(2,int(draft.tangent_mode));if(combo("Bezier tangents",&tangentMode,"Auto\0Linear\0Free\0"))draft.tangent_mode=cinematic::TangentMode(tangentMode);
                if(draft.tangent_mode==cinematic::TangentMode::Free)for(int i=0;i<3;++i){ImGui::PushID(i);number("Bezier handle in",&draft.tangent_in[i],.1,1);number("Bezier handle out",&draft.tangent_out[i],.1,1);ImGui::PopID();}
                if(ImGui::Button("Duplicate at cursor"))camera_runtime::duplicate_key(key.id,f.snapshot.time_ns);
                if(ImGui::Button("Apply key edit"))camera_runtime::edit_key(draft);ImGui::NewLine();if(ImGui::Button("Delete this key")){camera_runtime::delete_key(key.id);drafts.erase(key.id);}
                ImGui::TreePop();}ImGui::PopID();}
        }
        if(ImGui::CollapsingHeader("Diagnostics")){
        const auto& evaluation=runtime.evaluation;
        ImGui::Text("Segment %zu | t %.3f | u %.3f",evaluation.segment,evaluation.normalized_time,evaluation.parameter);
        ImGui::Text("Distance %.3f / %.3f",evaluation.distance,evaluation.total_distance);
        ImGui::Text("Target: %s",evaluation.target_resolved?"resolved":"none / waiting");
        ImGui::Text("Native interception: %s | observed: %s",runtime.hook_ready?"READY":"UNAVAILABLE",runtime.observed?"YES":"NO");
        ImGui::Text("Camera writes: %s",runtime.writing?"ACTIVE (EXPERIMENTAL)":"OFF");
        if(ImGui::Button("2-second +0.25 X camera probe"))camera_runtime::probe();
        ImGui::TextWrapped("%s",runtime.status.c_str());
        ImGui::TextWrapped("%s",game_timing::status().c_str());
        bool probe=theater_camera::probe_enabled.load();
        if(checkbox("Enable experimental camera reads",&probe))theater_camera::probe_enabled=probe;
        if(probe){
        const auto& c=f.camera;
        ImGui::Text("CSCamera: %s | mask: 0x%X",c.available?"FOUND":"UNAVAILABLE",c.mask);
        if(c.timestamp_ns){
            // Same QueryInterruptTimePrecise clock as the adapter; GetTickCount64 is a different epoch.
            ULONGLONG ticks=0;QueryInterruptTimePrecise(&ticks);
            const auto now=std::uint64_t(ticks)*100;
            ImGui::Text("Update age: %.1f ms%s",now>=c.timestamp_ns?double(now-c.timestamp_ns)/1e6:0.,
                now<c.timestamp_ns||now-c.timestamp_ns>2000000000ULL?" (STALE)":"");
        }
        for(int i=0;i<4;++i){const auto&slot=c.slots[i];ImGui::PushID(i);
            if(ImGui::TreeNode("slot","Camera slot %d: %s",i+1,slot.valid?"PLAUSIBLE":"UNAVAILABLE / INVALID")){
                if(slot.valid){ImGui::Text("Position: %.3f %.3f %.3f",slot.matrix[12],slot.matrix[13],slot.matrix[14]);
                    ImGui::Text("FOV raw: %.5f (units unverified)",slot.fov);ImGui::Text("Aspect: %.4f | clip: %.4f / %.1f",slot.aspect,slot.near_plane,slot.far_plane);
                    for(int r=0;r<4;++r)ImGui::Text("%.4f %.4f %.4f %.4f",slot.matrix[r*4],slot.matrix[r*4+1],slot.matrix[r*4+2],slot.matrix[r*4+3]);}
                ImGui::TreePop();}ImGui::PopID();
        }
        } // SDK probe
        } // Diagnostics
        ImGui::PopStyleVar();
        break;
    }
    case Tool::Lights:
    {
        section("Sun / time of day");
        auto clock=light_editor::time_view();
        float hour=clock.requested?clock.target:clock.hour;
        ImGui::BeginDisabled(!clock.available||!snap.connected||!snap.player_found);
        if(slider("Day / night",&hour,0.f,23.f+59.f/60.f,"%.2f h",0,12.f))light_editor::time(hour);
        if(ImGui::Button("Restore time",ImVec2(-FLT_MIN,0)))light_editor::restore_time();
        ImGui::EndDisabled();
        if(clock.available)ImGui::TextDisabled("Observed: %02d:%02d",int(clock.hour),int(clock.hour*60)%60);else ImGui::TextDisabled("Clock unavailable");
        ImGui::TextWrapped("Changes native world time and its sun/shadows where supported by the area. World time may be autosaved.");
        ImGui::Separator();
        section("Custom lights");
        auto native=native_lights::view();bool renderLights=native.enabled;
        if(checkbox("Render lights",&renderLights))native_lights::enable(renderLights);
        if(checkbox("Show light handles",&showLightMarkers_)){lightGizmoDragging_=false;SaveSettings();}
        auto camera=camera_runtime::view(false);
        ImGui::BeginDisabled(!camera.observed);
        if(ImGui::Button("Create point light",ImVec2(-FLT_MIN,0))){light_editor::create(light_editor::Type::Point,camera.pose);native_lights::enable(true);viewportLightSelected_=true;gizmoDragging_=false;}
        if(ImGui::Button("Create spot light",ImVec2(-FLT_MIN,0))){light_editor::create(light_editor::Type::Spot,camera.pose);native_lights::enable(true);viewportLightSelected_=true;gizmoDragging_=false;}
        ImGui::EndDisabled();
        ImGui::TextDisabled("Native lights: %u",native.rendered);
        if(!native.status.empty())ImGui::TextWrapped("%s",native.status.c_str());
        auto editor=light_editor::view();
        static bool enabledOnly=false;
        checkbox("Enabled lights only",&enabledOnly);
        const auto enabledCount=std::count_if(editor.lights.begin(),editor.lights.end(),[](const auto& l){return l.enabled;});
        ImGui::TextDisabled("%zu lights | %zu enabled | %u submitted",editor.lights.size(),size_t(enabledCount),native.rendered);
        const float listHeight=ImGui::GetTextLineHeightWithSpacing()*7;
        if(ImGui::BeginTable("##scene_lights",5,ImGuiTableFlags_ScrollY|ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_SizingStretchProp,ImVec2(0,listHeight))){
            ImGui::TableSetupScrollFreeze(0,1);
            ImGui::TableSetupColumn("On",ImGuiTableColumnFlags_WidthFixed,Px(28,s));
            ImGui::TableSetupColumn("Light",ImGuiTableColumnFlags_WidthStretch,2);
            ImGui::TableSetupColumn("Type",ImGuiTableColumnFlags_WidthStretch,1);
            ImGui::TableSetupColumn("State",ImGuiTableColumnFlags_WidthStretch,1);
            ImGui::TableSetupColumn("Anim",ImGuiTableColumnFlags_WidthFixed,Px(34,s));
            ImGui::TableHeadersRow();
            for(const auto& l:editor.lights){
                if(enabledOnly&&!l.enabled)continue;
                ImGui::PushID(std::to_string(l.id).c_str());ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);
                bool enabled=l.enabled;if(ImGui::Checkbox("##enabled",&enabled)){auto edited=l;edited.enabled=enabled;light_editor::edit(edited);}
                ImGui::TableSetColumnIndex(1);
                if(ImGui::Selectable(l.name.c_str(),l.id==editor.selected,ImGuiSelectableFlags_None)){light_editor::select(l.id);viewportLightSelected_=true;gizmoDragging_=lightGizmoDragging_=false;}
                if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\nID: %llu",l.name.c_str(),static_cast<unsigned long long>(l.id));
                ImGui::TableSetColumnIndex(2);ImGui::TextUnformatted(l.type==light_editor::Type::Point?"Point":"Spot");
                const bool submitted=std::find(native.submitted_ids.begin(),native.submitted_ids.end(),l.id)!=native.submitted_ids.end();
                ImGui::TableSetColumnIndex(3);ImGui::TextUnformatted(!l.enabled?"Off":!native.enabled?"Defined":submitted?"Submitted":"Pending");
                ImGui::TableSetColumnIndex(4);
                {   // Clock toggle: enables keyframing of this light's position and rotation.
                    const bool on=light_editor::animated(l.id);const ImVec2 at=ImGui::GetCursorScreenPos();const float r=Px(8,s);
                    if(ImGui::InvisibleButton("##clock",ImVec2(r*2+Px(4,s),r*2+Px(2,s)))){light_editor::set_animated(l.id,!on);light_editor::select(l.id);}
                    auto*dl=ImGui::GetWindowDrawList();const ImVec2 c(at.x+r+Px(2,s),at.y+r+Px(1,s));const ImU32 col=on?Color::AccentAmber.U32():Color::TextSecondary.U32();
                    dl->AddCircle(c,r,col,20,Px(1.6f,s));dl->AddLine(c,ImVec2(c.x,c.y-r*.65f),col,Px(1.6f,s));dl->AddLine(c,ImVec2(c.x+r*.5f,c.y+r*.2f),col,Px(1.6f,s));
                    if(ImGui::IsItemHovered())ImGui::SetTooltip(on?"Animation on: click to turn off":"Click to animate this light with keyframes");
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if(editor.lights.empty())ImGui::TextDisabled("No custom lights. Create one at the camera.");
        {   // Keyframes of the selected light (position and rotation against ReplayTime).
            const auto sel=editor.selected;const bool on=sel&&light_editor::animated(sel);
            ImGui::Separator();
            ImGui::TextUnformatted("Light animation");
            if(!sel)ImGui::TextDisabled("Select a light, then click its clock to animate it.");
            else{
                if(!on)ImGui::TextDisabled("Click the clock in the list to turn animation on for the selected light.");
                const auto now=camera_runtime::replay_time();
                ImGui::BeginDisabled(!on||!now);
                char label[96];snprintf(label,sizeof(label),"Add key at %.2f s",now?double(*now)/1e9:0.0);
                if(ImGui::Button(label,ImVec2(-FLT_MIN,0))&&now)light_editor::add_light_key(sel,*now);
                ImGui::EndDisabled();
                if(!now)ImGui::TextDisabled("Load a replay so the lights have a timeline to follow.");
                const auto keys=light_editor::light_keys(sel);
                ImGui::TextDisabled("%zu keys. Move the light with its handle at a chosen time, then add a key.",keys.size());
                for(const auto&k:keys){
                    ImGui::PushID(int(k.time_ns/1000000));
                    ImGui::Text("%.2f s  (%.1f, %.1f, %.1f)",double(k.time_ns)/1e9,k.transform.position[0],k.transform.position[1],k.transform.position[2]);
                    ImGui::SameLine();if(ImGui::SmallButton("Delete"))light_editor::remove_light_key(sel,k.time_ns);
                    ImGui::PopID();
                }
                if(!keys.empty()&&ImGui::Button("Clear keys",ImVec2(-FLT_MIN,0)))light_editor::clear_light_keys(sel);
            }
            ImGui::Separator();
        }
        ImGui::BeginDisabled(!editor.selected);
        if(ImGui::Button("Duplicate selected",ImVec2(-FLT_MIN,0))&&light_editor::duplicate(editor.selected)){viewportLightSelected_=true;gizmoDragging_=lightGizmoDragging_=false;}
        ImGui::EndDisabled();
        bool shadows=native.shadows;
        ImGui::BeginDisabled(!native.shadow_available);
        if(checkbox("Experimental dynamic shadows",&shadows))native_lights::shadows(shadows);
        ImGui::EndDisabled();
        ImGui::TextWrapped("Custom lights only. Shadows depend on engine quality and shadow resources; may reduce FPS.");
        if(ImGui::CollapsingHeader("Shadow and volumetric quality (all lights)")){
            ImGui::TextWrapped("Changes the game's own graphics quality tables in memory (shadow map size, local light shadows, fog and volumetric light). The game normally applies them when a quality level is applied, so change the quality in the game's graphics menu or load an area to see them. Everything is put back when you switch this off or quit. Higher values cost FPS.");
            bool master=gGfx[0].load()!=0;if(checkbox("Override quality tables",&master)){gGfx[0]=master?1:0;SaveSettings();}
            ImGui::BeginDisabled(!master);
            auto dflt=[&](int slot){return gGfx[slot].load()==kGfxDefault;};
            // Tri-state for on/off values: engine default / off / on.
            auto triState=[&](const char* label,int slot){
                int sel=dflt(slot)?0:(gGfx[slot].load()?2:1);const char* names="Engine default\0Off\0On\0";
                if(combo(label,&sel,names)){gGfx[slot]=sel==0?kGfxDefault:(sel==2?1:0);SaveSettings();}};
            // Number with an "override" checkbox.
            auto numeric=[&](const char* label,int slot,int lo,int hi,int fallback,const char* fmt){
                ImGui::PushID(slot);bool on=!dflt(slot);if(ImGui::Checkbox("##set",&on)){gGfx[slot]=on?fallback:kGfxDefault;SaveSettings();}
                ImGui::SameLine();ImGui::BeginDisabled(!on);int value=dflt(slot)?fallback:gGfx[slot].load();ImGui::SetNextItemWidth(-FLT_MIN);
                if(ImGui::SliderInt(label,&value,lo,hi,fmt)){gGfx[slot]=value;SaveSettings();}ImGui::EndDisabled();ImGui::PopID();};
            ImGui::SeparatorText("Shadows");
            {int sel=0;const int sizes[]={0,512,1024,2048,4096,8192};for(int i=1;i<6;++i)if(gGfx[1].load()==sizes[i])sel=i;
             if(combo("Shadow map size",&sel,"Engine default\0" "512\0" "1024\0" "2048\0" "4096\0" "8192\0")){gGfx[1]=sel==0?kGfxDefault:sizes[sel];SaveSettings();}}
            numeric("Shadow filter level",2,0,8,3,"%d");
            numeric("Shadow blur bias",3,-8,16,0,"%d");
            triState("Local light shadows",4);
            numeric("Local light shadow level cap",5,0,5,5,"%d");
            numeric("Local light distance (percent)",6,25,800,100,"%d%%");
            ImGui::SeparatorText("Fog and volumetric light");
            triState("Fog",7);triState("Fog shadows",8);
            numeric("Fog shadow sample bias",9,-16,16,0,"%d");
            numeric("Fog light distance (percent)",10,25,800,100,"%d%%");
            triState("Fog volume",11);triState("Fog volume shadows",12);triState("Force fog volume shadowing",13);
            numeric("Fog volume resolution level",14,0,8,3,"%d");
            numeric("Fog volume ray-march samples offset",15,-8,8,0,"%d");
            if(ImGui::Button("Reset all to engine defaults",ImVec2(-FLT_MIN,0))){for(int i=1;i<16;++i)gGfx[i]=kGfxDefault;SaveSettings();}
            ImGui::EndDisabled();
        }
        editor=light_editor::view();
        const auto selected=std::find_if(editor.lights.begin(),editor.lights.end(),[&](const auto& l){return l.id==editor.selected;});
        if(selected!=editor.lights.end()){
            auto light=*selected;bool changed=false;
            changed|=checkbox("Enabled",&light.enabled);
            ImGui::BeginDisabled(!camera.observed);
            if(ImGui::Button("Move to current camera",ImVec2(-FLT_MIN,0))){light.transform.position=camera.pose.position;light.transform.orientation=camera.pose.orientation;light.transform.roll_degrees=camera.pose.roll_degrees;changed=true;}
            ImGui::EndDisabled();
            float xyz[3];for(int i=0;i<3;++i)xyz[i]=float(light.transform.position[i]);
            labelAbove("Position XYZ");if(ImGui::DragFloat3("##light_xyz",xyz,.05f)){for(int i=0;i<3;++i)light.transform.position[i]=xyz[i];changed=true;}
            if(light.type==light_editor::Type::Spot){
                auto angles=cinematic::viewport::angles(light.transform.orientation);
                float degrees[3];for(int i=0;i<3;++i)degrees[i]=float(angles[i]*180./3.141592653589793);
                labelAbove("Rotation pitch / yaw / roll");if(ImGui::DragFloat3("##light_rotation",degrees,.2f)){
                    if(auto rotation=cinematic::mouse_look({0,0,0,1},degrees[1]*3.141592653589793/180,degrees[0]*3.141592653589793/180,degrees[2]*3.141592653589793/180)){light.transform.orientation=*rotation;changed=true;}}
                changed|=slider("Cone angle",&light.cone_degrees,1,179,"%.1f deg",0,45);
                ImGui::BeginDisabled();slider("Cone softness",&light.softness,0,1,"%.2f",0,.25f);ImGui::EndDisabled();
            }
            changed|=slider("Radius",&light.radius,.01f,500,"%.2f",ImGuiSliderFlags_Logarithmic,5);
            changed|=slider("Intensity",&light.intensity,0,100,"%.3f",ImGuiSliderFlags_Logarithmic,1);
            labelAbove("Light color RGB");ImGui::SetNextItemWidth(std::min(ImGui::GetContentRegionAvail().x,Px(170,s)));changed|=ImGui::ColorPicker3("##light_color",light.rgb,ImGuiColorEditFlags_PickerHueWheel|ImGuiColorEditFlags_Float|ImGuiColorEditFlags_InputRGB);
            ImGui::BeginDisabled(!native.shadow_available);
            changed|=checkbox("Cast shadows",&light.shadows);
            ImGui::BeginDisabled(!light.shadows);
            int quality=int(light.shadow_level);labelAbove("Required shadow quality level");if(ImGui::SliderInt("##shadow_quality",&quality,1,5)){light.shadow_level=quality;changed=true;}
            if(ImGui::IsItemHovered())ImGui::SetTooltip("Native minimum quality threshold, not shadow-map resolution. A higher requirement can suppress shadows.");
            changed|=slider("Shadow strength",&light.shadow_strength,0,1,"%.2f",0,1);
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            if(ImGui::CollapsingHeader("Advanced light properties")){
                changed|=slider("Source radius",&light.source_radius,0,500,"%.2f",ImGuiSliderFlags_Logarithmic,.1f);
                ImGui::BeginDisabled(!native.shadow_available||!light.shadows);
                labelAbove("Shadow depth bias");changed|=ImGui::SliderInt("##shadow_bias",&light.shadow_bias,-7,7);
                ImGui::EndDisabled();
                ImGui::BeginDisabled();
                changed|=slider("Scattering scale",&light.scattering,0,10,"%.2f",0,1);
                ImGui::EndDisabled();
                labelAbove("Specular color RGB");changed|=ImGui::ColorEdit3("##specular_color",light.specular_rgb,ImGuiColorEditFlags_Float|ImGuiColorEditFlags_InputRGB);
            }
            if(changed)light_editor::edit(light);
            if(ImGui::Button("Delete selected light",ImVec2(-FLT_MIN,0))){light_editor::remove(light.id);lightGizmoDragging_=false;}
        }
        if(ImGui::Button("Save light setup"))light_editor::save();ImGui::SameLine();if(ImGui::Button("Load setup")){light_editor::load();viewportLightSelected_=true;gizmoDragging_=lightGizmoDragging_=false;}
        if(!editor.status.empty())ImGui::TextWrapped("%s",editor.status.c_str());
        ImGui::TextDisabled("Shadow requests are experimental. Cone softness unavailable.");
        break;
    }
    case Tool::Bones:
    {
        section(T(Str::BoneCamera));
        const auto cam=camera_runtime::view(false);const auto names=camera_runtime::bone_names();
        ImGui::TextWrapped("%s",names.empty()?"Bone names are not available yet. Load into the world with your character; the list fills in by itself.":"Pick a bone and the camera attaches to it. It moves and turns like the free camera but follows the bone. F3 cycles Default, Free, Dolly and Bone camera.");
        ImGui::TextDisabled("Bone camera: %s | bone: %s | source: %s",cam.mode==3&&cam.enabled?"ACTIVE":"not active",cam.bone_index>=0&&cam.bone_index<(int)names.size()?names[cam.bone_index].c_str():"none",cam.bone_available?"available":"unavailable");
        auto attach=[&](int index){camera_runtime::bone_attach(index);camera_runtime::mode(3);camera_runtime::enable(true);};
        const float half=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)*.5f;
        if(ImGui::Button("Attach to selected bone",ImVec2(half,0))){if(cam.bone_index>=0||AutoSelectBone()){camera_runtime::mode(3);camera_runtime::enable(true);}}
        ImGui::SameLine();
        if(ImGui::Button("Detach (Default)",ImVec2(-FLT_MIN,0))){camera_runtime::mode(0);camera_runtime::enable(false);}
        if(ImGui::Checkbox("Show bones on the character (click a dot to attach)",&showBoneDots_))SaveSettings();
        ImGui::BeginDisabled(!showBoneDots_);
        if(ImGui::Checkbox("Main bones only (hide fingers, twist and cloth bones)",&boneDotsMajorOnly_))SaveSettings();
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::TextUnformatted("Quick picks");
        const struct{const char*label;const char*keys[3];} quick[]={{"Head",{"Head"}},{"Neck",{"Neck"}},{"Spine",{"Spine2","Spine1","Spine"}},{"Pelvis",{"Pelvis"}},{"Left hand",{"L_Hand"}},{"Right hand",{"R_Hand"}},{"Left weapon",{"L_Weapon","L_Hand"}},{"Right weapon",{"R_Weapon","R_Hand"}},{"Left foot",{"L_Foot"}},{"Right foot",{"R_Foot"}}};
        int column=0;
        for(const auto&q:quick){
            int found=-1;for(const char*key:q.keys){if(!key)break;found=FindBone(names,key);if(found>=0)break;}
            if(column)ImGui::SameLine();
            ImGui::BeginDisabled(found<0);
            if(ImGui::Button(q.label,ImVec2(half,0)))attach(found);
            ImGui::EndDisabled();
            column=1-column;
        }
        ImGui::Separator();
        ImGui::TextUnformatted("Camera position on the bone");
        double offset[3]={cam.bone_offset[0],cam.bone_offset[1],cam.bone_offset[2]};bool offsetChanged=false;
        const char*offsetLabels[]={"Right of bone","Above bone","In front of bone"};
        for(int i=0;i<3;++i)offsetChanged|=number(offsetLabels[i],&offset[i],.01,.1,"%.3f");
        if(offsetChanged&&cam.bone_index>=0)camera_runtime::bone(cam.bone_index,{offset[0],offset[1],offset[2]});
        if(ImGui::Button("Reset camera on bone",ImVec2(-FLT_MIN,0))&&cam.bone_index>=0)camera_runtime::bone(cam.bone_index,{0,0,-1});
        ImGui::Separator();
        static char boneFilter[48]="";
        ImGui::SetNextItemWidth(-FLT_MIN);ImGui::InputTextWithHint("##bone-filter","Filter bones...",boneFilter,sizeof(boneFilter));
        std::string needle=boneFilter;for(auto&c:needle)c=(char)std::tolower((unsigned char)c);
        if(ImGui::BeginChild("##bone-list",ImVec2(0,std::max(160.f,ImGui::GetContentRegionAvail().y)),true)){
            for(int i=0;i<(int)names.size();++i){
                if(!needle.empty()){std::string lower=names[i];for(auto&c:lower)c=(char)std::tolower((unsigned char)c);if(lower.find(needle)==std::string::npos)continue;}
                char row[96];snprintf(row,sizeof(row),"%3d  %s",i,names[i].c_str());
                if(ImGui::Selectable(row,cam.bone_index==i))attach(i);
            }
        }
        ImGui::EndChild();
        break;
    }
    case Tool::Particles:
    {
        section(T(Str::ParticleSpawner));
        if(ImGui::Button("Inspect native VFX"))native_particles::inspect();
        const auto vfx=native_particles::view();
        ImGui::TextWrapped("%s",vfx.status.c_str());
        if(vfx.available)ImGui::Text("Native debug FXR ID: %u | Distance: %.2f",vfx.debug_effect_id,vfx.camera_distance);
        static bool nativeParticleExperiment=false;
        checkbox("Experimental native FXR preview",&nativeParticleExperiment);
        const auto previewStatus=native_particles::preview_view();
        ImGui::TextWrapped("%s",previewStatus.status.c_str());
        if(ImGui::Button("Stop native particle preview"))native_particles::stop_preview();
        ImGui::TextWrapped("Position an emitter, choose a resident FXR, then preview it.");
        auto particles=particle_editor::view(); auto camera=camera_runtime::view(false);
        bool show=showParticleMarkers_; if(checkbox("Show particle emitters",&show)){showParticleMarkers_=show;SaveSettings();}
        std::size_t count=0; const auto* catalog=particle_editor::presets(count);
        static int chosenPreset=1001;
        const char* chosenName="Choose particle"; for(std::size_t i=0;i<count;++i)if(catalog[i].id==chosenPreset)chosenName=catalog[i].name;
        if(ImGui::BeginCombo("Particle category / effect",chosenName)){
            const char* last="";
            for(std::size_t i=0;i<count;++i){
                const char* category="Ambient";
                switch(catalog[i].category){case particle_editor::Category::Environment:category="Environment";break;case particle_editor::Category::Weather:category="Weather";break;case particle_editor::Category::Fire:category="Fire";break;case particle_editor::Category::Smoke:category="Smoke";break;case particle_editor::Category::Magic:category="Magic";break;case particle_editor::Category::Combat:category="Combat";break;case particle_editor::Category::Water:category="Water";break;case particle_editor::Category::Custom:category="Custom";break;default:break;}
                if(std::strcmp(last,category)!=0){if(last[0]){}ImGui::TextDisabled("%s",category);last=category;}
                ImGui::PushID(catalog[i].id);if(ImGui::Selectable(catalog[i].name,catalog[i].id==chosenPreset))chosenPreset=catalog[i].id;ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::BeginDisabled(!camera.observed);
        if(ImGui::Button("Add emitter at camera",ImVec2(-FLT_MIN,0)))particle_editor::create(chosenPreset,camera.pose);
        ImGui::TextDisabled("Catalog entries are editor templates; native FXR mappings are pending.");
        ImGui::EndDisabled();
        if(ImGui::Button("Save particle setup"))particle_editor::save();ImGui::SameLine();if(ImGui::Button("Load setup")){particle_editor::load();viewportParticleSelected_=false;particleGizmoDragging_=false;}
        if(ImGui::Button("Delete all emitters")){particle_editor::clear();particleGizmoDragging_=false;}
        auto selected=std::find_if(particles.emitters.begin(),particles.emitters.end(),[&](const auto&e){return e.id==particles.selected;});
        const char* selectedName=selected==particles.emitters.end()?"Select emitter":selected->name.c_str();
        if(ImGui::BeginCombo("Emitter",selectedName)){for(const auto&e:particles.emitters){ImGui::PushID((int)e.id);if(ImGui::Selectable(e.name.c_str(),e.id==particles.selected)){particle_editor::select(e.id);viewportParticleSelected_=true;}ImGui::PopID();}ImGui::EndCombo();}
        if(selected!=particles.emitters.end()){
            static std::uint32_t loadedFxr=0;
            static ImGuiTextFilter fxrFilter;
            static bool favoritesOnly=false, describedOnly=false;
            fxrFilter.Draw("Search ID / name / category",-FLT_MIN);
            checkbox("Favorites only",&favoritesOnly);
            checkbox("Described effects only",&describedOnly);
            const auto& annotations=particle_catalog::entries();
            std::vector<std::uint32_t> visibleEffects;
            for(auto id:vfx.loaded_effect_ids){
                const auto found=annotations.find(id);
                if(favoritesOnly&&(found==annotations.end()||!found->second.favorite))continue;
                const auto* ref=particle_catalog::reference(id);
                if(describedOnly&&(found==annotations.end()||found->second.name.empty())&&(!ref||(!ref->info[0]&&!ref->behavior[0])))continue;
                if(!fxrFilter.IsActive()||fxrFilter.PassFilter(particle_catalog::search_text(id).c_str()))visibleEffects.push_back(id);
            }
            ImGui::TextDisabled("%zu shown / %zu resident effects",visibleEffects.size(),vfx.loaded_effect_ids.size());
            const auto selectedFxrLabel=loadedFxr?particle_catalog::label(loadedFxr):"Select a resident effect";
            if(ImGui::BeginCombo("Resident game effects",selectedFxrLabel.c_str())){
                ImGuiListClipper clipper;clipper.Begin(static_cast<int>(visibleEffects.size()));
                while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){
                    const auto id=visibleEffects[i];const auto label=particle_catalog::label(id);
                    ImGui::PushID(static_cast<int>(id));if(ImGui::Selectable(label.c_str(),id==loadedFxr))loadedFxr=id;ImGui::PopID();
                }
                if(visibleEffects.empty())ImGui::TextDisabled("No matching resident effects");
                ImGui::EndCombo();
            }
            if(const auto* ref=particle_catalog::reference(loadedFxr)){
                if(ImGui::TreeNode("Reference sheet details")){
                    ImGui::TextWrapped("Bank: %s",ref->bank);
                    if(ref->origin[0])ImGui::TextWrapped("Origin: %s",ref->origin);
                    if(ref->color[0])ImGui::TextWrapped("Color: %s",ref->color);
                    if(ref->behavior[0])ImGui::TextWrapped("Behavior: %s",ref->behavior);
                    if(ref->info[0])ImGui::TextWrapped("Usage / notes: %s",ref->info);
                    if(!ref->info[0]&&!ref->behavior[0])ImGui::TextDisabled("Sheet has no description for this ID.");
                    ImGui::TextWrapped("Resources: %s",ref->resources);
                    if(ref->refs[0])ImGui::TextWrapped("References: %s",ref->refs);
                    ImGui::TextDisabled("Community reference; not verified against every effect in 2.7.0.0.");
                    ImGui::TreePop();
                }
            }
            static std::uint32_t editingFxr=0;
            static char fxrName[256]{},fxrCategory[128]{};
            static bool fxrFavorite=false;
            if(editingFxr!=loadedFxr){
                editingFxr=loadedFxr;fxrName[0]=fxrCategory[0]=0;fxrFavorite=false;
                if(auto it=annotations.find(loadedFxr);it!=annotations.end()){
                    snprintf(fxrName,sizeof(fxrName),"%s",it->second.name.c_str());
                    snprintf(fxrCategory,sizeof(fxrCategory),"%s",it->second.category.c_str());fxrFavorite=it->second.favorite;
                }
            }
            ImGui::BeginDisabled(!loadedFxr);
            ImGui::InputText("My effect name",fxrName,sizeof(fxrName));
            ImGui::InputText("My category",fxrCategory,sizeof(fxrCategory));
            checkbox("Favorite effect",&fxrFavorite);
            if(ImGui::Button("Save effect label"))particle_catalog::save(loadedFxr,{fxrName,fxrCategory,fxrFavorite});
            ImGui::EndDisabled();
            if(!particle_catalog::status().empty())ImGui::TextWrapped("%s",particle_catalog::status().c_str());
            ImGui::TextDisabled("Sheet labels are community descriptions. Your saved names take priority.");
            ImGui::BeginDisabled(!nativeParticleExperiment||!loadedFxr||previewStatus.faulted);
            if(ImGui::Button("Preview selected FXR here (2 seconds)",ImVec2(-FLT_MIN,0)))native_particles::preview(loadedFxr,selected->transform);
            ImGui::EndDisabled();
            ImGui::TextDisabled("Resident effects only. Native scale / intensity are not connected yet.");
            auto e=*selected; bool changed=false; changed|=checkbox("Enabled",&e.enabled);changed|=checkbox("Loop",&e.loop);
            changed|=slider("Duration (s)",&e.duration_seconds,0.f,60.f,"%.2f",ImGuiSliderFlags_Logarithmic,1.f);
            changed|=slider("Repeat delay (s)",&e.repeat_seconds,0.f,60.f,"%.2f",ImGuiSliderFlags_Logarithmic,0.f);
            changed|=slider("Scale",&e.scale,.01f,100.f,"%.2f",ImGuiSliderFlags_Logarithmic,1.f);changed|=slider("Intensity",&e.intensity,0.f,100.f,"%.2f",ImGuiSliderFlags_Logarithmic,1.f);
            float xyz[3];for(int i=0;i<3;++i)xyz[i]=float(e.transform.position[i]);labelAbove("Emitter position XYZ");if(ImGui::DragFloat3("##particle_xyz",xyz,.05f)){for(int i=0;i<3;++i)e.transform.position[i]=xyz[i];changed=true;}
            auto angles=cinematic::viewport::angles(e.transform.orientation);float deg[3];for(int i=0;i<3;++i)deg[i]=float(angles[i]*180./3.141592653589793);labelAbove("Emitter rotation pitch / yaw / roll");if(ImGui::DragFloat3("##particle_rotation",deg,.2f))if(auto q=cinematic::mouse_look({0,0,0,1},deg[1]*3.141592653589793/180,deg[0]*3.141592653589793/180,deg[2]*3.141592653589793/180)){e.transform.orientation=*q;changed=true;}
            if(ImGui::Button("Move emitter to current camera",ImVec2(-FLT_MIN,0))&&camera.observed){e.transform=camera.pose;changed=true;}
            if(changed)particle_editor::edit(e);if(ImGui::Button("Delete selected emitter",ImVec2(-FLT_MIN,0)))particle_editor::remove(e.id);
        }
        if(!particles.status.empty())ImGui::TextWrapped("%s",particles.status.c_str());
        ImGui::TextDisabled("FXR previews use native effects; emitter markers are editor controls.");
        break;
    }
    case Tool::Weather:
    {
        section(T(Str::WeatherEditor));
        auto weather=game_weather::view();std::size_t count=0;const auto* presets=game_weather::presets(count);
        const char* selected="Choose weather";
        for(std::size_t i=0;i<count;++i)if(presets[i].id==weather.selected)selected=presets[i].name;
        labelAbove("Weather preset");
        if(ImGui::BeginCombo("##weather",selected)){
            for(std::size_t i=0;i<count;++i){ImGui::PushID(presets[i].id);
                if(ImGui::Selectable(presets[i].name,presets[i].id==weather.selected))game_weather::select(presets[i].id);
                ImGui::PopID();}
            ImGui::EndCombo();
        }
        ImGui::BeginDisabled(!weather.available);
        if(ImGui::Button("Apply weather",ImVec2(-FLT_MIN,0)))game_weather::enable(true);
        ImGui::EndDisabled();
        if(ImGui::Button("Restore automatic weather",ImVec2(-FLT_MIN,0)))game_weather::enable(false);
        ImGui::TextWrapped("%s",weather.enabled?(weather.applied?"Override active":"Waiting for native transition"):"Automatic weather");
        ImGui::TextDisabled("Native ID: %d   Requested: %d",weather.current,weather.pending);
        ImGui::TextWrapped("%s",weather.diagnostic.c_str());
        ImGui::Separator();
        ImGui::TextWrapped("Rain, snow, fog, wind and regional variants. Effects depend on the loaded area and may blend gradually.");
        ImGui::TextDisabled("Runtime / visual validation required.");
        ImGui::TextWrapped("Overrides turn off on loading, lost connection or game focus loss. Weather is not stored in replays yet.");
        ImGui::Separator();section("Wind response");
        auto wind=game_wind::view();
        bool windChanged=checkbox("Override foliage wind strength",&wind.enabled);
        windChanged|=slider("Strength",&wind.strength,0.f,3.f,"%.2fx",0,1.f);
        if(windChanged){game_wind::configure(wind.enabled,wind.strength);SaveSettings();}
        if(ImGui::Button("Restore native wind",ImVec2(-FLT_MIN,0)))game_wind::configure(false,wind.strength);
        ImGui::TextDisabled("Grass: %u   Wind-enabled assets: %u",wind.grass_rows,wind.asset_rows);
        ImGui::TextWrapped("%s",wind.status==1?"Native response rows updated; visual verification required.":wind.status<0?"Native wind response unavailable; override disabled.":"Native wind response.");
        ImGui::TextWrapped("Direction and cloth forces: unavailable. This changes foliage response, not the world's wind force. Loaded models may cache these values.");
        if(ImGui::TreeNode("Wind diagnostics")){
            if(checkbox("Inspect native force fields (read-only)",&wind.inspect))game_wind::inspect(wind.inspect);
            ImGui::TextDisabled("Registry slots: %u   Readable records: %u",wind.native_slots,wind.observed_records);
            ImGui::TextWrapped("%s",wind.probe_status==1?"Registry observed; force types and cloth consumers still unverified.":wind.probe_status==0?"Inactive / native registry not loaded.":wind.probe_status==-3?"Registry changed during read; snapshot discarded.":"Read guard failed or registry unreadable.");
            ImGui::TextDisabled("One-second read-only sampling; no native force writes.");ImGui::TreePop();
        }
        break;
    }
    case Tool::Look: {
        section("Look");
        const auto gradeView=color_grading::view();auto grade=gradeView.settings;
        bool changed=checkbox("Enable Look effects",&grade.enabled);
        if(ImGui::CollapsingHeader("Color",ImGuiTreeNodeFlags_DefaultOpen)){
            changed|=slider("Exposure (EV)",&grade.exposure_ev,-5.f,5.f,"%+.2f EV",0,0.f);
            changed|=slider("Contrast",&grade.contrast,0.f,2.f,"%.2f",0,1.f);
            changed|=slider("Saturation",&grade.saturation,0.f,2.f,"%.2f",0,1.f);
            changed|=slider("Vibrance",&grade.vibrance,-1.f,1.f,"%+.2f",0,0.f);
        }
        if(ImGui::CollapsingHeader("LUT")){
            static char lutPath[2048]{};
            ImGui::SetNextItemWidth(-FLT_MIN);ImGui::InputTextWithHint("##lut_path","Paste a .cube file path",lutPath,sizeof(lutPath));
            if(ImGui::Button("Load .cube"))color_grading::load_lut(lutPath);
            ImGui::SameLine();if(ImGui::Button("Unload LUT"))color_grading::unload_lut();
            changed|=slider("LUT blend",&grade.lut_blend,0.f,1.f,"%.2f",0,0.f);
            ImGui::TextWrapped("%s",gradeView.lut_status.c_str());
            if(!gradeView.lut_path.empty())ImGui::TextWrapped("%s",gradeView.lut_path.c_str());
            ImGui::TextDisabled("3D Cube; use an SDR / display-referred LUT.");
        }
        if(ImGui::CollapsingHeader("Lens")){
            changed|=slider("Vignette",&grade.vignette,0.f,1.f,"%.2f",0,0.f);
            changed|=slider("Vignette radius",&grade.vignette_radius,0.f,1.f,"%.2f",0,.45f);
            changed|=slider("Vignette softness",&grade.vignette_softness,.01f,1.f,"%.2f",0,.5f);
            changed|=slider("Chromatic aberration",&grade.aberration,0.f,10.f,"%.1f px",0,0.f);
            changed|=slider("Lens distortion",&grade.distortion,-.5f,.5f,"%+.2f",0,0.f);
        }
        if(ImGui::CollapsingHeader("Texture")){
            changed|=slider("Film grain",&grade.grain,0.f,.25f,"%.3f",0,0.f);
            changed|=slider("Grain size",&grade.grain_size,1.f,8.f,"%.1f px",0,1.f);
            changed|=slider("Grain speed",&grade.grain_speed,0.f,4.f,"%.2fx",0,1.f);
            changed|=slider("Sharpening",&grade.sharpen,0.f,2.f,"%.2f",0,0.f);
        }
        if(ImGui::Button("Reset Look values",ImVec2(-FLT_MIN,0))){bool enabled=grade.enabled;grade={};grade.enabled=enabled;changed=true;}
        if(changed){color_grading::configure(grade);SaveSettings();}
        ImGui::TextWrapped("%s",gradeView.status.c_str());
        ImGui::TextDisabled("Right / middle click a slider to reset.");
        if(ImGui::TreeNode("About Look effects")){
            ImGui::TextWrapped("SDR image adjustments after game tone mapping. Theater UI stays unchanged; the native game HUD is part of the graded image.");
            ImGui::TextWrapped("Disable to compare with the original. Values are saved; enable is off at startup. HDR is unsupported in this pass.");
            ImGui::TextWrapped("Grain uses real time, independent of replay speed. Bloom, diffusion, local contrast and native fog controls are not implemented in this pass.");
            ImGui::TreePop();
        }
        if(ImGui::CollapsingHeader("Rendering quality")){
            bool quality=camera_runtime::view(false).high_quality_lods;
            if(checkbox("Full character updates (experimental)",&quality)){camera_runtime::high_quality_lods(quality);SaveSettings();}
            ImGui::TextWrapped("Requests normal loaded-character updates in Free / Dolly. May increase CPU use; does not force mesh LODs.");
        }
        break;
    }
    case Tool::Export:
    {
        section("Video / image export");
        auto st=video_export::status();auto cfg=video_export::settings();bool changed=false;
        ImGui::TextWrapped("Records the game picture as it is shown, with your Look effects and without any Theater UI or export banner. Play your replay at the speed you want in the video.");
        if(st.ffmpeg_found)ImGui::TextDisabled("ffmpeg: %s",st.ffmpeg_path.c_str());
        else ImGui::TextColored(ImVec4(1.f,.65f,.25f,1.f),"ffmpeg.exe not found. Put it next to TheaterMode.dll (or in a folder named ffmpeg there), or set its path below.");
        int container=cfg.container;
        if(combo("Output",&container,"AVI video\0" "PNG image sequence\0" "JPEG image sequence\0")){cfg.container=container;changed=true;}
        if(cfg.container==0){int codec=cfg.codec;
            if(combo("Video codec",&codec,"H.264 (NVIDIA NVENC)\0" "H.264 (CPU, x264)\0" "Motion JPEG\0" "FFV1 (lossless)\0")){cfg.codec=codec;changed=true;}}
        {   // Output size: the game can run at one resolution while the export is scaled to another.
            static const int presets[][2]={{0,0},{1280,720},{1920,1080},{2560,1440},{3840,2160},{-1,-1}};
            int sel=5;for(int i=0;i<5;++i)if(cfg.out_width==presets[i][0]&&cfg.out_height==presets[i][1])sel=i;
            if(combo("Output resolution",&sel,"Same as the game picture\0" "1280 x 720\0" "1920 x 1080\0" "2560 x 1440\0" "3840 x 2160 (4K)\0" "Custom\0")){
                if(sel<5){cfg.out_width=presets[sel][0];cfg.out_height=presets[sel][1];}else if(cfg.out_width<=0){cfg.out_width=1920;cfg.out_height=1080;}
                changed=true;}
            if(sel==5){int wh[2]={cfg.out_width,cfg.out_height};labelAbove("Custom width / height");ImGui::SetNextItemWidth(-FLT_MIN);if(ImGui::InputInt2("##export_size",wh)){cfg.out_width=std::clamp(wh[0],64,7680);cfg.out_height=std::clamp(wh[1],64,4320);changed=true;}}
            if(cfg.out_width>0)ImGui::TextDisabled("The picture is scaled with Lanczos; a different aspect ratio gets black bars.");
        }
        int fps=cfg.fps;labelAbove("Frame rate");if(ImGui::SliderInt("##export_fps",&fps,10,240,"%d fps")){cfg.fps=fps;changed=true;}
        ImGui::TextDisabled("Quick:");ImGui::SameLine();
        for(int preset:{24,30,60,120}){char b[16];snprintf(b,sizeof(b),"%d",preset);ImGui::SameLine();if(ImGui::SmallButton(b)){cfg.fps=preset;changed=true;}}
        const bool lossless=cfg.container==1||(cfg.container==0&&cfg.codec==3);
        ImGui::BeginDisabled(lossless);
        int quality=cfg.quality;labelAbove("Quality");if(ImGui::SliderInt("##export_quality",&quality,1,100,"%d")){cfg.quality=quality;changed=true;}
        ImGui::EndDisabled();
        static char folderBuffer[512]{},ffmpegBuffer[512]{};static bool synced=false;
        if(!synced){snprintf(folderBuffer,sizeof(folderBuffer),"%s",cfg.folder.c_str());snprintf(ffmpegBuffer,sizeof(ffmpegBuffer),"%s",cfg.ffmpeg.c_str());synced=true;}
        labelAbove("Output folder (empty = Videos\\EldenRingTheaterMode)");ImGui::SetNextItemWidth(-FLT_MIN);if(ImGui::InputText("##export_folder",folderBuffer,sizeof(folderBuffer))){cfg.folder=folderBuffer;changed=true;}
        labelAbove("ffmpeg.exe path (empty = automatic)");ImGui::SetNextItemWidth(-FLT_MIN);if(ImGui::InputText("##export_ffmpeg",ffmpegBuffer,sizeof(ffmpegBuffer))){cfg.ffmpeg=ffmpegBuffer;changed=true;}
        if(changed)video_export::configure(cfg);
        ImGui::Separator();
        const std::string key=KeyName(theater_hotkeys::Action::ToggleExport);
        const std::string label=(st.active?"Stop export  [":"Start export  [")+key+"]";
        if(ImGui::Button(label.c_str(),ImVec2(-FLT_MIN,0)))video_export::request_toggle();
        if(!st.text.empty())ImGui::TextWrapped("%s",st.text.c_str());
        if(!st.error.empty())ImGui::TextColored(ImVec4(1.f,.55f,.3f,1.f),"%s",st.error.c_str());
        if(!st.output.empty()){
            ImGui::TextWrapped("Last export: %s",st.output.c_str());
            if(ImGui::Button("Open folder",ImVec2(-FLT_MIN,0))){
                int wn=MultiByteToWideChar(CP_UTF8,0,st.output.c_str(),-1,nullptr,0);std::wstring wide(wn?wn-1:0,L'\0');if(wn)MultiByteToWideChar(CP_UTF8,0,st.output.c_str(),-1,wide.data(),wn);std::filesystem::path p=wide;if(p.has_extension())p=p.parent_path();
                std::wstring command=L"explorer.exe \""+p.wstring()+L"\"";std::vector<wchar_t> mutableCommand(command.begin(),command.end());mutableCommand.push_back(0);
                STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};if(CreateProcessW(nullptr,mutableCommand.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi)){CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}
            }
        }
        ImGui::TextDisabled("Tip: the hotkey is %s and can be changed in Settings > Keybinds. NVENC needs an NVIDIA graphics card.",key.c_str());
        break;
    }
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
        if(ImGui::IsItemHovered()&&(ImGui::IsMouseClicked(ImGuiMouseButton_Middle)||ImGui::IsMouseClicked(ImGuiMouseButton_Right))){ui_.layout.uiScaleUser=1.f;SaveSettings();}
        if (ImGui::IsItemDeactivatedAfterEdit()) SaveSettings();
        section(T(Str::UiSounds));
        {
            bool on = Sound::Enabled();
            if (checkbox(T(Str::UiSoundsOn), &on)) { Sound::SetEnabled(on); if (on) Sound::Play(Sound::Cue::Ok); SaveSettings(); }
            ImGui::BeginDisabled(!on);
            float volume = Sound::Volume() * 100.0f;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##volume", &volume, 0.0f, 100.0f, "%.0f%%")) Sound::SetVolume(volume / 100.0f);
            if(ImGui::IsItemHovered()&&(ImGui::IsMouseClicked(ImGuiMouseButton_Middle)||ImGui::IsMouseClicked(ImGuiMouseButton_Right))){Sound::SetVolume(.6f);SaveSettings();}
            if (ImGui::IsItemDeactivatedAfterEdit()) { Sound::Play(Sound::Cue::Focus); SaveSettings(); }
            ImGui::EndDisabled();
        }
        section(T(Str::ReplayWorld));
        {
            bool flags = false; // unsupported overrides must not appear enabled from old settings
            ImGui::BeginDisabled();checkbox(T(Str::ReplayWorldFlags), &flags);ImGui::EndDisabled();
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
            // Update-LOD override (adapter omission.rs), on by default: bit 4 set means the user switched it off.
            bool fullRate = (gReplayOptions.load() & 4) == 0;
            if (ImGui::Checkbox(T(Str::ReplayFullRate), &fullRate)) { gReplayOptions = fullRate ? (gReplayOptions & ~4u) : (gReplayOptions | 4u); SaveSettings(); }
            PushFont(Font::Meta);
            ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(T(Str::ReplayFullRateNote));
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::PopFont();
            // Freeze every character that is not part of the replay (adapter actors.rs freeze_others), on by default: bit 8 = off.
            bool freezeAi = (gReplayOptions.load() & 8) == 0;
            if (ImGui::Checkbox(T(Str::ReplayFreezeAi), &freezeAi)) { gReplayOptions = freezeAi ? (gReplayOptions & ~8u) : (gReplayOptions | 8u); SaveSettings(); }
            PushFont(Font::Meta);
            ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(T(Str::ReplayFreezeAiNote));
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::PopFont();
            // Recorded gear written into the player (adapter equipment.rs): OFF by default, bit 16 = on.
            bool replayGear = (gReplayOptions.load() & 16) != 0;
            if (ImGui::Checkbox(T(Str::ReplayEquipment), &replayGear)) { gReplayOptions = replayGear ? (gReplayOptions | 16u) : (gReplayOptions & ~16u); SaveSettings(); }
            PushFont(Font::Meta);
            ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(T(Str::ReplayEquipmentNote));
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::PopFont();
            // Summon Torrent with the game's own whistle effect when the recording has him: ON by default, bit 32 = off.
            bool summonHorse = (gReplayOptions.load() & 32) == 0;
            if (ImGui::Checkbox(T(Str::ReplaySummonHorse), &summonHorse)) { gReplayOptions = summonHorse ? (gReplayOptions & ~32u) : (gReplayOptions | 32u); SaveSettings(); }
            PushFont(Font::Meta);
            ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(T(Str::ReplaySummonHorseNote));
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::PopFont();
            if(ImGui::Checkbox("Camera info text in the viewport (lower left)",&showCameraInfo_))SaveSettings();
            // No fade-out near the camera (adapter camera_fade.rs): ON by default, bit 64 = off.
            bool noNearFade = (gReplayOptions.load() & 64) == 0;
            if (ImGui::Checkbox(T(Str::NoNearFade), &noNearFade)) { gReplayOptions = noNearFade ? (gReplayOptions & ~64u) : (gReplayOptions | 64u); SaveSettings(); }
            PushFont(Font::Meta);
            ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(T(Str::NoNearFadeNote));
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::PopFont();
            // Recorded one-shot effects (adapter effects.rs): ON by default, bit 256 = off.
            bool replayFx = (gReplayOptions.load() & 256) == 0;
            if (ImGui::Checkbox(T(Str::ReplayEffects), &replayFx)) { gReplayOptions = replayFx ? (gReplayOptions & ~256u) : (gReplayOptions | 256u); SaveSettings(); }
            PushFont(Font::Meta);
            ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(T(Str::ReplayEffectsNote));
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
        section("Clean view / HUD");
        bool hudHidden=game_hud::requested();ImGui::BeginDisabled(!game_hud::ready());
        if(checkbox("Hide native game HUD",&hudHidden))game_hud::request(hudHidden);
        const auto cleanLabel="Hide game HUD + all Theater UI ["+KeyName(theater_hotkeys::Action::ToggleAllHud)+"]";
        if(ImGui::Button(cleanLabel.c_str()))Emit(kCommandCleanView);
        ImGui::EndDisabled();ImGui::TextDisabled("HUD opacity hook: %s. Clean View key is rebindable.",game_hud::ready()?"READY":"UNAVAILABLE");
        section(T(Str::HotkeysTitle));
        {
            auto& waiting=bindingWaiting_;auto& released=bindingReleased_;auto& bindError=bindingError_;
            ImGui::TextWrapped("Click a key to rebind. Escape cancels. Reset restores that action's default; conflicts are reported without replacing another binding.");
            if(waiting>=0&&f.focused){
                ImGui::TextColored(Color::AccentAmber.Vec4(),"Binding: %s",theater_hotkeys::Label(static_cast<theater_hotkeys::Action>(waiting)));
                if(ImGui::Button("Cancel binding",ImVec2(-FLT_MIN,0))){waiting=-1;theater_hotkeys::rebinding=false;}
                if(waiting>=0){bool any=false;for(int vk=8;vk<256;++vk)if(GetAsyncKeyState(vk)&0x8000)any=true;
                    if(!any)released=true;
                    if(released)for(int vk=8;vk<256;++vk)if(GetAsyncKeyState(vk)&0x8000){
                        if(vk!=VK_ESCAPE)theater_hotkeys::Rebind(static_cast<theater_hotkeys::Action>(waiting),vk,bindError);
                        waiting=-1;theater_hotkeys::rebinding=false;break;}
                }
            }
            ImGui::BeginDisabled(waiting>=0);
            ImGui::TextUnformatted("Search actions");bindingFilter_.Draw("##search-actions",-FLT_MIN);
            if(ImGui::BeginTable("##keybindings",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_SizingStretchProp)){
                ImGui::TableSetupColumn("Action",ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Key",ImGuiTableColumnFlags_WidthFixed,Px(82,s));
                ImGui::TableSetupColumn("Reset",ImGuiTableColumnFlags_WidthFixed,Px(48,s));
                ImGui::TableHeadersRow();
                for(auto&binding:theater_hotkeys::kDefaults){
                    if(theater_hotkeys::retired(binding.action)||!bindingFilter_.PassFilter(binding.label))continue;
                    ImGui::PushID(static_cast<int>(binding.action));ImGui::TableNextRow();ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s",binding.label);
                    unsigned vk=theater_hotkeys::Key(binding.action);char name[120]{};wchar_t wide[60]{};
                    LONG scan=MapVirtualKeyW(vk,MAPVK_VK_TO_VSC)<<16;if(vk>=VK_PRIOR&&vk<=VK_DOWN)scan|=1<<24;
                    GetKeyNameTextW(scan,wide,60);WideCharToMultiByte(CP_UTF8,0,wide,-1,name,sizeof(name),nullptr,nullptr);
                    ImGui::TableNextColumn();
                    if(ImGui::Button(name[0]?name:"Key",ImVec2(-FLT_MIN,0))){waiting=static_cast<int>(binding.action);released=false;bindError.clear();theater_hotkeys::rebinding=true;}
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s: %s (VK %u)",binding.label,name,vk);
                    ImGui::TableNextColumn();
                    if(ImGui::Button("Reset",ImVec2(-FLT_MIN,0))){bindError.clear();theater_hotkeys::Rebind(binding.action,binding.vk,bindError);}
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndDisabled();
            if(!bindError.empty())ImGui::TextColored(Color::AccentAmber.Vec4(),"%s",bindError.c_str());
            ImGui::TextDisabled("Bindings are saved automatically in LOCALAPPDATA/EldenRingTheaterMode/keybinds.ini.");
        }
        PushFont(Font::Meta);
        ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
        ImGui::TextUnformatted(T(Str::HotkeysBody));
        ImGui::PopStyleColor();
        ImGui::PopFont();
        break;
    }
    }

    ImGui::PopTextWrapPos();
    ImGui::EndChild();ImGui::PopID();
    if(showEventLog_){
        section(T(Str::EventLog));ImGui::SameLine();
        if(ImGui::SmallButton("Hide##event-log")){showEventLog_=false;SaveSettings();}
        const float remaining=std::max(1.f,ImGui::GetContentRegionAvail().y);
        DrawEventLog(remaining);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void Overlay::DrawEventLog(float height)
{
    const float s = ui_.rects.uiScale;
    const float controlsStart=ImGui::GetCursorPosY();
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
    if (ImGui::GetTime() < copiedUntil_) { ImGui::TextColored(Color::AccentGreen.Vec4(), "%s", T(Str::Copied)); }
    height = std::max(1.f, height - (ImGui::GetCursorPosY()-controlsStart));

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
    if(clearDollyDialog_){ImGui::OpenPopup("Delete all dolly keyframes?");clearDollyDialog_=false;}
    if(ImGui::BeginPopupModal("Delete all dolly keyframes?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextWrapped("Delete all %zu dolly keyframes? This cannot be undone.",camera_runtime::view().keys.size());
        ImGui::TextWrapped("The original gameplay recording will not be changed.");
        if(ImGui::Button("Cancel")||ImGui::IsKeyPressed(ImGuiKey_Escape,false))ImGui::CloseCurrentPopup();
        ImGui::SameLine();
        if(ImGui::Button("Delete all")){camera_runtime::clear_keys();ImGui::CloseCurrentPopup();}
        ImGui::EndPopup();
    }
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
        ImVec2(Px(820, s), toolbar + Px(Layout.RulerHeight + Layout.NavigatorHeight + (compactTracks_?20.f:Metric::TrackRowHeight) * 2, s) + ImGui::GetFrameHeight()+(showDollyCurves_?Px(100,s):0.f)), &showTimeline_);
    if (!visible) { ImGui::End(); ImGui::PopStyleColor(); ImGui::PopStyleVar(); return; }
    // Expanding the sequencer for curves must not put its bottom off-screen.
    if(!ImGui::IsWindowDocked()){
        const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
        if(pos.y+size.y>ImGui::GetIO().DisplaySize.y)ImGui::SetWindowPos(ImVec2(pos.x,std::max(menuH_,ImGui::GetIO().DisplaySize.y-size.y)));
    }
    if(f.game_texture&&!ImGui::IsWindowDocked()){
        const auto p=ImGui::GetWindowPos();const float minimum=menuH_+Px(150,s);
        if(p.y<minimum)ImGui::SetWindowPos(ImVec2(p.x,minimum));
        if(ImGui::GetWindowPos().y+ImGui::GetWindowHeight()>ImGui::GetIO().DisplaySize.y)ImGui::SetWindowSize(ImVec2(ImGui::GetWindowWidth(),ImGui::GetIO().DisplaySize.y-ImGui::GetWindowPos().y));
    }
    sequencerTop_=ImGui::GetWindowPos().y;sequencerRight_=ImGui::GetWindowPos().x+ImGui::GetWindowWidth();
    if(ImGui::BeginPopupContextWindow("##sequencer-layout",ImGuiPopupFlags_MouseButtonRight)){
        if(ImGui::MenuItem("Compact tracks",nullptr,&compactTracks_))SaveSettings();
        if(ImGui::MenuItem("Expand actor tracks",nullptr,&expandActorTracks_))SaveSettings();
        if(ImGui::MenuItem("Dolly curve editor",nullptr,&showDollyCurves_))SaveSettings();
        ImGui::EndPopup();
    }
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight());
    ImGui::BeginDisabled(selectedDollyKeys_.empty());
    if(ImGui::Button("Remove selected camera keys [Del]"))DeleteSelectedDollyKeys();
    ImGui::EndDisabled();ImGui::SameLine();ImGui::TextDisabled("%zu selected | Ctrl: toggle | Shift: range",selectedDollyKeys_.size());
    ImGui::SameLine();if(ImGui::SmallButton("Undo [Alt+Z]"))camera_runtime::undo();ImGui::SameLine();if(ImGui::SmallButton("Redo [Alt+Shift+Z]"))camera_runtime::redo();
    const float selectionBar=ImGui::GetCursorScreenPos().y-min.y;
    DrawToolbar(f, toolbar);
    const float space=std::max(0.f,max.y-min.y-toolbar-selectionBar),curveHeight=showDollyCurves_?std::min(std::max(0.f,space-Px(80,s)),space*curveFraction_):0.f;
    const float split=max.y-curveHeight;
    DrawTimeline(f, ImVec2(min.x, min.y + toolbar+selectionBar), ImVec2(max.x,split));
    if(curveHeight>Px(50,s)){
        ImGui::SetCursorScreenPos(ImVec2(min.x,split));ImGui::InvisibleButton("##curve-splitter",ImVec2(max.x-min.x,Px(5,s)));
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(min.x,split),ImVec2(max.x,split+Px(3,s)),Color::BorderStrong.U32());
        if(ImGui::IsItemHovered()||ImGui::IsItemActive())ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if(ImGui::IsItemActive()){ImGui::MarkItemEdited(ImGui::GetItemID());curveFraction_=std::clamp((max.y-ImGui::GetIO().MousePos.y)/std::max(1.f,space),.15f,.85f);}
        if(ImGui::IsItemDeactivatedAfterEdit())SaveSettings();
        DrawDollyCurves(f,ImVec2(min.x,split+Px(6,s)),max);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// Editor uses the same evaluator as the game camera. Cache spline/arc data
// between edits; sample only a bounded number of graph points per UI frame.
void Overlay::DrawDollyCurves(const OverlayFrame& f,ImVec2 min,ImVec2 max)
{
    auto&io=ImGui::GetIO();
    const float s=ui_.rects.uiScale;auto camera=camera_runtime::view();
    ImGui::SetCursorScreenPos(ImVec2(min.x+Px(8,s),min.y+Px(4,s)));
    ImGui::TextUnformatted("Dolly curves");ImGui::SameLine();ImGui::SetNextItemWidth(Px(65,s));
    const char* channels[]={"X","Y","Z","FOV","Roll","Focus"};ImGui::Combo("##dolly-channel",&curveChannel_,channels,6);
    ImGui::SameLine();if(ImGui::SmallButton("Fit")){curveZoom_=1;curvePan_=0;}
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Wheel: value zoom. Ctrl+wheel: time zoom. Shift+wheel: value pan.");
    ImGui::SameLine();if(ImGui::SmallButton("Select all")){
        selectedDollyKeys_.clear();for(const auto&key:camera.keys)selectedDollyKeys_.insert(key.id);
        selectedDollyKey_=camera.keys.empty()?0:camera.keys.front().id;dollySelectionAnchor_=selectedDollyKey_;
    }
    // Apply to an explicit scope, atomically and with a single undo entry.
    ImGui::SetCursorScreenPos(ImVec2(min.x+Px(8,s),ImGui::GetCursorScreenPos().y));
    ImGui::SetNextItemWidth(Px(100,s));const char* scopes[]={"Selected","All keys"};
    ImGui::Combo("##curve-scope",&curveEditScope_,scopes,2);
    ImGui::SameLine();ImGui::SetNextItemWidth(Px(95,s));
    const char* modes[]={"Linear","Smooth","Bezier","Curve","Spline","Step"};
    ImGui::Combo("##curve-interpolation",&curveInterpolation_,modes,6);
    ImGui::SameLine();ImGui::BeginDisabled(!camera.track_current||camera.keys.empty()||(!curveEditScope_&&selectedDollyKeys_.empty()));
    if(ImGui::SmallButton("Apply")){
        std::vector<std::uint64_t> ids;
        for(const auto&key:camera.keys)if(curveEditScope_||selectedDollyKeys_.contains(key.id))ids.push_back(key.id);
        camera_runtime::end_edit();curveDragging_=gizmoDragging_=curveBoxSelecting_=false;
        camera_runtime::set_interpolation(ids,cinematic::Interpolation(curveInterpolation_));
        camera=camera_runtime::view();
    }
    ImGui::EndDisabled();ImGui::SameLine();ImGui::TextDisabled("%zu selected",selectedDollyKeys_.size());
    if(!camera.track_current||camera.keys.empty()){curveDragging_=false;ImGui::TextDisabled("Load a replay and capture Dolly keys to edit its spline.");return;}
    if(curveGeneration_!=camera.project_generation){curveTrack_.replace(camera.keys,camera.track_settings);curveGeneration_=camera.project_generation;}
    const ImVec2 a(min.x+Px(8,s),ImGui::GetCursorScreenPos().y+Px(3,s)),b(max.x-Px(8,s),max.y-Px(4,s));
    if(b.y-a.y<Px(20,s)||b.x<=a.x)return;
    // Match timeline zoom/pan exactly, including its left track tree width.
    const float lane=min.x+Px(ui_.layout.trackTreeWidth,s);
    auto timeAt=[&](float x){return std::clamp(ui_.timeline.viewStart+(x-lane)/ui_.timeline.pixelsPerSecond,0.,f.snapshot.duration_ns/1e9);};
    auto xAt=[&](std::uint64_t t){return lane+float((t/1e9-ui_.timeline.viewStart)*ui_.timeline.pixelsPerSecond);};
    const unsigned curveMask=curveChannel_<3?cinematic::Position:curveChannel_==3?cinematic::Fov:curveChannel_==4?cinematic::Roll:cinematic::Focus;
    auto value=[&](const cinematic::State& pose){return curveChannel_<3?pose.position[curveChannel_]:curveChannel_==3?pose.fov_degrees:curveChannel_==4?pose.roll_degrees:pose.focus_distance;};
    auto&curveIo=ImGui::GetIO();
    if(!curveDragging_&&!curveBoxSelecting_&&ImGui::IsWindowHovered()&&curveIo.MousePos.x>=a.x&&curveIo.MousePos.x<=b.x&&curveIo.MousePos.y>=a.y&&curveIo.MousePos.y<=b.y&&curveIo.MouseWheel){
        if(curveIo.KeyCtrl){const auto anchor=timeAt(curveIo.MousePos.x);ui_.timeline.pixelsPerSecond=std::clamp(ui_.timeline.pixelsPerSecond*std::pow(1.2,curveIo.MouseWheel),.1,2000.*s);ui_.timeline.viewStart=anchor-(curveIo.MousePos.x-lane)/ui_.timeline.pixelsPerSecond;}
        else if(curveIo.KeyShift)curvePan_+=curveIo.MouseWheel*(curveMax_-curveMin_)*.1;
        else curveZoom_=std::clamp(curveZoom_*std::pow(1.2,curveIo.MouseWheel),.01,1000.);
    }
    const double t0=timeAt(a.x),t1=timeAt(b.x);
    if(!curveDragging_&&!curveBoxSelecting_){
        double lo=1e300,hi=-1e300;
        for(int n=0;n<=192;++n)if(auto pose=cinematic::dolly_evaluate(curveTrack_,std::uint64_t((t0+(t1-t0)*n/192)*1e9),camera.dolly_smoothing_seconds)){lo=std::min(lo,value(*pose));hi=std::max(hi,value(*pose));}
        for(auto&key:camera.keys)if((key.channels&curveMask)&&xAt(key.time_ns)>=a.x&&xAt(key.time_ns)<=b.x){lo=std::min(lo,value(key.state));hi=std::max(hi,value(key.state));}
        const double padding=std::max(.1,(hi-lo)*.15),center=(lo+hi)*.5+curvePan_,half=((hi-lo)*.5+padding)/curveZoom_;curveMin_=float(center-half);curveMax_=float(center+half);
    }
    auto yAt=[&](double v){return b.y-float((v-curveMin_)/(curveMax_-curveMin_))*(b.y-a.y);};
    ImGui::SetCursorScreenPos(a);ImGui::InvisibleButton("##dolly-curve-plot",ImVec2(b.x-a.x,b.y-a.y));
    auto*draw=ImGui::GetWindowDrawList();draw->PushClipRect(a,b,true);
    draw->AddRectFilled(a,b,Color::ChildBg.U32());
    for(int n=1;n<4;++n){float y=a.y+(b.y-a.y)*n/4;draw->AddLine(ImVec2(a.x,y),ImVec2(b.x,y),Color::BorderSubtle.U32());}
    ImVec2 last{};bool has=false;
    for(int n=0;n<=192;++n){auto t=std::uint64_t((t0+(t1-t0)*n/192)*1e9);if(auto pose=cinematic::dolly_evaluate(curveTrack_,t,camera.dolly_smoothing_seconds)){ImVec2 point(xAt(t),yAt(value(*pose)));if(has)draw->AddLine(last,point,Color::AccentBlue.U32(),Px(2,s));last=point;has=true;}}
    const float playhead=xAt(f.snapshot.time_ns);draw->AddLine(ImVec2(playhead,a.y),ImVec2(playhead,b.y),Color::AccentAmber.U32());
    bool pointHit=false;
    for(auto&key:camera.keys){if(!(key.channels&curveMask))continue;ImVec2 point(xAt(key.time_ns),yAt(value(key.state)));draw->AddCircleFilled(point,Px(5,s),(selectedDollyKeys_.contains(key.id)?Color::AccentAmber:Color::TextPrimary).U32());
        if(!pointHit&&!curveDragging_&&!curveBoxSelecting_&&ImGui::IsItemHovered()&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&std::hypot(ImGui::GetIO().MousePos.x-point.x,ImGui::GetIO().MousePos.y-point.y)<Px(9,s)){
            pointHit=true;SelectDollyKey(key.id,io.KeyCtrl,io.KeyShift);curveStart_=key;curveDragging_=!io.KeyCtrl&&!io.KeyShift;if(curveDragging_)camera_runtime::begin_edit();gizmoDragging_=false;gizmoLastCommit_=f.now;}
    }
    if(f.focused&&!pointHit&&!curveDragging_&&!curveBoxSelecting_&&ImGui::IsItemHovered()&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
        curveBoxSelecting_=true;curveBoxStart_=io.MousePos;curveBoxBase_=(io.KeyCtrl||io.KeyShift)?selectedDollyKeys_:std::unordered_set<std::uint64_t>{};
    }
    if(curveBoxSelecting_){
        ImVec2 mouse(std::clamp(io.MousePos.x,a.x,b.x),std::clamp(io.MousePos.y,a.y,b.y));
        ImVec2 lo(std::min(curveBoxStart_.x,mouse.x),std::min(curveBoxStart_.y,mouse.y)),hi(std::max(curveBoxStart_.x,mouse.x),std::max(curveBoxStart_.y,mouse.y));
        selectedDollyKeys_=curveBoxBase_;
        for(const auto&key:camera.keys){if(!(key.channels&curveMask))continue;auto x=xAt(key.time_ns),y=yAt(value(key.state));if(x>=lo.x&&x<=hi.x&&y>=lo.y&&y<=hi.y)selectedDollyKeys_.insert(key.id);}
        selectedDollyKey_=selectedDollyKeys_.empty()?0:*selectedDollyKeys_.begin();
        draw->AddRectFilled(lo,hi,Color::AccentBlue.Alpha(35).U32());draw->AddRect(lo,hi,Color::AccentBlue.U32());
        if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){selectedDollyKeys_=curveBoxBase_;curveBoxSelecting_=false;}
        if(!io.MouseDown[0]){curveBoxSelecting_=false;dollySelectionAnchor_=selectedDollyKey_;}
    }
    char bounds[80];snprintf(bounds,sizeof(bounds),"%.3f / %.3f",curveMin_,curveMax_);draw->AddText(ImVec2(a.x+Px(4,s),a.y),Color::TextMuted.U32(),bounds);
    draw->PopClipRect();
    if(curveDragging_){auto draft=curveStart_;auto&io=ImGui::GetIO();
        if(io.KeyCtrl)draft.time_ns=std::uint64_t(timeAt(io.MousePos.x)*1e9);
        else {double v=curveMin_+(b.y-io.MousePos.y)/(b.y-a.y)*(curveMax_-curveMin_);if(curveChannel_<3)draft.state.position[curveChannel_]=v;else if(curveChannel_==3)draft.state.fov_degrees=std::clamp(v,1.,178.);else if(curveChannel_==4)draft.state.roll_degrees=v;else draft.state.focus_distance=std::max(0.,v);}
        if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){camera_runtime::edit_key(curveStart_);curveDragging_=false;}
        else if(f.now-gizmoLastCommit_>=1./30||!io.MouseDown[0]){camera_runtime::edit_key(draft);gizmoLastCommit_=f.now;}
        if(!io.MouseDown[0])curveDragging_=false;
    }
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
    if (sliderHovered) ImGui::SetTooltip("Timescale 0.001x to 10x, continuous. Shift: fine; Ctrl: ultra-fine.\nRight/middle/double click resets rate only. World speed follows the same rate during active replay.");
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
    if (timescaleInputInvalid_ && inputHovered) ImGui::SetTooltip("Enter a finite positive value, optionally ending in x. Range: 0.001x to 10x.");
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
    const float rowH = Px(compactTracks_?20.f:Metric::TrackRowHeight, s);
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
    if(!compactTracks_)rows.push_back({ T(Str::TrackActors), Color::AccentAmber, 0, false, 0, true });
    for (unsigned i = 0; i < std::min<std::uint32_t>(snap.count, 16); ++i)
    {
        char name[96];
        snprintf(name, sizeof(name), "Actor %llu", (unsigned long long)snap.actors[i].id);
        rows.push_back({ name, Color::AccentAmber, 1, false, snap.actors[i].id, false });
    }
    if(!compactTracks_)rows.push_back({ T(Str::TrackBookmarks), Color::EventNeutral, 0, false, 0, false });
    const auto cameraView=camera_runtime::view();
    const auto cameraKeys=cameraView.track_current?cameraView.keys:std::vector<cinematic::Key>{};

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
    ImGui::SetCursorScreenPos(ImVec2(min.x+tree-Px(24,s),min.y));
    if(ImGui::InvisibleButton("##compact-tracks",ImVec2(Px(24,s),ruler))){compactTracks_=!compactTracks_;SaveSettings();}
    dl->AddText(ImVec2(min.x+tree-Px(19,s),min.y+Px(2,s)),Color::AccentBlue.U32(),compactTracks_?"+":"-");
    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s tracks. Right-click sequencer for actor/curve visibility.",compactTracks_?"Expand":"Minimize");

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
        if(i==1&&!cameraKeys.empty()){
            dl->PushClipRect(ImVec2(laneMin.x,y0),ImVec2(laneMax.x,y1),true);
            if(!io.MouseDown[0])cameraMarkerClick_=false;
            for(const auto&key:cameraKeys){float x=static_cast<float>(tv.TimeToX(double(key.time_ns)/1e9,laneMin.x));float y=(y0+y1)*.5f,r=Px(5,s);
                const auto color=(selectedDollyKeys_.contains(key.id)?Color::AccentAmber:Color::AccentBlue).U32();
                dl->AddQuadFilled(ImVec2(x,y-r),ImVec2(x+r,y),ImVec2(x,y+r),ImVec2(x-r,y),color);
                if(ImGui::IsWindowHovered()&&io.MouseClicked[0]&&x>=laneMin.x&&x<=laneMax.x&&std::hypot(io.MousePos.x-x,io.MousePos.y-y)<Px(10,s)){
                    SelectDollyKey(key.id,io.KeyCtrl,io.KeyShift);ui_.activeTool=Tool::Camera;ui_.layout.panelOpen=true;cameraMarkerClick_=true;if(!io.KeyCtrl&&!io.KeyShift)Emit(theater_ui::seek,key.time_ns);}
            }
            dl->PopClipRect();
        }
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
    if (canSeek && ImGui::IsItemActive()&&!cameraMarkerClick_)
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

extern "C" void tm_gfx_quality(int*out){if(!out)return;for(int i=0;i<16;++i)out[i]=TheaterUI::gGfx[i].load();}

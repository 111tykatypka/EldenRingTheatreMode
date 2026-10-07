#pragma once
#include "TheaterSounds.h"
#include <atomic>
#include <cstdint>
// In-game Theater overlay, v3 Phase 1: theme, layout solver, tool rail, side
// panel with event log, sequencer with transport, F4 hide/show and the REC pill.
//
// The overlay only reads a copied theater_ui::Snapshot and emits
// theater_ui::Command values through EmitFn. It never touches game memory.
#include <cstdint>
#include <deque>
#include <string>
#include "imgui.h"
#include "TheaterUiProtocol.h"
#include "TheaterLayout.h"
#include "TheaterStrings.h"
#include "CameraTelemetry.h"
#include "CinematicCamera.h"
#include "TheaterHotkeys.h"

namespace TheaterUI
{
    // Replay options set in the Settings panel and read by the game side (tm_overlay_bone_link).
    // Bit 0: restore world event flags (doors, fog walls, bosses) while a replay plays.
    inline std::atomic<std::uint32_t> gReplayOptions{0};
    using EmitFn = void (*)(void* user, std::uint32_t command, std::uint64_t value, const char* text);
    // Not a pipe command: asks the backend to toggle F4 visibility (rail and toolbar buttons).
    inline constexpr std::uint32_t kCommandToggleUi = 0xFFFFFFFFu;
    // Not a pipe command: set visibility (value: 0 shown, 1 hidden), used by the name box.
    inline constexpr std::uint32_t kCommandSetVisibility = 0xFFFFFFFEu;

    struct OverlayFrame
    {
        theater_ui::Snapshot snapshot;   // copy taken under the IPC lock
        bool        hostLinked = false;  // pipe round trip succeeded at least once since the last drop
        std::vector<std::string> events; // new game-side event log lines since the last frame
        double      now = 0.0;           // seconds, monotonic
        theater_camera::Telemetry camera; // read-only candidates, copied from game task
        UiVisibility visibility = UiVisibility::Hidden;
        double      hiddenAt = -100.0;   // when F4 last hid the UI, for the fading hint
    };

    // A log line is either a translated message (id) or host text copied verbatim (id == Str::Count).
    struct LogLine { char clock[12]; Theme::Tone tone; Str id; std::string text; }; // clock = local HH:MM:SS

    class Overlay
    {
    public:
        void Init(ImGuiIO& io);
        // Draws one frame. Returns the rects used, so the backend can place the
        // native ghost HUD inside the game area.
        const LayoutRects& Draw(const OverlayFrame& frame, EmitFn emit, void* user);
        static bool IsRecording(const theater_ui::Snapshot& s) { return s.recording_state != theater_ui::record_idle; }

        // Exposed for tests.
        UIState& State() { return ui_; }
        // Render-thread only; window thread queues actions instead of editing UI state.
        void CameraHotkey(theater_hotkeys::Action action);
        Lang     language = Lang::English;

    private:
        void Observe(const OverlayFrame& f);
        void DrawRail(const OverlayFrame& f);
        void DrawPanel(const OverlayFrame& f);
        void DrawSequencer(const OverlayFrame& f);
        void DrawToolbar(const OverlayFrame& f, float height);
        void DrawTimeline(const OverlayFrame& f, ImVec2 min, ImVec2 max);
        void DrawEventLog(float height);
        void DrawRecordingPill(const OverlayFrame& f);
        void DrawHiddenHint(const OverlayFrame& f);
        void DrawCursor();
        void DrawMenuBar();
        void DrawDialogs(const OverlayFrame& f);
        void DrawLibrary(const OverlayFrame& f);
        void OpenNameDialog(const OverlayFrame& f, bool restoreHidden);
        bool BeginPanel(const char* id, const char* title, ImVec2 defMin, ImVec2 defMax, ImVec2 minSize, bool* open);
        void Emit(std::uint32_t command, std::uint64_t value = 0, const char* text = nullptr);
        void PushFont(Theme::Font role, float extraScale = 1.0f);
        const char* T(Str s) const { return Tr(language, s); }
        void LoadSettings();
        void SaveSettings() const;

        UIState ui_;
        unsigned cameraSelection_ = 0; // selection, not native ownership
        bool clearDollyDialog_ = false;
        ImFont* iconFont_ = nullptr;
        float   appliedScale_ = 0.0f;
        EmitFn  emit_ = nullptr;
        void*   emitUser_ = nullptr;

        // Event log, newest at the back.
        std::deque<LogLine> log_;
        std::string lastDiagnostic_;
        bool  eventError_ = false;            // an event since start mentioned an error (Debug badge)
        // UI sounds: one cue per action; hover/click fallback for controls without their own cue.
        void  Cue(Sound::Cue cue);
        void  UiSoundsAfterFrame();
        bool  cuedThisFrame_ = false;
        unsigned lastHoverId_ = 0;
        double hoverSoundAt_ = 0.0, messageSoundAt_ = -10.0;
        std::uint32_t lastRecording_ = theater_ui::record_idle;
        int  lastLinked_ = -1, lastConnected_ = -1, lastPlayer_ = -1, lastLoaded_ = -1;

        // Panels and layout (Layout menu).
        bool  showTools_ = true, showTimeline_ = true, resetLayout_ = false;
        bool  savedTools_ = true, savedTimeline_ = true, savedPanel_ = true;
        float menuH_ = 0.0f;

        // "Name this replay" box (F5 or a Record button), and Replay Library dialogs.
        bool  nameDialog_ = false, nameFocus_ = false, restoreHidden_ = false;
        char  nameBuf_[128] = {};
        std::uint32_t lastNameRequest_ = 0; bool nameRequestSeen_ = false;
        int   selectedReplay_ = -1;           // library index of the selected row
        int   pendingDialog_ = 0;             // 1 load, 2 rename, 3 delete (opened next frame)
        char  renameBuf_[128] = {};
        std::uint32_t lastMessageId_ = 0; double messageUntil_ = 0.0;
        std::string message_; bool messageError_ = false;
        double copiedUntil_ = 0.0;           // "Copied" feedback in the event log

        // Timeline interaction.
        std::uint64_t lastDuration_ = 0;
        std::string lastReplayPath_;
        double pendingTimescale_ = 0; // UI request awaiting host acknowledgement, not a playback clock
        double timescaleSentAt_ = 0;
        char timescaleInput_[32]{};
        bool timescaleInputInvalid_ = false;
        bool   scrubbing_ = false;
        double lastScrubSent_ = 0.0, scrubTime_ = 0.0;
        bool   draggingNavigator_ = false;
        float  navigatorGrab_ = 0.0f;
    };

    // Icon code points in Segoe MDL2 Assets.
    namespace Glyph
    {
        inline constexpr std::uint16_t Scene = 0xE81E, Camera = 0xE714, Look = 0xE706, Replays = 0xE8B2,
            Export = 0xE898, Debug = 0xEBE8, Settings = 0xE713, HideUi = 0xE73F, ShowUi = 0xE740,
            Play = 0xE768, Pause = 0xE769, Stop = 0xE71A, Restart = 0xE892, StepBack = 0xE76B, StepFwd = 0xE76C,
            Record = 0xE7C8, Person = 0xE77B, Flag = 0xE7C1, Globe = 0xE774, Warning = 0xE7BA;
    }
    // UTF-8 for a BMP code point; the buffer must hold 4 bytes.
    const char* IconUtf8(std::uint16_t codepoint, char (&out)[4]);
}

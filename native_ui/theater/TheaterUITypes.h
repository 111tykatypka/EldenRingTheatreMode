#pragma once
// ============================================================================
// TheaterUITypes.h
// Elden Ring Theater Mode - view models, UI-only state and the command model.
//
// Data flow (one direction per frame):
//
//   Game runtime (ReplaySession, CameraManager, ActorManager, NativeReplayBackend)
//        |  BuildViewModel()  - game thread, copies plain values, no pointers
//        v
//   TheaterViewModel  (immutable for the duration of the UI frame)
//        |  TheaterUI::DrawMain(vm, ui, cmds)
//        v
//   CommandQueue      (UI pushes TheaterCommand values, never touches memory)
//        |  DrainCommands() - game thread, next tick, validated
//        v
//   Game runtime
//
// The UI owns only UIState (layout, selection mirror, timeline zoom, etc.).
// ============================================================================

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "TheaterTheme.h"

namespace TheaterUI
{
    // ------------------------------------------------------------------------
    // Ids and small value types
    // ------------------------------------------------------------------------
    using ActorId  = uint32_t;  // stable for the life of a replay session
    using TrackId  = uint32_t;
    using KeyId    = uint32_t;
    using MarkerId = uint32_t;
    inline constexpr ActorId InvalidActor = 0;

    struct Vec3 { float x = 0, y = 0, z = 0; };
    struct Rot3 { float yaw = 0, pitch = 0, roll = 0; }; // degrees

    // Time is always seconds (double) on the UI side; frames are derived.
    struct TimeRange { double start = 0.0, end = 0.0; bool Valid() const { return end > start; } };

    // ------------------------------------------------------------------------
    // Playback state machine (mirrors ReplaySession; UI only displays it)
    // ------------------------------------------------------------------------
    enum class PlaybackState : uint8_t
    {
        Idle, OpeningReplay, CheckingWorld, TransitioningWorld, WaitingForStreaming,
        PreparingScene, ReadyPaused, Playing, Paused, Seeking, Stopping,
        RestoringGameplay, Error
    };

    struct StateStyle { const char* label; Theme::Tone tone; bool busy; /* spinner */ bool toast; /* show top-centre */ };

    inline StateStyle StyleFor(PlaybackState s)
    {
        using T = Theme::Tone;
        switch (s)
        {
        case PlaybackState::Idle:                return { "IDLE",          T::Neutral, false, false };
        case PlaybackState::OpeningReplay:       return { "OPENING",       T::Info,    true,  true  };
        case PlaybackState::CheckingWorld:       return { "CHECKING WORLD",T::Info,    true,  true  };
        case PlaybackState::TransitioningWorld:  return { "TRANSITIONING", T::Warning, true,  true  };
        case PlaybackState::WaitingForStreaming: return { "STREAMING",     T::Warning, true,  true  };
        case PlaybackState::PreparingScene:      return { "PREPARING",     T::Info,    true,  true  };
        case PlaybackState::ReadyPaused:         return { "READY",         T::Success, false, false };
        case PlaybackState::Playing:             return { "PLAYING",       T::Success, false, false };
        case PlaybackState::Paused:              return { "PAUSED",        T::Neutral, false, false };
        case PlaybackState::Seeking:             return { "SEEKING",       T::Info,    true,  false };
        case PlaybackState::Stopping:            return { "STOPPING",      T::Neutral, true,  true  };
        case PlaybackState::RestoringGameplay:   return { "RESTORING",     T::Warning, true,  true  };
        case PlaybackState::Error:               return { "ERROR",         T::Error,   false, true  };
        }
        return { "?", T::Neutral, false, false };
    }

    enum class CameraMode : uint8_t { Gameplay, Free, Follow, Orbit, Dolly };
    enum class BackendKind : uint8_t { None, NativeReplay, PoseFallback };
    enum class ActorKind : uint8_t { Camera, Player, Npc, Boss, WorldObject, ReplayData };

    // ------------------------------------------------------------------------
    // VIEW MODELS - plain copies built on the game thread each frame
    // ------------------------------------------------------------------------
    struct ReplaySessionViewModel
    {
        PlaybackState state = PlaybackState::Idle;
        std::string   replayName;          // "Replay_001"
        std::string   areaName;            // "Leyndell, Royal Capital"
        std::string   blockId;             // "m60_41_38_00"
        double        currentTime = 0.0;   // seconds
        double        duration = 0.0;
        float         playbackSpeed = 1.0f;
        float         frameRate = 30.0f;   // replay sample rate, for frame stepping
        uint64_t      gameFrame = 0;
        BackendKind   backend = BackendKind::None;
        bool          worldReady = false;
        bool          inputIsolated = false;
        bool          recording = false;
        double        recordingElapsed = 0.0; // seconds, for the REC pill
        uint32_t      recordingFrames = 0;
        bool          loop = false;
        TimeRange     markInOut;           // invalid when unset
        std::string   errorText;           // set when state == Error
    };

    struct ActorViewModel
    {
        ActorId     id = InvalidActor;
        ActorId     parent = InvalidActor; // outliner hierarchy
        std::string name;                  // "Replay Player", "Enemy_001"
        ActorKind   kind = ActorKind::Npc;
        bool        visible = true;
        bool        locked = false;
        bool        selected = false;
        Theme::Tone status = Theme::Tone::Neutral; // status dot
        PlaybackState replayState = PlaybackState::Idle;
    };

    struct NativeReplayDebugViewModel
    {
        bool        available = false;
        std::string backendName;           // "NativeReplayBackend"
        std::string actorClass;            // "ReplayGhostIns"
        std::string manipulatorClass;      // "ReplayManipulator"
        int         manipulatorType = 0;   // 3
        bool        replayDataValid = false;
        int         primaryCursor = 0,   primaryCount = 0;   // 127 / 842
        int         secondaryCursor = 0, secondaryCount = 0; // 23 / 110
        int         activeGate = 0;
        bool        grounded = false;
        bool        inputOwnershipIsolated = false;
        std::string chrSet;                // "ghost_chr_set"
        PlaybackState state = PlaybackState::Idle;
        std::string blockId;
        bool        poseFallback = false;
        Vec3        position;
        Rot3        rotation;
        std::string weapon, armor;
    };

    struct CameraViewModel
    {
        CameraMode  mode = CameraMode::Free;
        std::string name;                  // "FreeCam", "Dolly_01"
        Vec3        position; Rot3 rotation;
        float       fovDeg = 60, focalLengthMm = 35, focusDistance = 8, aperture = 2.8f;
        float       nearClip = 0.1f, exposureEv = 0, shake = 0;
        bool        dofEnabled = false;
        ActorId     followTarget = InvalidActor, lookAtTarget = InvalidActor;
        bool        pathRecording = false;
    };

    // One dolly key as the gizmo and the Selected Key fields see it.
    struct CameraKeyViewModel
    {
        KeyId    id = 0;
        double   time = 0.0;           // never changed by the gizmo
        Vec3     position; Rot3 rotation;
        float    fovDeg = 60.0f;
        uint8_t  ease = 0;             // EaseMode: Linear, Smooth, Hold
        bool     selected = false, warning = false;
    };

    struct KeyframeViewModel  { KeyId id; double time; bool selected; Theme::MarkerType type; };
    struct MarkerViewModel    { MarkerId id; double time; double duration; Theme::MarkerType type; std::string label; };

    struct TrackViewModel
    {
        TrackId     id = 0;
        TrackId     parent = 0;            // 0 = root group
        std::string name;                  // "Transform", "Focal Length"
        Theme::Rgba colorId = Theme::Color::TextMuted; // 3px swatch at row start
        bool        isGroup = false;
        bool        locked = false, muted = false, solo = false;
        bool        keyable = true;
        std::vector<KeyframeViewModel> keys;
        std::vector<MarkerViewModel>   markers;
        std::vector<TimeRange>         ranges;  // clip / activity bars
    };

    struct EnvironmentViewModel
    {
        bool  fogEnabled = true;
        float fogDistance = 120, fogDensity = 0.35f, fogFalloff = 0.6f;
        float fogColor[3] = { 0.72f, 0.66f, 0.54f };
        float fogLuminance = 1.0f, fogScattering = 0.45f, skyHaze = 0.3f;
        std::array<float, 8> fogCurve{};   // sampled curve for the mini graph
        float timeOfDay = 14.5f;
        int   weatherId = 0;
        float exposureEv = 0.0f;
        float contrast = 1.0f, saturation = 1.0f, vignette = 0.0f;
        float letterboxAspect = 0.0f;      // 0 = off, 2.39 = scope
        bool  hideGameHud = true;
        int   activePreset = -1; bool presetEdited = false;
    };

    // ---- Export (ffmpeg / NVENC) ----
    enum class ExportKind    : uint8_t { Video, ImageSequence };
    enum class VideoCodec    : uint8_t { HEVC, H264, AV1 /*RTX 40+*/, ProRes422 /*CPU*/ };
    enum class EncoderKind   : uint8_t { Nvenc, Cpu };
    enum class ImageFormat   : uint8_t { Png8, Png16, Jpg, Exr };

    struct ExportSettings
    {
        TimeRange   range;                      // In/Out; empty = whole replay
        ExportKind  kind = ExportKind::Video;
        VideoCodec  codec = VideoCodec::HEVC;
        EncoderKind encoder = EncoderKind::Nvenc;
        int         cq = 18;                    // NVENC constant quality 0-51 (lower = better); CRF for CPU
        int         preset = 6;                 // NVENC P1-P7
        ImageFormat image = ImageFormat::Png8;
        int         width = 0, height = 0;      // 0 = native back-buffer size
        double      fps = 60.0;
        int         motionBlurSubframes = 1;    // 1 = off
        bool        gameAudio = true;
        std::string path;                       // "D:/Captures/Replay_001.mp4" or ".../name_#####.png"
    };

    struct EncoderCapsViewModel
    {
        bool        ffmpegFound = false;  std::string ffmpegPath, ffmpegVersion;
        bool        nvenc = false, nvencHevc = false, nvencAv1 = false;
        std::string gpuName;              // "RTX 4080"
    };

    enum class ExportJobState : uint8_t { Queued, Rendering, Done, Failed, Cancelled };
    struct ExportJobViewModel
    {
        uint32_t       id = 0;
        ExportJobState state = ExportJobState::Queued;
        ExportSettings settings;
        int            frame = 0, frameCount = 0;
        float          renderFps = 0.0f; double etaSeconds = 0.0;
        uint64_t       bytesWritten = 0;
        std::string    error;             // "disk full (D:)"
        std::string    dateText;
    };

    struct LookPresetViewModel { std::string name; Theme::Rgba swatch[3]; bool pinned = false; };

    struct ReplayEntryViewModel
    {
        std::string path, name, location, dateText;
        double      duration = 0.0;
        int         actorCount = 0;
        bool        loaded = false, olderFormat = false;  // olderFormat -> amber, opens read-only
    };

    struct PerfViewModel { float fps = 0; float frameMs = 0; };

    // What the game actually renders. Vanilla Elden Ring renders 16:9 and
    // letterboxes on ultrawide; with an ultrawide fix it matches the display.
    // The layout solver fits the game image using `contentAspect`, so the
    // game's own black bars never count as usable picture.
    struct GameFrameViewModel
    {
        uint32_t backBufferW = 0, backBufferH = 0;   // swap chain size
        float    contentAspect = 16.0f / 9.0f;       // aspect of the picture inside the back buffer
        float    viewProj[16] = {};                  // for overlays drawn in world space (dolly path, brackets)
        float    view[16] = {}, proj[16] = {};       // separate, for the camera key gizmo (ImGuizmo::Manipulate)
    };

    struct TheaterViewModel
    {
        ReplaySessionViewModel           session;
        CameraViewModel                  camera;
        EnvironmentViewModel             environment;
        NativeReplayDebugViewModel       nativeReplay;
        PerfViewModel                    perf;
        std::vector<ActorViewModel>      actors;
        std::vector<TrackViewModel>      tracks;     // flattened, parent links
        GameFrameViewModel               gameFrame;
        std::vector<CameraKeyViewModel>  cameraKeys;   // active dolly
        std::vector<LookPresetViewModel> lookPresets;
        std::vector<ReplayEntryViewModel> replays;     // newest first
        EncoderCapsViewModel            encoderCaps;
        std::vector<ExportJobViewModel> exportJobs;    // running first, then queued, then history
        double                          exportEstimateBytes = 0, exportEstimateSeconds = 0; // for the current settings
        ActorId                          selectedActor = InvalidActor;
        int                              cameraKeyCount = 0, eventCount = 0;
    };

    // ------------------------------------------------------------------------
    // COMMANDS - the only way the UI changes the game
    // ------------------------------------------------------------------------
    namespace Cmd
    {
        struct Play {};
        struct Pause {};
        struct Stop {};                                  // stop playback, keep replay loaded
        struct StopReplay {};                            // unload replay, restore gameplay
        struct StartRecording {};
        struct StopRecording {};
        struct OpenReplay       { std::string path; };
        struct Seek             { double time; bool scrubbing; }; // scrubbing=true while dragging
        struct StepFrames       { int delta; };          // +1 / -1
        struct SetSpeed         { float speed; };
        struct SetLoop          { bool loop; };
        struct SetMarkInOut     { TimeRange range; };
        struct SelectActor      { ActorId id; bool additive; };
        struct SetActorVisible  { ActorId id; bool visible; };
        struct SetActorLocked   { ActorId id; bool locked; };
        struct FocusActor       { ActorId id; };
        struct SetCameraMode    { CameraMode mode; };
        struct SetCameraParam   { uint16_t param; float value; }; // param = CameraParam enum
        struct SetFollowTarget  { ActorId id; };
        struct AddCameraKeyframe{ double time; };
        struct MoveKeyframes    { std::vector<KeyId> keys; double deltaTime; };
        struct DeleteKeyframes  { std::vector<KeyId> keys; };
        struct AddBookmark      { double time; std::string label; };
        struct SetTrackFlags    { TrackId id; bool locked, muted, solo; };
        struct SetEnvParam      { uint16_t param; float value; };     // EnvParam enum
        struct SetEnvColor      { uint16_t param; float rgb[3]; };
        struct ResetEnvParam    { uint16_t param; };
        struct SetDebugOverlay  { bool enabled; };
        // v3: camera keys edited in place (time and key count never change)
        struct SetCameraKeyTransform { KeyId key; Vec3 position; Rot3 rotation; bool dragging; }; // dragging=true merges into one undo step
        struct SetCameraKeyFov  { KeyId key; float fovDeg; };
        struct SetCameraKeyEase { KeyId key; uint8_t ease; };
        struct SetKeyFromView   { KeyId key; };                 // U: copy the current view into the key
        struct LookThroughKey   { KeyId key; bool enable; };    // V: viewport becomes that key's camera
        struct AddLookKey       { double time; };               // K in Look: keys every changed Look value
        struct ApplyLookPreset  { int index; };
        struct OpenReplayEntry  { std::string path; };
        struct RenameReplay     { std::string path, newName; };
        struct DeleteReplay     { std::string path; };          // asks to confirm first
        struct StartExport      { ExportSettings settings; };   // Ctrl+E
        struct QueueExport      { ExportSettings settings; };
        struct CancelExport     { uint32_t jobId; };            // Esc while rendering
        struct RemoveExportJob  { uint32_t jobId; };
        struct OpenExportFolder { uint32_t jobId; };
    }

    using TheaterCommand = std::variant<
        Cmd::Play, Cmd::Pause, Cmd::Stop, Cmd::StopReplay, Cmd::StartRecording, Cmd::StopRecording,
        Cmd::OpenReplay, Cmd::Seek, Cmd::StepFrames, Cmd::SetSpeed, Cmd::SetLoop, Cmd::SetMarkInOut,
        Cmd::SelectActor, Cmd::SetActorVisible, Cmd::SetActorLocked, Cmd::FocusActor,
        Cmd::SetCameraMode, Cmd::SetCameraParam, Cmd::SetFollowTarget, Cmd::AddCameraKeyframe,
        Cmd::MoveKeyframes, Cmd::DeleteKeyframes, Cmd::AddBookmark, Cmd::SetTrackFlags,
        Cmd::SetEnvParam, Cmd::SetEnvColor, Cmd::ResetEnvParam, Cmd::SetDebugOverlay,
        Cmd::SetCameraKeyTransform, Cmd::SetCameraKeyFov, Cmd::SetCameraKeyEase, Cmd::SetKeyFromView, Cmd::LookThroughKey,
        Cmd::AddLookKey, Cmd::ApplyLookPreset, Cmd::OpenReplayEntry, Cmd::RenameReplay, Cmd::DeleteReplay,
        Cmd::StartExport, Cmd::QueueExport, Cmd::CancelExport, Cmd::RemoveExportJob, Cmd::OpenExportFolder>;

    // Single-producer (UI/render thread) -> single-consumer (game thread).
    // Swap under a mutex once per frame; no per-command locking needed.
    struct CommandQueue
    {
        std::vector<TheaterCommand> pending;
        template <class T> void Push(T&& c) { pending.emplace_back(std::forward<T>(c)); }
    };

    // ------------------------------------------------------------------------
    // UI-ONLY STATE (owned by TheaterUI, persisted to theater_ui.ini)
    // ------------------------------------------------------------------------
    // Five tools on the rail, plus Debug / Settings / Exit at the bottom.
    enum class Tool : uint8_t { Scene, Camera, Look, Replays, Export, Debug, Settings, Weather, Lights, Particles, Bones, None };

    struct PanelLayout
    {
        float uiScaleUser   = 1.0f;                          // Settings slider, 0.75-1.5
        float panelWidth    = Theme::Layout.PanelWidth;      // logical px; solver may widen it
        float sequencerMin  = Theme::Layout.SequencerMin;    // user can drag the sequencer taller
        float trackTreeWidth= Theme::Layout.TrackTreeWidth;
        bool  panelOpen     = true;                          // clicking the active rail tool closes it
        bool  allowSelectionColumn = true;
    };

    // Output of TheaterUI::SolveLayout (TheaterLayout.h). Display pixels.
    struct LayoutRects
    {
        float  uiScale = 1.0f;
        ImVec2 railMin, railMax;
        ImVec2 panelMin, panelMax;          // zero-size when closed
        ImVec2 selectionMin, selectionMax;  // zero-size unless there was spare width
        ImVec2 sequencerMin, sequencerMax;
        ImVec2 gameMin, gameMax;            // where the scaled game image is drawn
        ImVec2 areaMin, areaMax;            // the region the game is centred in (pillarbox = area - game)
        bool   hasSelectionColumn = false;
    };

    // F4 state machine
    enum class UiVisibility : uint8_t
    {
        Shown,        // editor layout, game composited into gameMin/gameMax
        Hidden,       // F4: game full-screen untouched, only the REC pill (if recording)
        HiddenClean,  // Shift+F4: nothing at all, for external capture
    };

    struct TimelineView
    {
        double viewStart = 0.0;            // seconds at left edge of the track area
        double pixelsPerSecond = 40.0;     // zoom
        double snapStep = 1.0 / 30.0;      // seconds (1 replay frame)
        bool   snap = true;
        bool   followPlayhead = true;      // auto-scroll during playback
        bool   scrubbing = false;
        TimeRange selection;               // drag-select on ruler
        std::vector<KeyId> selectedKeys;
        float  scrollY = 0.0f;             // shared by track tree and lanes

        double TimeToX(double t, float laneX) const { return laneX + (t - viewStart) * pixelsPerSecond; }
        double XToTime(float x, float laneX) const { return viewStart + (x - laneX) / pixelsPerSecond; }
    };

    // Camera key gizmo (Camera tool, Dolly mode)
    enum class GizmoMode  : uint8_t { Move /*W*/, Rotate /*E*/, Aim /*A: drag a look-at point*/ };
    enum class GizmoSpace : uint8_t { Local, World };
    struct GizmoState
    {
        GizmoMode  mode = GizmoMode::Move;
        GizmoSpace space = GizmoSpace::Local;
        bool       lookingThrough = false;   // V
        bool       dragging = false;         // one undo step per drag
        float      snapMove = 0.25f, snapRotateDeg = 5.0f; // while Ctrl is held
    };

    struct TrackView { TrackId id; bool expanded = false; bool autoExpanded = false; int depth = 0; };

    struct SelectedActor { ActorId id = InvalidActor; ActorKind kind = ActorKind::Npc; };

    struct Toast { std::string text; Theme::Tone tone; double expiresAt; bool sticky; };

    // Icon glyphs come from IconFontCppHeaders (IconsMaterialSymbols.h: ICON_MS_VIDEOCAM, ...).
    // IconSet indirects them so the font can be swapped in one place.
    struct IconSet
    {
        // rail
        const char *Scene /*layers*/, *Camera /*videocam*/, *Look /*wb_sunny*/, *Replays /*movie*/, *ExportTool /*upload*/,
                   *Debug /*bug_report*/, *Settings /*settings*/, *Exit /*logout*/;
        // transport
        const char *Play, *Pause, *Stop, *Record, *StepBack, *StepFwd, *First, *Last, *Loop, *Speed, *HideUi /*fullscreen*/;
        // rows and actions
        const char *Eye, *EyeOff, *Lock, *Unlock, *Search, *Filter, *ChevronRight, *ChevronDown, *More,
                   *Player /*person*/, *Enemy /*skull*/, *Boss /*crown*/, *Weather /*cloud*/, *Time /*schedule*/, *Fog /*foggy*/,
                   *Key /*diamond*/, *AddKey, *Bookmark, *Free /*open_with*/, *Follow /*person_pin*/, *Orbit /*360*/,
                   *Dolly /*timeline*/, *Target /*my_location*/, *Copy, *Plus, *Minus, *Reset, *Warning,
                   *Tune, *Letterbox /*width_wide*/, *Palette, *Compare, *Export /*upload*/, *Trash /*delete*/, *Edit, *Star, *Folder, *Image, *ImageSequence /*filter_none*/, *Gpu /*memory*/;
    };

    struct UIState
    {
        UiVisibility  visibility = UiVisibility::Shown;
        double        hiddenAt = 0.0;          // for the 2 s "F4 Show UI" hint
        Tool          activeTool = Tool::Scene;
        PanelLayout   layout;
        LayoutRects   rects;
        TimelineView  timeline;
        GizmoState    gizmo;
        ExportSettings exportDraft;            // the Export panel's current settings
        std::vector<TrackView> trackViews;
        SelectedActor selection;
        char          searchFilter[64] = {};
        std::vector<Toast> toasts;
        bool          wantsCaptureKeyboard = false; // forwarded to the input-isolation layer
        Theme::Fonts  fonts;
        IconSet       icons;
    };

    // ------------------------------------------------------------------------
    // ENTRY POINTS
    // ------------------------------------------------------------------------
    void Init(UIState& ui, ImGuiIO& io, ImVec2 displaySize);
    void DrawMain(const TheaterViewModel& vm, UIState& ui, CommandQueue& out);

    // Called from the present hook BEFORE ImGui renders. When the UI is shown it
    // copies the back buffer and returns true; DrawMain then draws that copy into
    // rects.gameMin/Max. When hidden it does nothing and returns false.
    bool PrepareGameComposite(const UIState& ui);

    bool HandleHotkeys(const TheaterViewModel& vm, UIState& ui, CommandQueue& out); // F4, Space, K, R...
    void DrawToolRail(const TheaterViewModel& vm, UIState& ui);
    void DrawSidePanel(const TheaterViewModel& vm, UIState& ui, CommandQueue& out);   // Scene / Camera / Look / Replays / Debug
    void DrawSelectionColumn(const TheaterViewModel& vm, UIState& ui, CommandQueue& out);
    void DrawGameView(const TheaterViewModel& vm, UIState& ui);                     // composited image + world overlays + toasts
    void DrawSequencer(const TheaterViewModel& vm, UIState& ui, CommandQueue& out);
    void DrawCameraKeyGizmo(const TheaterViewModel& vm, UIState& ui, CommandQueue& out); // ImGuizmo in the game rect
    void DrawTimelineNavigator(const TheaterViewModel& vm, UIState& ui, ImVec2 min, ImVec2 max); // overview strip + h-scroll
    void DrawEventLog(const TheaterViewModel& vm, UIState& ui, float height);       // flexible filler at the bottom of panels
    void DrawRecordingPill(const TheaterViewModel& vm, const UIState& ui);          // the only overlay in Hidden mode
    void DrawToasts(UIState& ui, double now);
}

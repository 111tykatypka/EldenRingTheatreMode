#pragma once
// The one table of Theater Mode hotkeys. Every hotkey in the host, the overlay and the
// game DLL reads its key from here (Rust through tm_hotkey_vk), so a later Keybinds tab only
// has to replace `vk` at runtime and persist it. Add an action: one enum value + one row.
#include <cstddef>
#include <cstdint>

namespace theater_hotkeys {

enum class Action : std::uint32_t {
    ToggleOverlay,       // F4: show/hide the overlay (Shift+F4: also hide the REC pill)
    StartRecording,      // F5: host global hotkey
    StopRecording,       // F6: host global hotkey; also stops replay playback
    TogglePlayback,      // Space: play/pause the timeline, only while a replay is loaded or the overlay is open
    GhostCreateTest,     // F10: bone replay test: play / stop (the native ghost is retired)
    GhostRemoveTest,     // F11: unused (was native ghost remove)
    AnimProbe,           // F9: bone replay test: start / stop recording
    Count
};

// Where a key is allowed to act and whether the game must not see it.
enum class Scope : std::uint8_t {
    Global,              // works anywhere, the game also receives it (F5/F6 via RegisterHotKey)
    GameWindow,          // only while the game window is focused
    ReplayOrOverlay,     // only while a replay is loaded or the overlay is open; the game never sees it then
};

struct Binding { Action action; const char* id; const char* label; std::uint32_t vk; Scope scope; };

// Win32 virtual-key codes. F7 and F8 are intentionally unbound.
inline constexpr Binding kDefaults[] = {
    { Action::ToggleOverlay,   "toggle_overlay",    "Show / hide Theater Mode",      0x73 /*VK_F4*/,    Scope::GameWindow },
    { Action::StartRecording,  "start_recording",   "Start recording",               0x74 /*VK_F5*/,    Scope::Global },
    { Action::StopRecording,   "stop_recording",    "Stop recording / playback",     0x75 /*VK_F6*/,    Scope::Global },
    { Action::TogglePlayback,  "toggle_playback",   "Play / pause timeline",         0x20 /*VK_SPACE*/, Scope::ReplayOrOverlay },
    { Action::GhostCreateTest, "ghost_create_test", "Bone replay: play / stop (test)",   0x79 /*VK_F10*/,   Scope::GameWindow },
    { Action::GhostRemoveTest, "ghost_remove_test", "Unused (was ghost remove)",   0x7A /*VK_F11*/,   Scope::GameWindow },
    { Action::AnimProbe,       "anim_probe",        "Bone replay: record (test)",    0x78 /*VK_F9*/,    Scope::GameWindow },
};
static_assert(sizeof(kDefaults) / sizeof(kDefaults[0]) == static_cast<std::size_t>(Action::Count), "one row per action");

inline std::uint32_t Key(Action a) { return kDefaults[static_cast<std::size_t>(a)].vk; }
inline Scope ScopeOf(Action a) { return kDefaults[static_cast<std::size_t>(a)].scope; }
inline const char* Label(Action a) { return kDefaults[static_cast<std::size_t>(a)].label; }

}

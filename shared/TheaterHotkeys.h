#pragma once
// The one table of Theater Mode hotkeys. Every hotkey in the host, the overlay and the
// game DLL reads its key from here (Rust through tm_hotkey_vk). Settings persists
// overrides for the host and DLL. Add an action: one enum value + one row.
#include <cstddef>
#include <cstdint>
#include <array>
#include <atomic>
#include <string>
#include <istream>
#include <charconv>

namespace theater_hotkeys {

enum class Action : std::uint32_t {
    ToggleOverlay,       // F4: show/hide the overlay (Shift+F4: also hide the REC pill)
    StartRecording,      // F5: host global hotkey
    StopRecording,       // F6: host global hotkey; also stops replay playback
    TogglePlayback,      // Space: play/pause the timeline, only while a replay is loaded or the overlay is open
    GhostCreateTest,     // F10: unused (native ghost retired; bone replays follow the timeline)
    GhostRemoveTest,     // F11: unused (was native ghost remove)
    AnimProbe,           // F9: unused (was research probes)
    CycleCamera,
    AddDollyKey,
    ClearDollyKeys,
    Forward,Backward,Left,Right,Up,Down,YawLeft,YawRight,PitchUp,PitchDown,RollLeft,RollRight,Fast,Slow,SpeedUp,SpeedDown,ResetRoll,FovUp,FovDown,ResetFov,
    ToggleDollyControls,
    ToggleAllHud,
    PlayDollyPath,
    ToggleExport,        // F7: start / stop the video or image-sequence export
    Count
};

// Where a key is allowed to act and whether the game must not see it.
enum class Scope : std::uint8_t {
    Global,              // host RegisterHotKey for F5/F6, scoped to game foreground
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
    { Action::GhostCreateTest, "ghost_create_test", "Unused (was ghost create)",   0x79 /*VK_F10*/,   Scope::GameWindow },
    { Action::GhostRemoveTest, "ghost_remove_test", "Unused (was ghost remove)",   0x7A /*VK_F11*/,   Scope::GameWindow },
    { Action::AnimProbe,       "anim_probe",        "Unused (was research probe)",    0x78 /*VK_F9*/,    Scope::GameWindow },
    { Action::CycleCamera, "cycle_camera", "Cycle camera selection", 0x72 /*F3*/, Scope::GameWindow },
    { Action::AddDollyKey, "add_dolly_key", "Add dolly keyframe", 0x4B /*K*/, Scope::GameWindow },
    { Action::ClearDollyKeys, "clear_dolly_keys", "Delete all dolly keys (confirm)", 0x4C /*L*/, Scope::GameWindow },
    { Action::Forward,"camera_forward","Camera forward",'W',Scope::GameWindow },
    { Action::Backward,"camera_backward","Camera backward",'S',Scope::GameWindow },
    { Action::Left,"camera_left","Camera left",'A',Scope::GameWindow },
    { Action::Right,"camera_right","Camera right",'D',Scope::GameWindow },
    { Action::Up,"camera_up","Camera up",'E',Scope::GameWindow },
    { Action::Down,"camera_down","Camera down",'Q',Scope::GameWindow },
    { Action::YawLeft,"yaw_left","Yaw left",0x25,Scope::GameWindow },
    { Action::YawRight,"yaw_right","Yaw right",0x27,Scope::GameWindow },
    { Action::PitchUp,"pitch_up","Pitch up",0x26,Scope::GameWindow },
    { Action::PitchDown,"pitch_down","Pitch down",0x28,Scope::GameWindow },
    { Action::RollLeft,"roll_left","Roll left",'Z',Scope::GameWindow },
    { Action::RollRight,"roll_right","Roll right",'X',Scope::GameWindow },
    { Action::Fast,"camera_fast","Fast movement (hold)",0x10,Scope::GameWindow },
    { Action::Slow,"camera_slow","Precision movement (hold)",0x11,Scope::GameWindow },
    { Action::SpeedUp,"speed_up","Increase camera speed",0xBB,Scope::GameWindow },
    { Action::SpeedDown,"speed_down","Decrease camera speed",0xBD,Scope::GameWindow },
    { Action::ResetRoll,"reset_roll","Reset roll",0x62,Scope::GameWindow },
    { Action::FovUp,"fov_up","Increase FOV",0x6B,Scope::GameWindow },
    { Action::FovDown,"fov_down","Decrease FOV",0x6D,Scope::GameWindow },
    { Action::ResetFov,"reset_fov","Reset FOV",0x6A,Scope::GameWindow },
    { Action::ToggleDollyControls,"toggle_dolly_controls","Show / hide Dolly viewport controls",'P',Scope::GameWindow },
    { Action::ToggleAllHud,"toggle_all_hud","Clean view: toggle game HUD and Theater UI",'O',Scope::GameWindow },
    { Action::PlayDollyPath,"play_dolly_path","Play Dolly path from first key",'J',Scope::GameWindow },
    { Action::ToggleExport,"toggle_export","Start / stop video export",0x76 /*VK_F7*/,Scope::GameWindow },
};
static_assert(sizeof(kDefaults) / sizeof(kDefaults[0]) == static_cast<std::size_t>(Action::Count), "one row per action");

inline std::array<std::atomic<std::uint32_t>,static_cast<std::size_t>(Action::Count)> overrides{};
inline std::atomic_bool rebinding=false;
inline std::uint32_t Key(Action a) { auto i=static_cast<std::size_t>(a);auto v=overrides[i].load();return v?v-1:kDefaults[i].vk; }
inline bool retired(Action a){return a==Action::GhostCreateTest||a==Action::GhostRemoveTest||a==Action::AnimProbe;}
bool Rebind(Action action,std::uint32_t vk,std::string&error);
inline bool DecodeBindings(std::istream&in,std::array<std::uint32_t,static_cast<std::size_t>(Action::Count)>&result){
 std::string line;if(!std::getline(in,line)||line!="THEATER_KEYBINDS_V1")return false;
 std::array<std::uint32_t,static_cast<std::size_t>(Action::Count)> keys{};std::array<bool,static_cast<std::size_t>(Action::Count)> seen{};for(auto&b:kDefaults)keys[static_cast<std::size_t>(b.action)]=b.vk;
 while(std::getline(in,line)){auto equal=line.find('=');if(equal==std::string::npos)return false;auto name=line.substr(0,equal),num=line.substr(equal+1);unsigned v=0;auto parsed=std::from_chars(num.data(),num.data()+num.size(),v);if(parsed.ec!=std::errc{}||parsed.ptr!=num.data()+num.size()||!v||v>255)return false;
  bool known=false;for(auto&b:kDefaults)if(name==b.id){auto i=static_cast<std::size_t>(b.action);if(seen[i])return false;seen[i]=true;keys[i]=v;known=true;}if(!known)return false;}
 if(!in.eof())return false;
 for(auto action:{Action::ToggleDollyControls,Action::ToggleAllHud,Action::PlayDollyPath,Action::ToggleExport}){
 const auto added=static_cast<std::size_t>(action);
 if(!seen[added]){bool conflict=false;for(std::size_t i=0;i<keys.size();++i)if(i!=added&&!retired(kDefaults[i].action)&&keys[i]==keys[added])conflict=true;
  if(conflict){for(unsigned candidate=0x7B;candidate<=0x87;++candidate){bool used=false;for(std::size_t i=0;i<keys.size();++i)if(i!=added&&!retired(kDefaults[i].action)&&keys[i]==candidate)used=true;if(!used){keys[added]=candidate;break;}}}}
 }
 for(auto&a:kDefaults)for(auto&b:kDefaults)if(a.action<b.action&&!retired(a.action)&&!retired(b.action)&&keys[static_cast<std::size_t>(a.action)]==keys[static_cast<std::size_t>(b.action)])return false;
 result=keys;return true;
}
void Reload(); // IPC/host worker only: never called by the camera hook
inline Scope ScopeOf(Action a) { return kDefaults[static_cast<std::size_t>(a)].scope; }
inline const char* Label(Action a) { return kDefaults[static_cast<std::size_t>(a)].label; }

}

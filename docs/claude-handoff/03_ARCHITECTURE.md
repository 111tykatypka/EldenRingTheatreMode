# Architecture

Snapshot: 2026-10-06, source `3d97070`. Engine features described as experimental are not runtime accepted.

```text
eldenring.exe -- YAFSML --> TheaterMode.dll
  CSTaskImp -> ChrIns_PostPhysics -> reacquire WorldChrMan/main_player
       | observations
       v
  Rust telemetry worker -> named pipes -> C++ editor backend
                                         | Recorder -> ERPLAY Writer -> disk
                                         | Reader -> ReplayPlayer/Clock
                                         | position lerp / quaternion SLERP
                                         v
  Rust control worker <- latest request <- dedicated playback worker
       | copied synchronized state, no engine mutation on IPC thread
       v
  verified game callback -> public mutable SDK -> player transform

  original TestNetStep -> guarded native ghost create/ownership hooks
  original world removal drain -> native removal -> DelayDelete
       | status bridge
       v
  injected DX12 overlay -> Editor pipe -> host command/backend
  standalone WinMain/DX11 ImGui UI ---> host command/backend
```

## Entry points and modules

Current EXE is `src/modern_main.cpp`, a WIN32 CMake target. `src/monitor.cpp` remains a legacy monitor, not the current entry point. `adapter/src/lib.rs` initializes Rust integration/profile/tasks/workers/render bridge. `native_ui/TheaterRenderBackend.cpp` owns injected graphics. `NativeReplayGhostPrototype.cpp` implements isolated native hooks; Rust bridge computes binding offsets and uses existing logging.

ERPLAY Writer streams bounded chunks; ReplayRecorder manages recording/pause-time accounting. Reader validates/indexes random access. ReplayPlayer owns playback, seek, speed and interpolation. `playback_worker.hpp` decouples dispatch from UI refresh. `in_game_replay` translates current state to control packets. `editor_backend` exposes snapshots/commands, `modern_ui` draws panels, `ingame_editor_server` exposes bounded overlay snapshots. Launcher and shared GameProfile are independent services.

## Thread model

- Host UI thread owns Win32 messages, DPI/resize and DX11 ImGui. No game pointers.
- Telemetry/character workers receive observations; bounded writer flushes to disk. Actor cursor access was serialized with playback after a race audit.
- Host dedicated playback worker dispatches interpolated state; requested 120 Hz does not mean game runs at 120 Hz.
- DLL IPC workers validate/copy latest requests. No engine writes or pipe blocking in game callbacks.
- ChrIns_PostPhysics reacquires current player for observations and controlled legacy writes. Early PreBehaviorSafe handles experimental action/input ownership; blocked in native read-only feature.
- Native create occurs only inside genuine TestNetStep context. Remove occurs at native world removal drain, not generic PostPhysics.
- Render thread owns its ImGui context, processes queued input, draws and submits DX12 transitions before forwarding Present.

## Pipes

|Name|Purpose/ownership|
|---|---|
|`\\.\pipe\EldenRingTheaterMode_1_17`|Host server, DLL PLAYER_STATE telemetry|
|`\\.\pipe\EldenRingTheaterMode_1_17_Control`|DLL duplex server, host replay/control requests and status|
|`\\.\pipe\EldenRingTheaterMode_1_17_Characters`|Separate character observation stream|
|`\\.\pipe\EldenRingTheaterMode_1_17_Editor`|Host server, injected UI client polling about 50 ms|

No demonstrated socket/shared-memory transport. No raw engine pointers should be sent.

Control magic 0x544d4354, current v3/128 bytes, compatibility v1/64 and v2/96. Carries sequence/source clock/transform/state/flags/replay timestamp/session/acknowledged sequence/ActionState. HELLO, HEARTBEAT, PROBE_NUDGE, STOP, REPLAY_BEGIN/APPLY/FINISH, trace, actor apply, ownership and return commands. Validation includes finite transform/quaternion, sequence/session/freshness. Player loss, invalid state and disconnect stop writes and restore normal control.

Editor magic 0x37495554, version1, Request32 bytes with monotonic sequence; poll/play/pause/stop/restart/seek/previous/next/speed/select/page. Snapshot has connection/player/live position/replay time/speed/diagnostic and 16 recorded actors per page. This is a page/resource bound, not a total actor limit. No generalized native ghost registry or camera track schema exists.

Actual depth-three module tree and concrete source anchors are appended below. Generated build/cache directories are excluded.

## Concrete source anchors (implementation HEAD 3d97070)

Lines refer to original code, not the appended prose. Long lines are truncated for display; inspect source before edits.

### `src/modern_main.cpp`

```text
src/modern_main.cpp:50: // RegisterHotKey is application-wide, independent of foreground ImGui focus.
src/modern_main.cpp:84: int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
src/modern_main.cpp:117: ImGui::CreateContext();
src/modern_main.cpp:121: ImGui::StyleColorsDark();
src/modern_main.cpp:132: io.Fonts->AddFontFromFileTTF(game_launcher::utf8(font.wstring()).c_str(),
src/modern_main.cpp:135: io.Fonts->AddFontDefault();
src/modern_main.cpp:139: editor::load_settings();
src/modern_main.cpp:141: game_launcher::utf8((theater::app.root / L"Modern.layout.ini").wstring());
src/modern_main.cpp:173: ImGui::GetStyle().FontScaleDpi = dpi;
src/modern_main.cpp:202: editor::save_settings();
```

### `src/editor_backend.hpp`

```text
src/editor_backend.hpp:40: enum class Command { start, pause, resume, stop };
src/editor_backend.hpp:41: struct ReplayEntry {
src/editor_backend.hpp:47: struct Snapshot {
src/editor_backend.hpp:58: struct App {
src/editor_backend.hpp:82: struct CharacterView {
src/editor_backend.hpp:96: struct PlaybackView {
```

### `src/replay_player.hpp`

```text
src/replay_player.hpp:9: enum class Status { stopped, loading, ready, playing, paused, seeking, error };
src/replay_player.hpp:10: struct State {
src/replay_player.hpp:19: class Player {
src/replay_player.hpp:41: class BookmarkStore {
```

### `src/playback_worker.hpp`

```text

```

### `src/in_game_replay.hpp`

```text
src/in_game_replay.hpp:8: enum class Phase { inactive, starting, playing, paused, finishing, finished, error, restarting, preparing };
src/in_game_replay.hpp:10: class Controller {
```

### `shared/TheaterUiProtocol.h`

```text
shared/TheaterUiProtocol.h:7: enum Command : std::uint32_t { poll, play, pause, stop, restart, seek, previous, next, speed, select, page };
shared/TheaterUiProtocol.h:8: struct Request { std::uint32_t magic_value{magic},version{1},command{};std::uint32_t reserved{};std::uint64_t sequence{},value{}; };
shared/TheaterUiProtocol.h:9: struct Actor {std::uint64_t id{},handle{};std::uint32_t entity{},type{};std::int32_t npc{};std::uint32_t reserved{};};
shared/TheaterUiProtocol.h:11: struct Snapshot {std::uint32_t magic_value{magic},version{1},loaded{},active{},phase{},count{},offset{},total{};
```

### `adapter/src/lib.rs`

```text
adapter/src/lib.rs:41: #[repr(C)]#[derive(Clone,Copy,Default)]pub struct PlayerSample{pub sequence:u64,pub timestamp_ns:u64,pub position:[f32;3],pub quaternion_xyzw:[f32;4],pub euler_raw:[f32;3],pub player_present:u32}
adapter/src/lib.rs:42: #[repr(C)]#[derive(Clone,Copy,Default)]struct WireMessage{magic:u32,version:u16,kind:u16,sequence:u64,timestamp_ns:u64,position:[f32;3],quaternion_xyzw:[f32;4],euler_raw:[f32;3],player_present:u32,reserved:u32,action:player_action::State}
adapter/src/lib.rs:44: #[repr(C)]struct TmValidationReport{size:u32,status:u32,checked:u32,passed:u32,file_version:[u16;4],product_version:[u16;4],machine:u16,reserved:u16,image_base:usize,runtime_path:[u16;32768],sha256:[i8;65]}
adapter/src/lib.rs:45: unsafe extern "C"{fn tm_render_start(emergency:extern "C" fn())->i32;}
adapter/src/lib.rs:46: extern "C" fn render_emergency_stop(){replay_runtime::stop(0);probe_runtime::stop(0);}
adapter/src/lib.rs:47: #[link(name="GameProfile",kind="static")]unsafe extern "C"{fn tm_validate_profile(path:*const u16,image_base:usize,report:*mut TmValidationReport)->u32;}
adapter/src/lib.rs:48: #[link(name="kernel32")]unsafe extern "system"{fn GetModuleFileNameW(module:*mut c_void,buffer:*mut u16,size:u32)->u32;fn GetModuleHandleW(name:*const u16)->*mut c_void;fn GetCurrentProcessId()->u32;}
adapter/src/lib.rs:49: #[link(name="mincore")]unsafe extern "system"{fn QueryInterruptTimePrecise(time:*mut u64);}
adapter/src/lib.rs:51: fn monotonic_ns()->u64 {let mut ticks=0;unsafe{QueryInterruptTimePrecise(&mut ticks)};ticks*100}
adapter/src/lib.rs:52: fn log_game(message:&str){
adapter/src/lib.rs:57: fn set_state(state:u32,label:&str){let old=INIT_STATE.swap(state,Ordering::AcqRel);if old!=state{log_game(&format!("INIT_STATE={label} ({state})"));}}
adapter/src/lib.rs:60: #[cfg(windows)]unsafe fn resolve_register_task()->Result<(RegisterTaskFn,u32),String>{
adapter/src/lib.rs:67: #[cfg(not(windows))]unsafe fn resolve_register_task()->Result<(RegisterTaskFn,u32),String>{Err("Windows runtime required".into())}
adapter/src/lib.rs:69: fn publish(s:PlayerSample,action:player_action::State){SEQ.fetch_add(1,Ordering::AcqRel);TIME.store(s.timestamp_ns,Ordering::Relaxed);PRESENT.store(s.player_present,Ordering::Relaxed);let values=[s.position[0],s.position[1],s.position[2],s.quaternion_xyzw[0],s.quaternion_xyzw[1],
adapter/src/lib.rs:70: fn latest()->Option<PlayerSample>{loop{let before=SEQ.load(Ordering::Acquire);if before&1!=0{std::hint::spin_loop();continue;}let mut v=[0.0;10];for(dst,src)in v.iter_mut().zip(VALUES.iter()){*dst=f32::from_bits(src.load(Ordering::Relaxed));}let t=TIME.load(Ordering::Relaxed);let
adapter/src/lib.rs:71: #[unsafe(no_mangle)]pub unsafe extern "C" fn theater_get_latest_sample(out:*mut PlayerSample)->bool{if out.is_null(){return false;}if let Some(sample)=latest(){unsafe{out.write(sample);}true}else{false}}
adapter/src/lib.rs:72: fn latest_action()->player_action::State {let mut b=[0u8;32];for(i,a)in ACTION.iter().enumerate(){b[i*4..i*4+4].copy_from_slice(&a.load(Ordering::Relaxed).to_le_bytes());}player_action::State::decode(&b)}
adapter/src/lib.rs:73: fn latest_pair()->Option<(PlayerSample,player_action::State)>{loop{let before=SEQ.load(Ordering::Acquire);if before&1!=0{continue;}let sample=latest();let action=latest_action();std::sync::atomic::fence(Ordering::Acquire);if before==SEQ.load(Ordering::Acquire){return sample.map(|
adapter/src/lib.rs:74: fn version(v:[u16;4])->String{format!("{}.{}.{}.{}",v[0],v[1],v[2],v[3])}
adapter/src/lib.rs:75: fn check_text(report:&TmValidationReport,flag:u32)->&'static str{if report.checked&flag==0{"UNAVAILABLE"}else if report.passed&flag!=0{"PASS"}else{"FAIL"}}
adapter/src/lib.rs:76: #[cfg(windows)]fn validate_runtime_profile()->u32{
adapter/src/lib.rs:95: #[cfg(not(windows))]fn validate_runtime_profile()->u32{0x101}
adapter/src/lib.rs:96: #[cfg(windows)]fn pipe_worker(){
adapter/src/lib.rs:99: #[link(name="kernel32")]unsafe extern "system"{fn CreateFileW(n:*const u16,a:u32,s:u32,sa:*mut c_void,c:u32,f:u32,t:Handle)->Handle;fn WriteFile(h:Handle,b:*const c_void,n:u32,w:*mut u32,o:*mut c_void)->i32;fn CloseHandle(h:Handle)->i32;fn Sleep(ms:u32);}
adapter/src/lib.rs:122: pub unsafe extern "system" fn DllMain(_module:usize,reason:u32,_reserved:usize)->i32 {
adapter/src/lib.rs:219: unsafe{register_task(task,CSTaskGroupIndex::ChrIns_PostPhysics,callback);}
adapter/src/lib.rs:220: log_game(&format!("Recurring task registered using resolved function eldenring.exe+0x{register_rva:X}; group=ChrIns_PostPhysics; waiting for WORLDCHR_READY and PLAYER_FOUND"));
adapter/src/lib.rs:249: #[test]fn transform_and_action_snapshot_remain_paired(){
```

## Actual module tree, depth three

Generated source/cache directories excluded; third-party internals summarized.

```text
.gitignore — authored documentation/configuration/reference
adapter — Rust cdylib and locked SDK integration
  adapter/build.rs — build/dependency configuration
  adapter/Cargo.lock — build/dependency configuration
  adapter/Cargo.toml — build/dependency configuration
  adapter/src — C++ host, storage/player/backend/UI/launcher/tests
    adapter/src/actor_replay.rs — game-side actor replay
    adapter/src/capture_fields.rs — game-side capture fields
    adapter/src/character_capture.rs — game-side character capture
    adapter/src/control_protocol.rs — game-side control protocol
    adapter/src/fidelity_capture.rs — game-side fidelity capture
    adapter/src/grounding.rs — game-side grounding
    adapter/src/lib.rs — game-side lib
    adapter/src/local_input.rs — game-side local input
    adapter/src/locomotion_trace.rs — game-side locomotion trace
    adapter/src/native_bloodstain.rs — game-side native bloodstain
    adapter/src/native_debug_flags.rs — game-side native debug flags
    adapter/src/native_ghost_prototype.rs — game-side native ghost prototype
    adapter/src/ownership_probe.rs — game-side ownership probe
    adapter/src/player_action.rs — game-side player action
    adapter/src/probe_runtime.rs — game-side probe runtime
    adapter/src/replay_return.rs — game-side replay return
    adapter/src/replay_runtime.rs — game-side replay runtime
    adapter/src/research_readonly.rs — game-side research readonly
    adapter/src/start_guard.rs — game-side start guard
    adapter/src/transform_probe.rs — game-side transform probe
    adapter/src/transform_replay.rs — game-side transform replay
    adapter/src/visual_capture.rs — game-side visual capture
    adapter/src/world_observation.rs — game-side world observation
build_release.bat — build/dependency configuration
CMakeLists.txt — build/dependency configuration
ERPLAY_FORMAT.md — authored documentation/configuration/reference
native_ui — injected DX12 renderer, native ghost bridge/hooks, smoke tests
  native_ui/NativeGhostFingerprints.h — in-process native renderer/prototype component
  native_ui/NativeReplayGhostPrototype.cpp — in-process native renderer/prototype component
  native_ui/render_backend_tests.cpp — regression/test harness; not in-game acceptance
  native_ui/render_dx12_smoke.cpp — in-process native renderer/prototype component
  native_ui/TheaterRenderBackend.cpp — in-process native renderer/prototype component
notes — dated implementation/validation evidence
  notes/CHARACTER_REPLAY_DESIGN.md — authored documentation/configuration/reference
  notes/CHARACTER_RUNTIME_RESEARCH.md — authored documentation/configuration/reference
  notes/CSTASK_2_7_0_0_DIAGNOSTIC.md — authored documentation/configuration/reference
  notes/CURRENT_RUNTIME_LIMITATIONS.md — authored documentation/configuration/reference
  notes/DEVELOPER_TRANSFER_BRIEF_2026_10_06.md — authored documentation/configuration/reference
  notes/FAILED_EXPERIMENTS.md — authored documentation/configuration/reference
  notes/GHIDRA_RUNTIME_RESEARCH.md — authored documentation/configuration/reference
  notes/IMGUI_UI_ARCHITECTURE.md — authored documentation/configuration/reference
  notes/KNOWN_ISSUES.md — authored documentation/configuration/reference
  notes/MODERN_REBUILD_STATUS.md — authored documentation/configuration/reference
  notes/NATIVE_GHOST_PROTOTYPE_IMPLEMENTATION_STATUS.md — authored documentation/configuration/reference
  notes/NATIVE_GHOST_PROTOTYPE_RUNTIME_TEST_PLAN.md — authored documentation/configuration/reference
  notes/NATIVE_PAYLOAD_CHECKPOINT_2026_10_06.md — authored documentation/configuration/reference
  notes/NIGHTLY_KNOWN_ISSUES.md — authored documentation/configuration/reference
  notes/NIGHTLY_RESEARCH_FINDINGS.md — authored documentation/configuration/reference
  notes/NIGHTLY_STATUS.md — authored documentation/configuration/reference
  notes/PHASE1_5_FINAL_STATUS.md — authored documentation/configuration/reference
  notes/PHASE2_FINAL_STATUS.md — authored documentation/configuration/reference
  notes/PHASE3_FINAL_STATUS.md — authored documentation/configuration/reference
  notes/PHASE4A_LAUNCHER_STATUS.md — authored documentation/configuration/reference
  notes/PHASE4A_MANUAL_TEST.md — authored documentation/configuration/reference
  notes/PHASE4A_STATUS.md — authored documentation/configuration/reference
  notes/PHASE4B_MANUAL_TEST.md — authored documentation/configuration/reference
  notes/PHASE4B_STATUS.md — authored documentation/configuration/reference
  notes/PHASE4C_STATUS.md — authored documentation/configuration/reference
  notes/PHASE5_MANUAL_TEST.md — authored documentation/configuration/reference
  notes/PHASE5_PLAYER_ANIMATION_RESEARCH.md — authored documentation/configuration/reference
  notes/PHASE5_ROOT_MOTION.md — authored documentation/configuration/reference
  notes/PHASE5_STATUS.md — authored documentation/configuration/reference
  notes/PHASE5C_CHARACTER_DRIVING_SYSTEMS.md — authored documentation/configuration/reference
  notes/PHASE5C_MANUAL_TEST.md — authored documentation/configuration/reference
  notes/PHASE5C_STATUS.md — authored documentation/configuration/reference
  notes/PHASE6_TESTER_STATUS.md — authored documentation/configuration/reference
  notes/PHASE7_CRASH_DIAGNOSTIC.md — authored documentation/configuration/reference
  notes/PHASE7_IMPLEMENTATION_STATUS.md — authored documentation/configuration/reference
  notes/PHASE7_KNOWN_ISSUES.md — authored documentation/configuration/reference
  notes/PHASE7_RECORDING_HOTKEY_FIX.md — authored documentation/configuration/reference
  notes/PHASE7_REPLAY_LEASE_FIX.md — authored documentation/configuration/reference
  notes/PHASE7_RUNTIME_TEST_PLAN.md — authored documentation/configuration/reference
  notes/PHASE7_SIMPLIFIED_CONTROLS.md — authored documentation/configuration/reference
  notes/PHASE8_CAPTURE_FIDELITY.md — authored documentation/configuration/reference
  notes/PHASE8_CAPTURE_FIELDS.md — authored documentation/configuration/reference
  notes/PHASE8_REAL_CAPTURE_AUDIT.json — authored documentation/configuration/reference
  notes/PHASE8_REAL_CAPTURE_INSPECTION.json — authored documentation/configuration/reference
  notes/PHASE8_REAL_CAPTURE_STATUS.md — authored documentation/configuration/reference
  notes/PHASE9_AUTO_START.md — authored documentation/configuration/reference
  notes/PHASE9_PLAYER_ACTION_REPLAY.md — authored documentation/configuration/reference
  notes/PLAYER_STATE_1_17_IMPLEMENTATION.md — authored documentation/configuration/reference
  notes/PLAYER_STATE_1_17_RESEARCH.md — authored documentation/configuration/reference
  notes/RUNTIME_TEST_PLAN.md — authored documentation/configuration/reference
  notes/TEST_RESULTS_TEMPLATE.md — authored documentation/configuration/reference
  notes/TESTER_README.md — authored documentation/configuration/reference
PHASE2_STATUS.md — authored documentation/configuration/reference
probe — standalone executable compatibility probe
  probe/CMakeLists.txt — build/dependency configuration
  probe/main.cpp — authored documentation/configuration/reference
README.md — authored documentation/configuration/reference
README.txt — authored documentation/configuration/reference
REPLAY_PLAYER.md — authored documentation/configuration/reference
research — independently authored native type/layout/lifecycle evidence
  research/ACCEPTANCE_MATRIX.md — authored documentation/configuration/reference
  research/BLOODSTAIN_GHOST_PIPELINE.md — authored documentation/configuration/reference
  research/NATIVE_GHOST_LIFECYCLE_BLOCKER.md — authored documentation/configuration/reference
  research/NATIVE_PAYLOAD_GHOST_FINDINGS.md — authored documentation/configuration/reference
  research/NATIVE_PAYLOAD_RUNTIME_TEST.md — authored documentation/configuration/reference
  research/NATIVE_REPLAY_ACCEPTANCE.md — authored documentation/configuration/reference
  research/NATIVE_REPLAY_FRAME_FORMAT.md — authored documentation/configuration/reference
  research/NATIVE_REPLAY_RUNTIME_RESULT.json — authored documentation/configuration/reference
  research/NATIVE_REPLAY_RUNTIME_RESULT.md — authored documentation/configuration/reference
  research/NATIVE_REPLAY_RUNTIME_TEST.md — authored documentation/configuration/reference
  research/NATIVE_REPLAY_SOURCE_INDEX.json — authored documentation/configuration/reference
  research/NATIVE_REPLAY_SOURCE_INDEX.md — authored documentation/configuration/reference
  research/REPLAY_GHOST_ACTOR.md — authored documentation/configuration/reference
  research/REPLAY_MANIPULATOR.md — authored documentation/configuration/reference
  research/REPLAY_RECORDER_LAYOUT.md — authored documentation/configuration/reference
  research/ROLLBACK_RECOVERY_STATUS.md — authored documentation/configuration/reference
  research/symbols_2_7_0_0.json — authored documentation/configuration/reference
scripts — project build/research helpers
  scripts/Build-CaptureFidelity.ps1 — authored documentation/configuration/reference
  scripts/Build-Modern.ps1 — authored documentation/configuration/reference
  scripts/Build-NativeBloodstainResearch.ps1 — authored documentation/configuration/reference
  scripts/Build-Nightly.ps1 — authored documentation/configuration/reference
  scripts/Build-Phase4A.ps1 — authored documentation/configuration/reference
  scripts/Build-Phase4B.ps1 — authored documentation/configuration/reference
  scripts/Build-Phase5.ps1 — authored documentation/configuration/reference
  scripts/Build-Phase5C.ps1 — authored documentation/configuration/reference
  scripts/Build-Phase7.ps1 — authored documentation/configuration/reference
  scripts/Build-PlayerActionReplay.ps1 — authored documentation/configuration/reference
  scripts/Build-ReplayAutoStart.ps1 — authored documentation/configuration/reference
  scripts/Build-Tester.ps1 — authored documentation/configuration/reference
  scripts/Enable-ReadOnlyResearch.ps1 — authored documentation/configuration/reference
shared — cross-language profile/UI protocol and capture schema
  shared/capture_schema.json — authored documentation/configuration/reference
  shared/CMakeLists.txt — build/dependency configuration
  shared/GameProfile.cpp — authored documentation/configuration/reference
  shared/GameProfile.h — authored documentation/configuration/reference
  shared/TheaterUiProtocol.h — authored documentation/configuration/reference
src — C++ host, storage/player/backend/UI/launcher/tests
  src/action_track_tests.cpp — regression/test harness; not in-game acceptance
  src/capture_schema.hpp — host capture schema
  src/capture_track.hpp — host capture track
  src/capture_track_tests.cpp — regression/test harness; not in-game acceptance
  src/character_client.cpp — host character client
  src/character_client.hpp — host character client
  src/character_client_tests.cpp — regression/test harness; not in-game acceptance
  src/character_track.hpp — host character track
  src/character_track_tests.cpp — regression/test harness; not in-game acceptance
  src/character_visual.hpp — host character visual
  src/clock_helpers.hpp — host clock helpers
  src/editor_backend.cpp — host editor backend
  src/editor_backend.hpp — host editor backend
  src/editor_math.hpp — host editor math
  src/erplay.cpp — host erplay
  src/erplay.hpp — host erplay
  src/erplay_tests.cpp — regression/test harness; not in-game acceptance
  src/game_control.cpp — host game control
  src/game_control.hpp — host game control
  src/game_control_tests.cpp — regression/test harness; not in-game acceptance
  src/game_launcher.cpp — host game launcher
  src/game_launcher.hpp — host game launcher
  src/game_launcher_tests.cpp — regression/test harness; not in-game acceptance
  src/in_game_replay.cpp — host in game replay
  src/in_game_replay.hpp — host in game replay
  src/in_game_replay_tests.cpp — regression/test harness; not in-game acceptance
  src/ingame_editor_server.cpp — host ingame editor server
  src/ingame_editor_server.hpp — host ingame editor server
  src/ingame_editor_tests.cpp — regression/test harness; not in-game acceptance
  src/locomotion_timeline_tests.cpp — regression/test harness; not in-game acceptance
  src/locomotion_trace_ui.hpp — host locomotion trace ui
  src/modern_main.cpp — host modern main
  src/modern_ui.cpp — host modern ui
  src/modern_ui.hpp — host modern ui
  src/modern_ui_tests.cpp — regression/test harness; not in-game acceptance
  src/monitor.cpp — host monitor
  src/playback_worker.hpp — host playback worker
  src/playback_worker_tests.cpp — regression/test harness; not in-game acceptance
  src/player_action.hpp — host player action
  src/replay_player.cpp — host replay player
  src/replay_player.hpp — host replay player
  src/replay_player_tests.cpp — regression/test harness; not in-game acceptance
third_party — vendored ImGui and MinHook source/licenses
  third_party/imgui — module/reference directory
  third_party/minhook — module/reference directory
  third_party/minhook_vendor — module/reference directory
tools — read-only research, capture inspection and isolated packaging
  tools/fidelity_capture — module/reference directory
    tools/fidelity_capture/analyze_animation_replay.py — research/capture/build utility
    tools/fidelity_capture/generate_schema.py — research/capture/build utility
    tools/fidelity_capture/inspect_capture.py — research/capture/build utility
    tools/fidelity_capture/README.md — research/capture/build utility
    tools/fidelity_capture/test_capture.py — regression/test harness; not in-game acceptance
  tools/ghidra_query — module/reference directory
    tools/ghidra_query/.gitignore — research/capture/build utility
    tools/ghidra_query/correlate_sdk.py — research/capture/build utility
    tools/ghidra_query/README.md — research/capture/build utility
    tools/ghidra_query/research_query.py — research/capture/build utility
  tools/native_replay — module/reference directory
    tools/native_replay/analyze_payload_capture.py — research/capture/build utility
    tools/native_replay/analyze_probe.py — research/capture/build utility
    tools/native_replay/build_ghost_prototype.ps1 — research/capture/build utility
    tools/native_replay/extract_static.py — research/capture/build utility
    tools/native_replay/ghost_fingerprints.py — research/capture/build utility
    tools/native_replay/payload_codec.py — research/capture/build utility
    tools/native_replay/test_payload.py — regression/test harness; not in-game acceptance
    tools/native_replay/test_probe.py — regression/test harness; not in-game acceptance
  tools/research_integration — module/reference directory
    tools/research_integration/analyze_runtime_trace.py — research/capture/build utility
    tools/research_integration/integrate_sources.py — research/capture/build utility
    tools/research_integration/README.md — research/capture/build utility
    tools/research_integration/test_tools.py — regression/test harness; not in-game acceptance
    tools/research_integration/verify_phase7_anchors.py — research/capture/build utility
```

# Phase5C status — diagnostic checkpoint

2026-10-05. **DIAGNOSTIC IMPLEMENTED — LIVE VALIDATION REQUIRED. LOCOMOTION DRIVER NOT VERIFIED.**

## Existing live evidence

User now confirms smooth in-game transform replay, recorded rotation and improved full replay. User also confirms the model slides instead of correctly walking. This overrides earlier notes' pending labels for those specific transform observations. It does not certify every pause/stop/equipment case or this new trace.

Current local game log includes roughly 120 Hz receive/60 Hz application, real changing transforms, and raw_requests=0. It contains no REPLAY_ANIMATION_REQUEST entries for the examined session. Therefore opt-in animation request execution/namespace rejection is not proven. No game process was launched or modified in this task.

## Implemented

- Game-thread read-only snapshot of 23 numeric and 15 integer candidate fields plus native recorder counters/oldest transform. Player reacquired each callback. No engine pointer survives callback.
- Candidates: placement/quaternion, derived horizontal/vertical speed, root motion, animation/HKS multipliers, turn/motion settings, request duration, movement/action/cancel/disabled flags, debug/controller/HKS flags, movement limit, grounded/fall state, pending/observed/idle animation IDs, native recorder presence/network flags.
- Existing v3 128-byte control protocol extended with TRACE_START=8, TRACE_STOP=9, TRACE_MARK=10. Marker is position[0] integer 0..6, other components zero; this is a command discriminator, never a player target. Reserved fields, version, sequence, timestamp and finite payload guards remain. No parser/clock duplication.
- Status flag bits: ready=1, replay_supported=2, trace_active=4, trace_supported=8, trace_error=16. New host understands existing ready/replay bits. Matching Phase5C DLL required for trace; older host receiving extended flags will disconnect safely, so use the supplied pair.
- START STATE TRACE / STOP TRACE / dropdown MARK PHASE / OPEN TRACE LOG in a small DPI-aware native development panel. Global F9 advances Idle→Walk→Run→Sprint→Roll→Jump→Idle while game has focus; registration failure is displayed. Existing global F6 stops everything. Existing main replay layout retained.
- Game callback performs fixed-size copies under try_lock only. Filesystem/diff formatting belongs to separate worker. Maximum diff poll 10 Hz; intermediate values coalesce. Copy lock skips counted. No giant memory dump or per-frame disk write. Normal mode has no trace allocation.
- Timestamped diffs and phase baselines, player lost/found, update gaps >250 ms/regressions, non-finite markers. Tiny float changes filtered: placement/derived speed 0.05, root motion 0.005, animation phase/request duration 0.5, other floats 0.01. Baseline is last emitted snapshot to preserve accumulation. Integer flags/IDs exact. Native recorder counters reported as raw units, not interpreted duration.
- Log appends `%TEMP%\TheaterModeLocomotionTrace.log`, flushes emitted diffs; file open/write errors stop trace and appear in game log/status. Crash can lose latest coalesced observation; no perfect recovery claim.
- Trace and replay/nudge sessions are mutually exclusive. STOP/disconnect cancels trace. Trace produces no game state writes and does not disable user controls.
- Normalized synthetic timeline tests exercise Idle→Walk→Run→Sprint→Idle, boundaries, backward seek, restart, stop, unload, missing v3 track and v2. Fixtures do not establish live gait mapping.

## Gate and unresolved driver

HKS source makes locomotion input/graph transition a concrete target. Pinned manipulator/graph access is opaque or private; no verified setter signature/ABI. A fake TEST WALK or arbitrary offset/pointer cast would not meet the request. No new WALK/RUN/SPRINT driver is exposed. Ranked hypotheses and exact bounded next experiment are in PHASE5C_CHARACTER_DRIVING_SYSTEMS.md.

No normalized locomotion capture/playback integration before the user-visible WALK gate. No animation phase/rate/root-motion forcing. Existing recorder/action v3 format, v2 loading, transform interpolation/SLERP, input lock, full playback, bookmarks, unload/switch and compact preview logic are preserved. Automated regression tests are not visual UI or in-game acceptance.

## Validation

- Windows x64 Release EXE/DLL build: PASS (MSVC + pinned Rust locked/offline).
- C++ CTest: **8/8 PASS**, including existing seven suites and new normalized timeline.
- Rust: **14/14 PASS**, including diff noise/invalid filtering and marker/protocol validation.
- Control IPC tests: synthetic named-pipe mock verifies trace capability/start/mark/stop, rejects invalid marker, and refuses replay/nudge during trace. Existing replay mock passes. No game/DLL loaded by mocks.
- Real existing v2 fixture `replay_2026-10-05_073848.erplay`: reader + first-five-seconds + full-duration mock PASS; 881 samples, 14.656 seconds, 2 chunks. This is a file/IPC regression check, not a new live game test.
- New trace in-game execution, UI visual/DPI review and WALK/RUN/SPRINT driver: **NOT VERIFIED**.

## Files and output

New: adapter/src/locomotion_trace.rs; src/locomotion_trace_ui.hpp; src/locomotion_timeline_tests.cpp; scripts/Build-Phase5C.ps1; three Phase5C notes.

Modified: adapter/src/lib.rs, control_protocol.rs, probe_runtime.rs; src/game_control.hpp/.cpp, game_control_tests.cpp, monitor.cpp; CMakeLists.txt. Cargo.toml/Cargo.lock, version guard, task signature, verified access path, original game/reference files remain untouched.

Output: `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase5C`. Matching executable/DLL plus three notes and BUILD_MANIFEST.txt. Previous Phase4/Phase5 packages retained. Branch remains phase4-in-game-replay-prototype; no main merge/push.

Next action: the single normal-play marked trace in PHASE5C_MANUAL_TEST.md. This is a buildable runtime checkpoint, **not completion of the desired visible locomotion milestone**.

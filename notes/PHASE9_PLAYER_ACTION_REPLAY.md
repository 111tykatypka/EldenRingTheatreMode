# Phase9 — player action replay Test1

Status: IMPLEMENTED — RUNTIME VALIDATION REQUIRED. This is a buildable controlled experiment toward faithful visual replay, not an exact recreation of the boss fight.

## Code changes

1. ReplayPlayer previously selected current_action from sparse events. For capture-schema1 recordings it now decodes the native selected TAE queue entry from AnimationTrack at the current playhead: read index, ID, play time, length, speed; native action bits from ActionTrackRaw. Availability/finite/range checks fail closed; missing continuous state does not invent an old action. Old replay formats retain their sparse event fallback.
2. Host still owns file/clock; the existing validated 32-byte ActionState uses existing 128-byte control packets. No Rust parser/extra replay clock, no protocol change.
3. Native AnimationLease previously suppressed every repeated animation ID. New recorded-cycle detector requests on ID change OR a recorded phase decrease beyond 0.1ms, preserving same-ID restarts/loops. Identical samples and small noise do not repeat requests. This is a detector, not permission to write engine queue phase.
4. Native request writes use only public CSChrEventModule.request_animation_id (SDK: override for next frame). Event/TAE owner must match current ChrIns; current player reacquired on callback. Owner/state failures stop playback and restore owned input/request state. IPC never writes game objects.
5. Log REPLAY_ANIMATION_REQUEST includes phase/length/raw action bits; once/sec REPLAY_ANIMATION_COMPARE reports requested/observed ID and timing difference before current callback request. Raw flag values are not injected as simulated inputs.
6. F6/disconnect/lease/player loss stop writes. Pending request restored only if still owned; animation already accepted by the engine is not magically undone. Recording/ERPLAY/launcher/transform interpolation remain intact.

## Exactness gaps

No native pose/blend buffers, behavior graph restore, semantic action driver, physics checkpoint, inventory/world rollback, item execution, HP/damage or boss simulation reconstruction is implemented. Native request may be overwritten by HKS, may change gameplay behavior and may not be a pure visual operation. Current captured phase is observed, not forced, so phase matching/scrubbing/speed-dependent animation are unverified. Do not claim an accurate reproduction until observed requests are accepted and visual comparison passes.

Existing 126s file includes HP=0, unknown world transition timing and separate character-queue losses; it cannot be assumed to contain every state needed for exact boss encounter reconstruction. Resting at grace and using items require a controlled playback environment and safe state restoration beyond this first animation experiment. No raw whole-structure/pointer copy is used as a shortcut.

## Build and verification

Build scripts/Build-PlayerActionReplay.ps1 (AMD64 Release, existing pinned dependencies/target guard). Output Phase9_PlayerActionReplay_Test1, previous Phase5/Phase8 builds preserved. Passed suites: CTest14, Rust30, Python8, separate DX12 smoke. CPU tests are not in-game evidence.

Read-only actual replay check: build/Release/replay-player-tests.exe --capture-real <replay>. It compares decoded animation ID/time with the native queue records for every actual sample; never applies player transforms. All 7555 real recorded samples passed bit-exact native ID/time decoding checks. Synthetic unit cases cover same-ID phase reset, repeat/noise suppression, invalid/unavailable samples, dense playhead seek and backward seek.

## Required first live experiment

1. Close Elden Ring and old host. DLL changed, so restart is necessary; never hot-swap a loaded DLL.
2. Launch Phase9_PlayerActionReplay_Test1/EldenRingTheaterMode.exe; use existing YAFSML launcher with this folder's TheaterMode.dll. Manual YAFSML users must replace its configured DLL PATH with the new path. Original game files remain untouched.
3. Load same save/map near recorded beginning. Open replay_2026-10-06_075252.erplay. Recorded first XYZ = (-0.0342532, 1.8364218, 3.7288363). Set playhead to 0 and use Inspector position to return near recorded start; same equipment/arm style. No automatic scene teleport. Current guard requires <=1 horizontal and <=0.25 vertical units.
4. Settings: Native duration = First 5 seconds; Experimental native animation requests + same-ID cycles = ON; existing-character replay = OFF; selected NPC only = OFF; playback speed = 1.0x. Use last stable transform mode for the flat starting location.
5. Start Play once; hands off movement input. Observe whether legs/body animate rather than sliding, facing/path follow, and Stop/F6 restores ordinary movement. Stop immediately if behavior is wrong; no full boss sequence yet.
6. Send what you visually saw. Logs: %TEMP%/TheaterModeGame.log and %LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log. I will compare REPLAY_ANIMATION_REQUEST/COMPARE and adjust the next native binding based on evidence. Do not interpret successful compiler/tests as animation acceptance.

Next stages depend on this result: if requests rejected/overwritten, verify the native dispatcher/ownership scheduling before further writes; if IDs accepted but phase drifts, investigate real animation timing/blend/pose APIs. Then controlled walking/rolling/attack clips, speed and stopping; items/grace/combat restoration separately. Goal remains exact actions; this checkpoint does not mark it complete.

Read-only log audit: `python analyze_animation_replay.py %TEMP%/TheaterModeGame.log`. Reports session-scoped requested/observed ID agreement and phase error for matching IDs only; not proof of visual pose fidelity.

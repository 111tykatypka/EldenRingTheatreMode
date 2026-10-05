# Phase 5 — smooth transform + recorded action foundation

2026-10-05. Branch: `phase4-in-game-replay-prototype`. **IMPLEMENTED — RUNTIME VALIDATION REQUIRED.** Main not merged/pushed. User reported that earlier in-game transform replay works but is visually unsmooth. That does not verify this new package.

## What changed

| Area | Implementation | Evidence / remaining gate |
| --- | --- | --- |
| ~301 samples | Default combo used TEST first5s; Controller intentionally sent FINISH at 5,000,000,000 ns. New real v2 has 301 samples <=5s. Default now FULL; optional 5/10s diagnostic modes remain. | Code/history + mock crosses samples301/800 and exact endpoint. |
| Clock | C++ ReplayPlayer remains sole clock/parser; dedicated requested120Hz waitable-timer worker advances it. UI33ms timer only observes copied state. Missed deadlines skipped, not bursts. | Offline worker ~119.8Hz with main thread blocked; NOT game rate. |
| Transport | Existing sample pipe plus existing duplex Control pipe, latest-state mailbox. Current control v3=128B; DLL accepts legacy v1/v2 controls; current host needs matching v3 DLL. Telemetry v2=104B; host can read legacy72B v1. | C++ isolated real-pipe mock; Rust protocol validation. No game-thread pipe I/O. |
| DLL interpolation | Previous/next targets only; one observed producer interval of latency, clamped **8.333–33.333ms**. Linear position, normalized shortest-path SLERP, action ID stepped at delayed timestamp. No extrapolation; endpoint hold,250ms freshness lease. | Rust q/-q/0/90/180/270/360 and bounded tests; new visible smoothness UNVERIFIED. |
| Input lock | Public local ChrDebugFlags.disabled_movement / disabled_secondary_actions. Save original bits + FieldInsHandle; set only owned bits while Playing/Paused; restore on next game callback after Stop/Finish/Error/disconnect/unload. | Bit-preservation unit test. Actual gameplay suppression UNVERIFIED. No global device/input patches. |
| Loading | Reacquire main_player each callback. If absent, replay writes stop and never automatically rearm. Retain only handle/original owned flags and retry restoration when player returns. Different handles do not receive old-owner writes. | Code safety; live reload remains manual test. |
| Unload/switch | STOP queued before destroying old Reader/Player; clears loaded metadata/state/bookmarks/preview. File preserved. Opening another replay is safe inactive replacement; no autoplay. | Source + lifecycle coordinator tests; native UI manual gate. |
| Bookmarks | Rebuild only on file/content change; preserve selection by timestamp, persist sidecar. Seek stops native writes first. | Core storage tests; native selection UI manual gate. |
| Flicker | Old panel did unconditional InvalidateRect and repeated LB_RESETCONTENT/SetWindowText on updates; preview painted directly. Trail grows toward end, increasing painting work. Replaced with compact100–190 DPI-scaled panel, offscreen bitmap/BitBlt, state-change invalidation, unchanged text/slider suppression. | Source root cause confirmed; no evidence of an end-boundary timer loop. UI_PREVIEW_PERF logs paints/s and percent; 0/50/90/99/100 and visible flicker still require manual verification. |
| Action capture | Same PostPhysics callback observes exact pinned public time_act.anim_queue[read_idx] (bounds<10), raw anim_id/play_time/length, behavior.animation_speed and action_request bits. Fixed32B copied action state, same seqlock snapshot as transform; no capture disk I/O. | Binding/API compile confirmed; current/last TAE queue interpretation HIGH CONFIDENCE; actual animation observations need fresh live recording. |
| ERPLAY | v3 typed CRC action chunks, sparse changes +500ms time observations; preserved52B transforms. v2 unchanged and no fabricated animation. | New serialization/order/enum/checksum/optional tracks/recovery/cursor tests + real v2 playback mock. |
| Action timeline | Sequential cursor, upper_bound on seek/backward/restart; state includes current_action and availability. Unknown/Idle/Walk/Run/Sprint/Turn/Jump/Fall/Land/Roll/Backstep represented. | Synthetic fixtures explicitly marked mock. Runtime semantic labels are Unknown except runtime default-idle-ID match. |
| Animation playback | Opt-in native UI checkbox, OFF by default. Public event.request_animation_id receives actual recorded raw ID once per ID transition; periodic sync observations do not retrigger continuous IDs. Saves/restores only an unconsumed owned pending field. | EXPERIMENTAL. ID namespace, acceptance, looping, interruptions, blends, animation after Stop unverified. Raw request bits are never replayed as input macros. |
| Root motion | Existing verified ChrIns_PostPhysics stays authoritative. Compare live-before with last applied target once/s. No root-motion/HKS multipliers, gravity/velocity, proxy sync flags, TAE queue indices or play_time writes. | Recorded root-motion effects may still fight physics/visual proxy. Need roll/jump visual evidence before additional writes. |

## Rates / real data evidence

Actual existing recording: `%LOCALAPPDATA%\EldenRingTheaterMode\replays\replay_2026-10-05_073848.erplay`: **881 samples,14.656s,60.0437Hz,2chunks,46,004bytes**, SHA256 `FCE2AB35AC5D64CBE4CE65305D557B40DADC5C5872CC045B70385D159FC1B987`. It is v2 transform-only. New host mock consumed its full14.656s and final881st sample. This uses no game DLL and is not a runtime acceptance claim. Deleted old4093-sample fixture was not fabricated or restored.

Earlier game logs had ~60 writes/s and35–41 sequence increments/s (includes control messages, so not exact replay-TX rate). The old UI16ms update path was frame-cadence dependent; native UI timer was never a reliable60Hz clock. New offline worker measurement:119.822Hz (120ticks over ~1s, main thread blocked). Actual **new** IPC transmit/receive, game-callback and apply rates remain UNMEASURED until the user launches Phase5.

Instrumentation once/s: CAPTURE_PERF(actual captureHz), host REPLAY_PERF(generationHz, successful replay exchangeHz), DLL REPLAY_PERF(receiveHz,callbackHz,applyHz), REPLAY_APPLY(previous applied position error), UI_PREVIEW_PERF(paintHz,percent). Animation IDs log only on transition. No invented CPU/gameFPS/overhead numbers; timer requests are not measurements. Native stats stop when replay becomes inactive.

## Tests / builds

Release x64 C++ tests **7/7**: playback-worker,action-track,erplay,replay-player,game-control,game-launcher,in-game-replay. Rust tests **12/12**: wire/action data validation,legacy formats,safety guards,shortest SLERP,bounded interpolation,lease/generation/time/distance behavior,owned input bits and paired transform/action atomic snapshots. Tests include:

- old transform-only files and new optional action track;
- sparse duplicate suppression, optional500ms observations;
- correct sequential action/seek/restart across transitions;
- unknown enums and regressing events with recomputed valid CRC;
- bad CRC, truncated action track, optional/required unknown track;
- torn action recovery keeping complete transform chunks;
- >800 transforms/full endpoint and old real881-sample file over isolated mock pipe;
- transport carrying action IDs/opt-in flags; STOP/disconnect/start displacement guards.

C++ host and Rust DLL build as AMD64 Release with current pinned dependencies and Cargo.lock preserved. Final staging script reruns both suites before copying final outputs. No original Elden Ring/reference files or proprietary assets modified. No injection/anti-cheat logic added; existing YAFSML launcher retained.

## Known limitations / truth labels

- **Animation track capture IMPLEMENTED; not live-validated.** New recordings must establish real useful IDs/time. Readable binding fields do not guarantee visual replay correctness.
- **Locomotion semantic mapping UNKNOWN** for Walk/Run/Sprint/Turn/Roll/Jump/etc. Enum representation exists. Raw animation replay can potentially reproduce those without a semantic table; no guessed IDs or velocity-derived fake recording.
- **Animation request EXPERIMENTAL**, off by default. A TAE ID may not equal the event override namespace. Loop continuation with local input suppression is not proven. Actual idle/walk/run/sprint/roll/jump replay is not claimed verified.
- No exact pose/phase/animation-rate writes. Stored time/rate are observations only. Animation may continue on Pause;0.5x/2x transform speed does not force animation speed. Seeking restores recorded action selection in the host, not arbitrary engine animation phase.
- Native seek/step/bookmark disables replay application; PLAY starts sample0. Full arbitrary in-game scrubbing still requires a safe seek protocol.
- No NPC/world/combat/camera replay; no save restoration or map relocation. Same area/character/equipment required;15-unit start and5-unit per-target step guards remain.
- Input restoration is game-callback based, not a global device toggle; a stopped game task cannot perform restoration until callbacks resume. Loading restoration is deferred without saving pointers.
- Sample pipe retains the original connection lifecycle; after closing/restarting host, restarting the game may be required. The manual procedure starts a fresh matching pair.
- Native UI rendering/resizing/DPI/near-end flicker acceptance and all new gameplay behavior need the consolidated manual test. No unsupported visual claims from unit tests.
- Recording streams bounded chunks. Reader transform index/action-event vector grow with length; no arbitrary total duration/sample caps, but not constant-memory multi-hour playback.

## Changed / new files

Phase4C (`c991220`): native input lock; transform replay/probe/runtime; host clock/worker/control/coordinator/UI/tests; PHASE4C_STATUS.

Research (`2ceb2f6`): `notes/PHASE5_PLAYER_ANIMATION_RESEARCH.md`, `notes/PHASE5_ROOT_MOTION.md`.

Action container/cursor (`4afb43a`) and capture/control/animation prototype (`3d072c8`): modified CMakeLists; ERPLAY_FORMAT; src/erplay.hpp/.cpp,replay_player.hpp/.cpp,monitor.cpp,game_control.hpp/.cpp,game_control_tests.cpp,in_game_replay.hpp/.cpp,in_game_replay_tests.cpp; adapter/src/lib.rs,control_protocol.rs,probe_runtime.rs,replay_runtime.rs,transform_replay.rs,local_input.rs. New src/player_action.hpp,action_track_tests.cpp,playback_worker_tests.cpp; adapter/src/player_action.rs; scripts/Build-Phase5.ps1; notes/PHASE5_STATUS.md,PHASE5_MANUAL_TEST.md. Exact build commit and binary hashes: BUILD_MANIFEST.txt beside the package.

## Output / next step

`C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase5\EldenRingTheaterMode.exe` + `TheaterMode.dll`; status/manual/research/root-motion/format docs alongside. Older Phase4A/Phase4B outputs preserved.

Run **PHASE5_MANUAL_TEST.md** in one session: fresh offline loader start, fresh18–25s action recording, same-location full replay/input/pause/stop, opt-in actual animation comparison, UI/bookmarks/unload/switch. Send the new replay and three logs. Only then label individual runtime features VERIFIED, map semantic IDs if justified, and decide whether animation override/proxy synchronization needs revision.

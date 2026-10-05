# Phase 4B — first in-game player transform replay

2026-10-05 — **IMPLEMENTED — RUNTIME VALIDATION REQUIRED**.
Branch `phase4-in-game-replay-prototype`; base `3761a7e`.

## Evidence boundary

User explicitly verified the Phase4A launcher: START ELDEN RING, YAFSML launch, DLL load, IPC CONNECTED, Player FOUND, normal game and no crash. Treat those as VERIFIED.

Read existing live logs: probe writes 950/972/982/1004/1052 changed X by 0.5, immediately read back the requested X, and retained it on subsequent callbacks. Y changed with physics; two probes completed 120 observation callbacks. These are **confirmed memory observations**, not user confirmation of visible model/Havok synchronization or replay.

Neither full transform playback nor Pause/Stop physics behavior is marked VERIFIED. No game/loader was launched during this implementation. Animation research/capture is deferred until the user confirms Stage 1. X/Z rendering/flicker is unchanged.

## Architecture preserved

`ERPLAY v2 → C++ ReplayPlayer/clock/linear interpolation/SLERP → control IPC v2 → Rust latest-request mailbox → ChrIns_PostPhysics → PlayerIns::local_player_mut → physics.position/orientation`.

Original 72-byte telemetry pipe, exact 2.7.0.0 profile/path/hash guard, task-registration signature and pinned Cargo dependency revision/lock are retained. The DLL does not parse ERPLAY or advance replay time. Mutable player references exist only in the game callback, before the existing immutable sampler borrow. No cached PlayerIns, offsets, input/HKS patches, velocity/gravity modifications or new injector.

## Host flow

PLAY with a connected game selects in-game playback. Without a game connection the existing offline viewer remains available. Opening a file never activates player writes. The replay UI displays `Transform: YES | Animation: NO`, and logs `ANIMATION_TRACK_MISSING` for v2 files. Recordings remain the existing real transform-only v2 format.

BEGIN sends sample 0 and waits for a game-thread applied sequence/session before starting the existing clock. Producer timer (16 ms request) advances that one clock; the pre-existing 33 ms UI timer only refreshes visuals. Actual IPC/render/game rate must be measured live; no 60 Hz performance claim is made. Latest-value buffering coalesces obsolete transforms, not a growing per-frame queue.

Default test endpoint is 5 replay seconds; user-selectable 10 seconds and full replay exist for staged testing. Duration is clamped to the actual file. This does not limit recording or format duration. Six existing speed choices remain. Initial runtime tests use 0.5x/1x/2x.

Pause holds the current transform while fresh packets continue; stability awaits the live test. Resume uses the same host clock timestamp. Restart waits for STOP acknowledgement before a new BEGIN/displacement check. Finish sends the exact endpoint transform once, then native writes OFF. F6/Stop clear pending targets, stop the host Player and send STOP. Opening/renaming/deleting the active replay or seeking/stepping/bookmark seeking disables native playback first. In-game seek is deliberately deferred; the next PLAY starts at zero. Recording commands are refused while native playback is active.

The recent-file list is refreshed only when its displayed rows change and preserves the selected file by path. Previously telemetry refresh cleared selection repeatedly, interfering with opening a new recording while the game was connected. This targeted file-control fix does not change X/Z preview rendering.

## Runtime guards

Native states: Inactive, Playing, Paused, Finished, Error. Inactive/Finished/Error perform no recurring transform writes. Reacquire the player each callback. No automatic rearm after player loss, loading, disconnect, malformed payload or unsupported runtime.

- Start displacement ≤15 Havok units, checked in host and again on the game callback.
- Finite positions and normalized quaternions; monotonic command sequence/session and replay timestamps.
- Producer timestamp freshness ≤250 ms for replay transforms. Producer time is recorded in the UI thread and is not refreshed by the IPC worker.
- Independent 500 ms transport heartbeat lease. A stalled UI cannot keep holding a player merely by sending heartbeats.
- Prototype discontinuity guard: >5 Havok units between successive requested transforms stops playback. These guards apply to playback safety, not recorder duration or file size. They may reject legitimate teleport/streaming segments; full cross-map replay is unsupported.
- STOP invalidates generations, and the callback checks generation immediately before writing. A write already executing cannot be undone; no API guarantees zero IPC latency. Normal input is never disabled.

## Physics evidence and remaining unknowns

Exact pinned `3c8c1d7633a99309fb004c9f894ea10b7967d0e0` exposes mutable physics position and Quaternion and documents `ChrCtrlChrProxyFlags::position_sync_requested` / `rotation_sync_requested` as copying the physics transform to the underlying Havok character. They are not written here: direct-write probes retained X, and a visual synchronization problem has not yet been confirmed. First test position/orientation only. If it fails visibly, investigate these documented flags as a separate runtime experiment.

The same pinned `position.rs` states Havok, block and global positions have different origins and need conversion context. Current v2 files store raw Havok coordinates without map/origin. Same area and coordinate origin are therefore a **manual precondition**; a small starting distance does not prove the same map. Warps/loading/region rebases/root motion/physics/input conflicts and visible yaw remain unverified. Animation/root-motion reconstruction has not been attempted; current live animations may slide or fight transform overrides.

## Protocol v2

Same local duplex control pipe, 96 bytes little endian. Offsets 0–63 preserve the old layout (version now 2): magic/version/kind, command sequence, producer boot timestamp, position[3], quaternion[4], probe state/detail, flags. Extension:

| Offset | Field |
| --- | --- |
| 64 | replay timestamp ns u64 |
| 72 | session u64 |
| 80 | replay state u32 |
| 84 | replay detail u32 |
| 88 | game-thread applied sequence u64 |

Kinds: HELLO=1, HEARTBEAT=2, PROBE_NUDGE=3, STOP=4, REPLAY_BEGIN=5, REPLAY_APPLY=6, REPLAY_FINISH=7, STATUS=0x8000. BEGIN requires timestamp zero/new nonzero session; APPLY supplies Playing/Paused and fully interpolated transform; FINISH supplies a final transform. STATUS flag bit0=runtime/player ready, bit1=replay supported. STATUS extension reports the actual game-thread state/session/applied sequence, distinct from receipt of a command.

New host requires v2/matching DLL. New DLL still supports old v1 64-byte probe/control requests and 64-byte replies, but v1 cannot issue replay commands. Unsupported/malformed version/payload cancels writes and closes the connection. No engine pointer is transmitted.

## Tests and fixture

Release C++ and Rust build. CTest 5/5 pass, Rust 8/8 pass at the implementation checkpoint. Pure Rust tests cover wire/legacy validation, invalid transforms, sequence/freshness, safe nudge, start distance/session/time/discontinuity, Pause/Resume/Finish-once, generation cancellation and stale targets.

C++ tests preserve prior ERPLAY/ReplayPlayer/launcher/probe coverage and add an isolated mock pipe coordinator test: first-write ACK before clock start, position interpolation/SLERP transmission, Pause/Resume timing, Stop ACK before Restart, exact 5-second endpoint/Finish-once, STOP/disconnect and distant start refusal. Synthetic data is used **only in automated tests**, never supplied as real gameplay.

The user confirmed deleting `replay_2026-10-05_022417.erplay` and requested a new actual gameplay recording. The replay folder was empty at the build checkpoint. The old fixture remains unavailable; its attempted integration test failed to open the file. The optional automated integration command is `build\Release\in-game-replay-tests.exe <actual .erplay path>`; it does not load a game/DLL.

### New real recording: validated 2026-10-05

The user completed recording with the staged Phase4B host. Host logs report finalization of `replay_2026-10-05_073848.erplay`: **881 samples, 14.656 seconds, 60.043668 Hz, 2 chunks**, 46,004 bytes. The existing ERPLAY Reader independently loaded and fully validated this new file in `in-game-replay-tests.exe`, including chunk CRCs, sample order, transforms and footer/header consistency. The isolated mock IPC test with this real file passed interpolation/SLERP transport, Pause/Resume, exact 5-second finish and STOP. **This is real-file integration PASS, not in-game transform replay verification.**

Additional read-only sample analysis: all transforms finite; no source/replay timestamp regressions; replay intervals 15–31 ms, no zero intervals or gaps above 50 ms. Quaternion norm squared ranged 0.999999770–1.000000251. Maximum consecutive position change was 0.065710 units; first 5 seconds contain 301 samples and maximum displacement from start 4.359115 units. Full recording has genuine movement (maximum displacement 5.827549 units) and orientation change (up to 177.927479 degrees from first orientation).

First sample Havok position: `(8.458280, 6.015267, -6.615375)`; quaternion XYZW: `(0, 0.5189684, 0, 0.8547934)`. File SHA-256: `FCE2AB35AC5D64CBE4CE65305D557B40DADC5C5872CC045B70385D159FC1B987`. Map/origin are still absent from v2; these numbers do not identify a map.

The first sandboxed test invocation failed at the isolated mock-pipe handshake before opening the real fixture. Repeating the same test outside the sandbox passed (exit 0, `REAL_FIXTURE_MOCK_PASS`). Neither invocation connected to the production control pipe or wrote to the game.

Current live logs confirm the new DLL profile `EldenRing_1_17` / 2.7.0.0, unique task signature, `ChrIns_PostPhysics` callback, main_player FOUND and READY. The running host path is the Phase4B output; generated YAFSML config selects its matching DLL. The user has been given the first-5-seconds visual test.

### Live callback evidence during the new-file test

Real DLL log sessions `26956062000001`, `26969687000002`, `26982484000003` subsequently show `REPLAY_START → REPLAY_STATE=1 → REPLAY_APPLY → REPLAY_STATE=3 replay_ns=5000000000 → REPLAY_FINISHED`. Host logs acknowledge FINISHED for all three. Initial starting displacement was 0.249371 units; subsequent starts from the 5-second endpoint were 4.359114 units. The native guard was retained.

Logged requested position and quaternion changed with the recording. On sampled following callbacks the reported error relative to the previous target was 0 to approximately 0.000000477 units, including during movement. This establishes **real game-thread writes and sampled memory persistence**, not visible character/collision correctness. Logs state position/orientation ONLY; no proxy sync flags, velocity or input patches were applied. A fourth session also began; its log timestamp progression was slower. No Pause/Resume/Stop or user visual result has yet been confirmed at this checkpoint.

The recording's first position change above 0.01 units is at **3.140 seconds**, and first orientation change above 1 degree is at **3.953 seconds**. The stationary beginning is recorded data, not a stall. Test at 1.0x; at 0.1x visible movement begins around 31.4 wall seconds.

Physical movement/yaw, Pause/Resume, Stop, normal controls after Finish and collision stability remain **RUNTIME VALIDATION REQUIRED** until the user reports their observations. No animation phase is authorized by this partial evidence.

## Output / next gate

Final package build on 2026-10-05 completed with CTest **5/5** and Rust **8/8**, with assertions enabled in Release tests. Both staged binaries have PE Machine **0x8664 / AMD64**. The only Rust warning is the existing `TheaterMode` crate-name style warning. No game process was running during packaging, and no new gameplay recording was produced by automated tests.

| Staged file | Bytes | SHA-256 |
| --- | ---: | --- |
| Phase4B/EldenRingTheaterMode.exe | 311296 | `EA20EA937DC1A9B40F94D398F6325C45A9B618B597B6B2DA0D99CE4FCF91DDEA` |
| Phase4B/TheaterMode.dll | 292352 | `0D419A8F0E894E498CBBA7342959DED49BC390DD3435A26B4B628A49D5E4C0D7` |

The verified Phase4A EXE/DLL hashes remain `2F89399B408E3938B680FA0B2E9D95612CCABB002C1B546831C0B4F7B4FA1D04` / `25E0CB1539C8E4DB3FA6AB7325844DD4D1EB718FFC2A63E76C90D262FA2F47E0`. The original installed YAFSML.ini still hashes to `51791F90771D4F9785E62AF80495645585F7E8DDEC0590BB733679C90514AF33`.

Package to `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase4B`, keeping the user's verified Phase4A outputs intact. `scripts/Build-Phase4B.ps1` builds both matching components, runs tests and copies this report/manual. DLL and EXE must be used together after a fresh game launch. See `PHASE4B_MANUAL_TEST.md`: make the requested new recording first; then first 5 seconds, user confirms physical path/rotation/Pause/Resume/Stop/no crash, then 10 seconds, then the full new recording. **Stop development at this runtime gate.** Only after the user's confirmation proceed to animation research, capture, a new replay with a real animation track, and locomotion playback.

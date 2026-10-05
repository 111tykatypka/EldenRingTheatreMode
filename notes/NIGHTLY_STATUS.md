# Nightly Research Integration — DEVELOPER EXPERIMENTAL BUILD

Target: Elden Ring 1.17 / AMD64 / file and product 2.7.0.0, exact disk SHA `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`.
Branch: `codex/nightly-research-integration`, baseline `20da8a6`.

## User runtime evidence is authoritative

NPC/boss trajectory recording and host visualization: **RUNTIME VERIFIED by user**.
Player transform replay: **PARTIAL**. Player grounding: **FAILED**.
NPC in-game transform replay: **FAILED**. Native locomotion/animation replay: **FAILED**.
These failures have not been converted to successes by this build. No live game was running during this implementation pass.

## Implemented and unit tested

- Explicit read-only, ten-second differential JSONL window. PreBehaviorSafe, PostPhysics before/after, immediate player write readback, NPC receive/gate/write stages, and subsequent callback observations.
- Fixed 8192-row bounded channel. Callback copies POD and uses `try_send`; file formatting/writes and 500ms flush happen on a dedicated worker. Full queue drops are counted. No per-frame JSON allocation in the new callback path.
- Position/quaternion, last physics position, model/physics model translations, vertical offset, ground/falling flags, fall timer, behavior root motion, animation speed, proxy request flag, native identity, session, target and command sequence. Unknown Havok proxy position and ground height remain `null`.
- **Replay selected NPC only**: host filters one track, uses the existing single clock/interpolation, and sends replay flag 2. DLL runs session/lease validation and acknowledgements but does not write the player or neutralize player input in this mode. Old DLLs without capability 64 cannot accept this mode.
- Host actor track/target diagnostics joined by session and native handle to DLL JSONL. Distinct lookup, identity, stale/target, budget/start-distance failures are exposed in the trace.
- Explicit selected-NPC `animationSpeed=0` experiment, two seconds, original captured float restored only on the exact reacquired character and behavior module. Stop/disconnect/timeout aborts; missing objects are never dereferenced through old pointers. **EXPERIMENTAL, RUNTIME VALIDATION REQUIRED**.
- Typed `CSLuaEventManImp` reflected singleton read-only observation: existence, script imitation warp ID/reentry and proxy load-wait flag. No warp calls or manager writes.
- Reusable exact-image source anchor scanner, machine-readable symbol matrix, streaming trace analyzer and tests.

## Safety decision from actual evidence

`offset_of!(ChrIns, debug_flags)` in the pinned SDK is **0x530**, but current FreecamMod documents the equivalent flags at **0x538**. The first cross-check test failed and exposed this mismatch. **No inferred correction to either layout is made.** Flag ownership experiments 1–4 are blocked. New nightly NPC playback performs transform writes only: no NPC debug/action-mask/neutralization writes. Player replay retains normalized action handling but its debug flag writes are blocked as well. This is a deliberate experimental behavior change; Phase5 remains untouched for comparison.

## Not implemented / unresolved

Grounding correction and proxy synchronization; production NPC ownership; visible native WALK/action replay; debug-camera input ownership experiment; camera control; actual map/world loading; native ReplayRecorder codec; world reconstruction; NPC spawning/health/equipment/VFX restoration; production ERPLAY04/checkpoints; in-game DX12 UI.

The master specification remains the roadmap. This is a diagnostic checkpoint, **not completion of the full scene replay**. New runtime results must determine the next safe writes.

## Build/test evidence

Release AMD64 EXE/DLL/probe are packaged separately in `Nightly_ResearchIntegration`.
C++ 11/11, Rust 22/22 and Python 3/3 automated tests passed during this pass; captured logs and binary hashes are in the package manifest. Mock transport tests are not Elden Ring tests. No FPS/performance percentages or new live capture size/rate are claimed.

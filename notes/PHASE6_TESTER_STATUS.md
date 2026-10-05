# Phase 6 tester checkpoint

Status: **IMPLEMENTED — RUNTIME VALIDATION REQUIRED**. This is an experimental multi-character **transform** replay, not verified full encounter reconstruction.

## What changed

- Existing Rust adapter, pinned `fromsoftware-rs` revision `3c8c1d7633a99309fb004c9f894ea10b7967d0e0`, CSTask callbacks, player path, host ReplayClock, interpolation/SLERP, YAFSML and ERPLAY are preserved. Cargo manifests/lock are unchanged.
- Nearby typed `ChrIns` observations use the existing `WorldChrMan::chr_inses_by_distance` collection. Default entry radius is 200, exit is 240; UI offers 50/100/200/custom. Requested sampling defaults to 60 Hz; actual rate is callback/transport dependent.
- Identity includes native handle, event entity ID, NPC param and local observational lifetime. An address is used only for current-object identity comparison, never sent or dereferenced after caching. Same handle + nonzero entity identity can survive pointer replacement during recording. Unseen identities expire after two seconds. Presence changes mean observed disappearance, not proven spawn/despawn/death events.
- Host suppresses unchanged actor transforms/actions with one-second idle heartbeats. ID/action/rate changes and animation-time wraps remain samples. Quaternions are interpolated with the existing host SLERP.
- Optional visual snapshots store actor model ID, available HP/maxHP, ground bits and raw item-use SFX context; local-player snapshots additionally store 22 equipment param IDs, weapon slots, arm style, gender/archetype and bounded native face data. Initial and changed snapshots are serialized; unchanged snapshots are suppressed. No inventory/save progression writes.
- A new control command carries current host-interpolated actor targets. The DLL applies them in the existing PostPhysics callback. IPC threads never access mutable game objects.
- Opt-in actor replay resolves existing actors by native handle and verifies event entity ID/NPC param/type every callback. It acquires only actors within 20 units of their recorded target and never writes to the local player through this path. No actor creation or resurrection.
- UI actor cursor refresh and playback read the reused indexed file stream under the same replay mutex. A concurrent host/UI read regression test covers this ownership rule.
- Experimental scoped debug/action masks and early normalized-request neutralization attempt ownership. Exact current object identity is required for restoration. Stop, generation changes, unavailable player/world, disconnect, invalid data and stale 250 ms target leases stop writes. The next available callback restores owned bits; a stopped game thread cannot perform restoration until callbacks resume.
- Capture resource budget (default 1024, configured 1..16384) also sizes actor ownership and IPC staging. Truncation/drop/rejection counts are visible or logged. This is a resource guard, not a guarantee that 16384 actors can be sampled or replayed at 60 Hz. Large batches that age past 250 ms trigger Stop instead of being given a fresh timestamp.
- Launcher permits selecting the game EXE and persists Unicode paths. The shared validator checks filename `eldenring.exe`, exact file/product version 2.7.0.0, AMD64 and the full original on-disk SHA-256. Installation-directory equality is no longer binary identity; no other build is accepted.

## Evidence and confidence

| Conclusion | Evidence | Confidence |
| --- | --- | --- |
| Public typed enumeration/handle lookup/mutable physics APIs exist | Exact pinned SDK source, compiled use in adapter | CONFIRMED |
| Radius capture and optional tracks can be serialized/read/validated | C++ and Rust unit tests | CONFIRMED for wire/storage; runtime unverified |
| Host clock drives both player and actor packets | Coordinator mock verifies session, timestamps, position, SLERP, pause and Stop | CONFIRMED for host/IPC |
| Existing matching NPCs will visually follow writes | Callback implementation exists; no new live experiment | UNKNOWN until tester confirms |
| Scoped action neutralization stops native AI ownership | Normalized request fields are typed; producer priority/AI behavior not proven | EXPERIMENTAL, UNKNOWN at runtime |
| Recorded HP, model, equipment and face fields reflect intended native state | Typed pinned source, explicit bounded serialization | HIGH CONFIDENCE; needs live comparison |
| Native replay/ghost APIs can reconstruct full encounter | Prior targeted Ghidra research finds structures; lifecycle and ABI unresolved | UNKNOWN; not called |

The partial Ghidra export is supporting research only. Previous targeted results and ABI warnings are in `GHIDRA_RUNTIME_RESEARCH.md`. In particular, no guessed ReplayManipulator vtable call, native ghost constructor, Havok proxy write or locomotion event ID is introduced here.

## Runtime instrumentation

`%TEMP%\TheaterModeGame.log` adds ACTOR_ACQUIRE, ACTOR_RELEASE and once-per-second ACTOR_REPLAY summaries: active actors, applied callback count, rejections, budget, queue drops and maximum pre-write positional correction. A low error does not prove model/Havok synchronization or correct animation. Existing player perf/grounding/locomotion diagnostics remain available.

## Verification

Release x64 host/probe/DLL compile. Rust suite: 17 tests. C++ suite: 11 configured tests, including optional snapshot roundtrip/dedup, CRC/recovery, Unicode, v2 character IPC with visual data, actor transport, host interpolation/pause/Stop and five-resolution ImGui draw construction. Named-pipe tests must run under a normal Windows process token: restricted sandbox peer-process checks fail; the unrestricted repeat passes.

Existing real replay `replay_2026-10-05_164335.erplay` was read and run to its full endpoint through a mock transport: 1034 samples, 17.216792600 seconds, 4 player chunks, 1,619,984 bytes. **No game or DLL was loaded for this test.** New schema snapshots/actor writes require a fresh recording using the matching tester pair.

No new live game sampling, NPC writes, visual animation, Stop restoration, game FPS cost or control lock has been validated in this task. See package manifest for final test/build results and binary hashes.

Actual serializer storage benchmark, **synthetic data only**, 60 seconds at 60 Hz with half the NPCs idle (1 Hz heartbeat), remaining NPCs moving, one initial visual snapshot each:

| NPCs | Bytes | Actor transform records | Generate + finalize + validate seconds |
| --- | --- | --- | --- |
| 0 | 187532 | 0 | 0.0118 |
| 5 | 1414116 | 10925 | 0.0690 |
| 10 | 2243996 | 18310 | 0.1069 |
| 20 | 4300332 | 36620 | 0.2071 |

These are raw optional-track storage measurements, not compressed encounter-size forecasts, game overhead, capture rates or IPC capacity. Actual field churn and actors in a loaded scene must be measured by the tester. Optional compression has not been added.

## Next engineering decision

Run TESTER_README in order: fallback player, nonlethal nearby-actor capture, first 5 seconds, Pause/Resume/F6, then 10 seconds/full duration. If native physics fields move but the model/proxy does not follow, use the request/actual diagnostics and exact pinned bindings to investigate synchronization. If native AI still wins, identify a safe native ownership API before adding more masks. These are materially different engine experiments; choosing writes blindly would risk the existing baseline.

NPC animation playback, exact locomotion/phase, spawning/resurrection, equipment/appearance restoration, VFX/projectiles, combat/damage and save isolation are not implemented. Those prevent claiming full multi-character encounter fidelity.

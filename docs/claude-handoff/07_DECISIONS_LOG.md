# Decisions log

Snapshot2026-10-06. Dates are Git evidence where available; early requirements have no reliable date. This summarizes decisions, not verbatim conversations.

## Original requirements (date unavailable)

Structured replay, not video or controller macro. Separate recorder, player and cinematic camera tracks. Reliability over file size; nondeterministic input replay rejected. Investigate actual interfaces before promises. Versioned format, streaming long recordings and indexed future checkpoints. Offline single-player, no game-file modifications, reference folders read-only, no proprietary copying. No arbitrary product limits/fake state/performance.

Exact2.7.0.0 guard shared by probe/runtime. Current pinned Rust SDK selected over stale SDK offsets; preserve verified CSTaskImp→PostPhysics→mainplayer. Mutable public APIs, reacquire each callback. Host owns parser/clock/interpolation, DLL only game integration/current-state application. Existing YAFSML remains loader; integrated launcher for one-place workflow.

## Baseline phases (2026-10-04/05 historical stage dates)

Real player sampling and streaming capture verified. Console-only F5 workflow rejected; native clickable controls/global hotkeys required. Offline viewer retained abstract compact X/Z rather than pretending game rendering. Flicker and actual six speed options prioritized. Runtime writes staged tiny nudge→hold→5s→10s→full; emergency stop and guarded start distance, no automatic cross-map teleport. Compilation not acceptance. Dedicated worker/interpolation improved Phase5; animation remained missing.

## Modern integration (2026-10-05/06)

DX11 standalone ImGui host over existing backend, DX12 injected overlay separate. Preserve Phase5 golden master. Insert/right-click crash led to instrumentation and render input reentrancy/capture fixes. First Insert transport mode intentionally lacks interactive cursor; Editor mode captures. F5 dispatch independent of ImGui keyboard focus. Random playback stops led to lease clock measurement after IPC snapshot; start mismatch investigated instead of removing guards. User later confirmed flat indoor movement, no falls/jerks, but sliding without animation.

Owner requested removing Pause/Resume UI buttons. Wheel alone zooms both directions; Shift+wheel scrolls tracks. Two-minute boss/grace/item capture prompted nine raw fidelity tracks. Audit proves captured bytes, not exact action reconstruction. Owner rejects remembering start position. Bounded scene-anchor ReturnStart added; invalid/rebased block coordinates still prevent universal automatic return.

## Native replay direction (2026-10-06)

Use native ReplayRecorder/ReplayData/ReplayManipulator/ReplayGhostIns instead of claiming transform/animation guesses reproduce all actions. Read-only SQLite/exact bytes/RTTI/lifecycle/scheduler proof before mutations. Offline session lacks natural bloodstain ghosts. Codec/reset/long-duration integration remain unresolved.

One-shot native create/remove default OFF, original execution context/factory/allocator/refcount/removal only. No manual gate/destructor/entry construction. One attempt/process. Add persistent noninteractive HUD because F10 had no visible acknowledgment. Startup context observed but command timed out waiting periodic builder. Latest3d97070 adds guarded post-original TestNetStep consumption, no timer/debug flag mutation; live acceptance pending.

Full Git-derived dated ledger appended below. A commit message is provenance, not runtime acceptance.

## Git chronological provenance ledger

Commit names are not proof of game acceptance.

```text
aee5afe 2026-10-05 Initial Phase 3 stable snapshot
1c947d5 2026-10-05 Initial commit
a4ad666 2026-10-05 Merge GitHub initial commit
bb3285e 2026-10-05 Phase 4: add safe player transform write probe
3761a7e 2026-10-05 Phase 4: launch existing YAFSML workflow from host
e2f2fb6 2026-10-05 Phase 4B: add guarded game-thread transform replay protocol
d727b8c 2026-10-05 Phase 4B: connect ReplayPlayer to guarded in-game playback
d578f47 2026-10-05 Phase 4B: document fresh real replay validation and live callback evidence
c991220 2026-10-05 Phase 4C: full replay, independent playback worker and scoped input lock
2ceb2f6 2026-10-05 Phase 5: research pinned player animation and root-motion interfaces
4afb43a 2026-10-05 Phase 5: add versioned player action track and indexed replay state
3d072c8 2026-10-05 Phase 5: capture native animation observations and opt-in transition playback
77508eb 2026-10-05 Phase 5: package build workflow and consolidated runtime validation checkpoint
dbcc315 2026-10-05 Phase 5: distinguish live animation observations from experimental replay requests
545a8fe 2026-10-05 Phase 5C: map native character driving systems and locomotion boundaries
9eb4cc1 2026-10-05 Phase 5C: add native locomotion differential tracer and marked diagnostic UI
81842b4 2026-10-05 Phase 5C: package diagnostic checkpoint and single-session runtime procedure
840cfda 2026-10-05 ui: add pinned ImGui DX11 docked editor over existing replay backend
61675d0 2026-10-05 runtime: add read-only nearby character capture and experimental local input diagnostics
f90b7da 2026-10-05 replay: stream optional character tracks into the modern editor with cancellable IPC
cb2c8a9 2026-10-05 build: package matching Modern Release artifacts and document the live validation checkpoint
4620098 2026-10-05 research: add targeted Ghidra queries and guarded read-only runtime evidence
a327a85 2026-10-05 runtime: add guarded existing-character replay and sparse visual snapshots
0fad360 2026-10-05 build: package matching multi-character tester build and manual acceptance plan
d14e5c4 2026-10-05 build: restrict offline attribution metadata to Windows target
20da8a6 2026-10-05 fix: serialize actor cursor reads with playback and retain legacy IPC coverage
ba8be9f 2026-10-05 research: cross-check camera animation warp anchors and reject debug layout conflict
f6fd2b9 2026-10-05 runtime: add bounded differential trace selected NPC replay and guarded speed probe
eb11f66 2026-10-05 build: package separate research integration nightly with explicit runtime validation plan
b4500de 2026-10-05 Phase7: guard replay starts and add DX12 editor runtime validation checkpoint
ed9bc1a 2026-10-06 Phase7: isolate startup rendering and instrument reported game crash
af97bd3 2026-10-06 Phase7: fix mouse input reentrancy and preserve game window capture
2ba304f 2026-10-06 Phase7: dispatch recording hotkeys regardless of ImGui keyboard capture
30170d3 2026-10-06 Phase7: sample replay lease clock after IPC snapshots
945485f 2026-10-06 Phase7: simplify playback controls and reserve timeline wheel for zoom
dda77f6 2026-10-06 Phase7: scroll timeline tracks vertically with Shift and wheel
bc556d1 2026-10-06 Capture fidelity: retain nine raw player state tracks with validated streaming storage
984c047 2026-10-06 Validate real fidelity capture and accept its exact producer version
2d87ad3 2026-10-06 Player action replay: decode continuous native state and retrigger recorded cycles
6432477 2026-10-06 Replay start: add bounded scene-checked preparation and return control
22420a1 2026-10-06 Native replay: exact-build research and isolated read-only recorder/ghost probe
c047350 2026-10-06 Native replay: verify live recorder pool and correct manipulator owner diagnostic
5e3ba5f 2026-10-06 Native replay: decode payload structure and add read-only action cadence probe
697d420 2026-10-06 Native replay: identify lifecycle gates and precise safe-spawn blockers
ccc3d1b 2026-10-06 Native replay: add isolated one-shot native ghost lifecycle prototype
843c482 2026-10-06 Native ghost: show persistent command status and wait countdown in game
3d97070 2026-10-06 Native ghost: consume create on guarded native step completion
```

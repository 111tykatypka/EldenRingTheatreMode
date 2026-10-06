# Roadmap and bugs

Snapshot2026-10-06, implementation3d97070. Native prototype implemented but runtime validation required.

|Milestone|Status|
|---|---|
|Real player reads/streaming recording|Done for verified baseline|
|Offline viewer/interpolation/timeline|Implemented, historical CPU/manual validation|
|In-game player transform movement|User verified bounded flat area; animations incomplete|
|Full multi-character encounter replay|In progress, not delivered|
|Native lifecycle static analysis|Substantial bounded static evidence|
|Native Step create/remove|Implemented, latest user test required|
|Native actions/durable codec/seek|Blocked on lifecycle and codec proof|
|Automatic arbitrary start/world restore|Blocked on scene/block/Havok conversion|
|Cinematic camera tracks|Todo/research|

## Bugs and reproduction

1. **CREATE timeout:** HUD checkpoint, load world/walk/F10once; context_seen1 then60s error. Latest Step consumption fix not tested. Collect new counters before changing execution context.
2. **Sliding:** transform replay follows recorded path without animations. Native action/locomotion ownership unresolved; request ID alone not full solution.
3. **Start mismatch:** raw XYZ after block-origin rebasing can reject same physical place or imply unsafe teleport. Real126s capture had originFFFFFFFF and endpointdelta(-8,+8,-96). Capture/reference coordinate conversion needed.
4. **Action fidelity:** boss/grace/items captured but raw observations are not restore commands. Native codec/state and ownership needed.
5. **Historical regressions:** right-click crash/F5ignored/premature lease stop had fixes; revalidate on UI/thread changes. No blanket driver/DPI acceptance.
6. **Recorder count check:** readiness uses capacity+40/headnonnull; actual count+44. Audit with live values before treating as proven defect.
7. **Lifecycle/resource acceptance:** no successful native create/remove, no measured leak test, no safe hot unload.

## Next five concrete tasks

1. **One Step build live create/remove test.** Exact guard/YAFSML/on-foot safe loaded world/recorder ready. F10once, wait actual OWNED or error; F11 only after ownership. Require native activation/entryclear/DelayDelete/destructor evidence and normal control.
2. **Close measured runtime context/ABI/lifecycle gaps.** Depends on task1 logs. Read-only instrumentation first; if genuine callback stops, fail closed, no generic-thread spawn. Isolated build, one controlled test per fix.
3. **Native Idle→Walk→Stop prototype.** Depends on successful lifecycle. Observe decoded cursor/time/control/animation and automatic end cleanup. No boss replay until native movement/animation visual acceptance.
4. **Durable native payload track and decoding.** Depends on opcode/keyframe/reset/sampling/ownership proof. Versioned typed storage, bounded streaming/schema/CRC. Do not concatenate blobs or increase native capacity blindly; single host clock preserved.
5. **Repeatable gameplay acceptance plus coordinate-safe start.** Depends on native playback/scene/block/Havok references. Start5s/10s then full replay, progressively include available actors/actions. UI can integrate via commands independently; cameras/world restore remain separate gates.

Never claim exact encounter reconstruction or release readiness without real owner testing. Preserve Phase5. No fake performance or disabled safety checks to make a demo pass.

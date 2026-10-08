# Cinematic editor pass — checkpoint 1

Date: 2026-10-07. Branch: `codex/cinematic-editor-pass`. Parent: `194d68f`.

This is an incremental diagnostic/foundation build, **not the complete cinematic editor**. Preserved P2d/P2e packages, original Claude project, recordings, game installation and reference binaries are unchanged. The Enemy test root-coordinate rejection remains unresolved; no NPC replay redesign is included.

## Inspection findings

**STATIC_VERIFIED:** The actual F4 overlay is `native_ui/theater/TheaterOverlayUI.cpp`, not the larger unused view-model prototypes in `TheaterUITypes.h`. Camera, Look and Export were unavailable placeholders. Host `ReplayPlayer` owns time; editor snapshots carry the source-time anchor and master-clock timestamp. Camera math must evaluate a supplied sequencer timestamp, not start a second playback clock.

**STATIC_VERIFIED:** DX12 backend hooks Present/ResizeBuffers/swapchain creation using MinHook. Theater draws before forwarding to the previous Present implementation, with its own command allocator/list, descriptors and fences. This does not establish ReShade ordering. No scene color capture, post-processing pass, depth acquisition, or deterministic game-render acknowledgement exists.

**STATIC_VERIFIED:** Pinned SDK `crates/eldenring/src/cs/camera.rs` exposes `CSCamera`, four `pers_cam` pointers, `camera_mask`, `CSCam.matrix`, raw FOV, aspect and clip planes. SDK debug UI independently displays these fields. Camera matrix rows are interpreted by SDK helpers as right/up/forward/position. This is structure evidence, not proof of the currently rendered view, FOV units, origin basis or safe write phase.

CameraTools exports inspected: `research/igcs-camera-v1018/REFERENCE_MEMORY.md`, provenance and existing `notes/REFERENCE_CAMERA_TIMING.md`. Managed client uses two named pipes and exposes smoothing/shake settings. Native camera AOBs remain leads, not a proven write/restore contract. The reference-memory note's timing slot `0x358DB58` is stale; the later independent report corrects it to `0x458DB58`. No reference code or binary is distributed in this package.

## Implemented camera diagnostics

**COMPILE_VERIFIED:** `adapter/src/camera_probe.rs` reacquires the SDK singleton on the existing game callback. Scalar/pointer/matrix reads use checked `ReadProcessMemory` copies and field offsets derived from the pinned types. No camera object reference is retained, no game camera values are written, no owner/mask is modified. Compile-time assertions check the read layout and C ABI size.

The probe defaults **OFF**. Enable it in F4 → Camera → **Enable experimental camera reads**. Four candidate slots appear with matrix, position, raw FOV, aspect/clip data and validity. Display age uses the same QueryInterruptTimePrecise domain as the adapter. Finite/projection/basis plausibility checks reject invalid copies. Plausibility is not proof of correct semantic interpretation.

The game callback publishes a fixed-size 352-byte copy through a nonblocking try-lock; it skips publication if the render-side reader is holding the lock. The render thread cannot mutate game objects. Logs emit one summary plus four slot records every two seconds while enabled, not every frame. Turning OFF stops subsequent reads; prior displayed data is hidden. Existing input, replay, timescale, DX12 resource ownership and pipe protocol are preserved.

**UNKNOWN:** Runtime singleton availability, active slot, FOV units and render consistency. No live camera test occurred. The default-off probe is deliberately not persisted until this validation is complete.

## Camera path foundation

**COMPILE_VERIFIED:** `shared/CinematicCamera.h` provides:

- Nanosecond key times and stable key IDs; normalized XYZW quaternion state.
- Position/FOV interpolation with shortest-path quaternion SLERP.
- Per-segment Linear, Smooth, Bezier, timing Curve, centripetal Catmull-Rom Spline and Step.
- Relative Bezier handles; reflected spline endpoints and repeated-point handling.
- Optional approximate constant speed using a 128-subdivision per-segment arc-length table rebuilt on edits.
- Ordered random-time evaluation without a local clock; exact key/end boundary behavior.
- Invalid state, duplicate ID/time and unsupported interpolation rejection.
- Integer rational output frame scheduling in `[start,end)`, independent of timescale/wall time. A ten-second 60 FPS range yields 600 timestamps.

These are callable/tested components, **not wired to a native camera or authoring UI yet**. No K shortcut, path visualization, gizmos, camera cuts or camera project save is claimed. `Curve` currently describes scalar time remapping, not a complete editable graph. Constant speed is approximate; adaptive error-bounded arc sampling remains future work. No SQUAD, target/bone tracking or shake is implemented.

## Other subsystems / remaining blockers

| Subsystem | Evidence and current limit |
| --- | --- |
| Player/Free/Dolly ownership | UNKNOWN. Native camera remains under game control; no transform override or restore contract implemented. |
| HUD | UNKNOWN complete native hide path. Clean F4 preview only affects Theater UI; do not call it full game-HUD suppression. |
| Custom lights | STATIC_VERIFIED RTTI leads `GXPointLight`, `GXSpotLight`, `GXLightManager`; factory, render-thread affinity, shadow ownership and destruction UNKNOWN. No fake light controls added. |
| Look / LUT | Existing shader research identifies converted grading/DOF artifacts; exact CPU owners and resource contract UNKNOWN. No LUT parser/application or native grading implemented. |
| ReShade | Existing hook-chain analysis only. Depth selection, overlay ordering, resize/input coexistence UNKNOWN until joint runtime tests. No ReShade dependency added. |
| Export / FFmpeg / NVENC | Timestamp scheduler implemented; GPU readback, applied-frame acknowledgement, FFmpeg process lifecycle and encoder runtime probing NOT IMPLEMENTED. A scheduler alone is not an exporter. |
| Audio / HDR | UNKNOWN. No deterministic audio capture or HDR export claim. |
| Project format | NOT IMPLEMENTED. Do not write editor state into the original ERPLAY/world data. |

Recommended sidecar contract remains versioned `.theater`: replay hashes for both ERPLAY and world companion, stable editor IDs, time units explicitly nanoseconds, quaternion XYZW, named camera tracks/cut track, capability-gated light/look tracks, relative LUT/preset references and export configuration. Host owns disk I/O, strict validation and atomic replacement. Unknown versions must fail explicitly. This is a design, not a serializer delivered here.

## Tests and build

- Release AMD64 native host/overlay: passed.
- CTest: 13/13 passed, including new camera evaluator/telemetry suite and existing recorder/replay/editor tests.
- Rust: 49 passed, one optional real-recording inspection ignored; final rebuild uses the same locked/offline pinned SDK.
- No Elden Ring launch, visual acceptance, ReShade test, GPU capture or encoding test in this session.

Run `scripts/Build-CinematicCheckpoint.ps1` to rebuild and stage the matching experimental host/DLL at `outputs/Cinematic-C1-camera-probe`. The script refuses to overwrite an existing package.

## Exact next manual test

1. Stop recording/playback; close Elden Ring and the running Theater host. DLL replacement requires the process to exit.
2. Use the new package's EXE and DLL as a matching pair. Point the established YAFSML module entry to the new **absolute TheaterMode.dll path**; leave the game files unchanged. Do not load CameraTools simultaneously.
3. Launch the new EXE first, then launch through its existing configured YAFSML workflow. Load a safe single-player playable location. Do not start Enemy test playback for this probe.
4. Verify normal gameplay with the probe OFF. Press F4, select Camera, enable experimental camera reads.
5. Keep the character stationary; orbit the native camera, change its distance/aim normally and observe which slots follow. Capture the four slot values/mask. Check for sensible update age and no slowdown.
6. Disable the probe; close/reopen the overlay and verify normal camera/gameplay. No camera restoration is needed because this checkpoint never overrides it.
7. Send `%TEMP%\TheaterModeGame.log`, `%TEMP%\TheaterModeRender.log` and a screenshot of Camera diagnostics. If any regression occurs, close the game and return YAFSML/host to the preserved P2e1 package.

Next implementation checkpoint: correlate those observations with the exact camera-copy/render hook to prove active ownership, FOV conversion, coordinate basis, a short reversible override, and generation-aware restore. Then wire Free (unscaled real-time input) and Dolly (authoritative sequencer evaluation). Independent lights/Look/export work may continue, but unsafe native APIs must remain unavailable.

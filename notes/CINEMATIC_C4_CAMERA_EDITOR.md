# Cinematic C4 — camera editor and rendering research

Date: 2026-10-07. Branch: `codex/cinematic-editor-pass`.

## What changed

The independent camera backend now has Default, Free, Dolly and player Bone modes. F3 cycles modes and explicitly attempts to arm the selected camera; mode selection also has a dropdown. Arming still requires the exact game profile, a fresh camera output, offline player and connected host. Dolly requires keys for the loaded replay. Bone requires a valid selected bone sample. F6 disables writes. Free input is WASD/QE, mouse/arrows, ZX roll, Shift fast and Ctrl slow; hide F4 overlay to navigate.

Movement speed, mouse sensitivity and optional exponential velocity damping are editable. Damping uses unscaled monotonic time, so replay speed does not change manual camera movement. Default damping is zero. Rotation remains direct. Focus checks accept the foreground root window only within the current game process; unrelated applications still release camera control. Switching an already armed Free camera to Dolly preserves its current pose for the path-start distance check.

Independent camera shake has position amplitude, rotation amplitude and frequency controls. It is deterministic sinusoidal motion evaluated at ReplayTime, does not advance while the replay is paused, does not accumulate into the editable pose and is not saved into node transforms. This is our own implementation, not CameraTools' unknown noise formula. Shake/settings are session-only. Numeric phase overflow fails closed.

Each Dolly key has editable time, position, XYZW quaternion, FOV, outgoing interpolation, Bezier handles, ease values and constant-position-speed option. Apply validates the entire replacement before committing; duplicate times/IDs, zero/nonfinite quaternions, invalid FOV and times beyond replay duration are rejected. Valid quaternions normalize on commit. Revert restores a draft from the current key. Individual deletion and confirmed L deletion remain available. Timeline diamonds are unchanged.

Save/Load writes `<replay>.ercam` beside the selected replay. It uses UTF-8 identity with Windows Unicode filesystem APIs, 17-digit double output, classic numeric locale and a versioned `ERTCAM 1` header. Save writes a temporary file and replaces the sidecar with `MoveFileExW`; no original recording is modified. Import rejects mismatched replay path, unsupported version, truncation, trailing garbage and invalid keys. Loading disables writes. The 16 MiB import guard is an untrusted-file resource limit, not a recording-duration limit. Identity is currently the replay path, **not a replay content hash**; replacing a recording at the same path cannot be detected yet. Renaming a replay needs an explicit sidecar migration in a later version. Settings and free-camera bookmarks are not persisted yet.

## Bone camera data flow

`Draw_Pre -> existing bone replay application -> current main_player reacquisition -> checked bone count -> current model-space hkQsTransform + ChrCtrl.model_matrix -> copied stack arrays -> native camera target`.

The reader runs only when a bone index is selected. It reuses existing profile offsets and SDK model-matrix fields. No player pointer is retained, no actor state is written by this reader, and no importer pointer crosses the FFI. The native backend validates a rigid affine root matrix, finite pose and nonzero quaternion, composes world position/orientation and applies an editable bone-relative offset. Missing/invalid targets clear availability; stale targets release the camera after 250 ms. Index -1 disables reads. There is no guessed universal head index. Current implementation targets the local player only; actor selection and bone-name lookup remain missing. Model-root coordinate alignment, head motion, replay seek behavior and visual smoothness remain runtime-unverified.

## Actual log evidence supplied by user

The supplied log contains `CAMERA_COPY_HOOK=1 RVA=0x681970` and multiple `enabled=1 writing=1 mode=1` records. Requested positions and FOV changed, including FOV 48.4 and 49.8 degrees. The end of the log shows `Game focus lost; native control restored`, with writes off. These establish hook operation and successful write API results, **not visual correctness or final camera smoothness**. Background Draw callback cadence near 22 Hz is not a measured GPU FPS value.

Additional diagnostics now report callback count, mutex skips and mean hook work in microseconds. This measures our work after the original copy, not total game/render time. The game callback uses try-lock; contention can still skip an override and expose a native frame. This is instrumented, not claimed fixed by reducing refresh. Arc-table rebuilding for explicit edits/loading and file I/O occurs outside the game mutex; capture-key/view copies still share that mutex. Gamepad input is not blocked.

## Tests and package

The first C4 build passed 14/14 CTest and 49 Rust tests (one optional real-file inspector ignored). Further edits require a fresh final build; see the final package manifest for exact executable hashes. Tests cover camera conversions, bone/root composition, path interpolation/SLERP/constant speed, sidecar round trips and malformed imports, Unicode identity, and CPU LUT parsing/interpolation. They do not substitute for game testing.

Final package: `outputs/Cinematic-C4-final`. Its EXE and DLL must be used as a matching pair with the existing YAFSML workflow; the running process cannot acquire new code until restarted. No automatic loader configuration or running-game replacement is performed by this pass. F3 cycles Default/Free/Dolly/Bone; F4 shows/hides the editor; K captures the current camera at replay time; L prompts before deleting all nodes; F6 stops overrides. The Camera panel contains the editable controls and Save/Load. New features remain runtime-unverified; no claim of a completed native lighting/post-processing/particle system is made.

Preserve Phase5, P2d, C1, C2 and C3. No game installation files, reference binaries or loader were changed. No proprietary code is included in the implementation. New code is **IMPLEMENTED — RUNTIME VALIDATION REQUIRED**.

See `CINEMATIC_RENDERING_RESEARCH_C4.md` for lights, LUTs, post-processing, ReShade and particles.

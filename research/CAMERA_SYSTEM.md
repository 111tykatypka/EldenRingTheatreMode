# Camera subsystem РІР‚вЂќ C6

## Responsibilities

- CinematicCameraRuntime (native_ui): game-specific CameraTools-site native write interception; real-time Free movement; matrix conversion/validation; input ownership; fail-closed release; camera mode controller. Hook initialized only after exact Rust GameProfile acceptance.
- shared/CinematicCamera.h: pure spline/Bezier/SLERP/arc-length evaluator, no game memory.
- shared/CameraProject.h: .ercam ERTCAM1 replay-bound text serialization.
- shared/CameraCutTrack.h: validated hard cuts at ReplayTime, Player default in gaps. Current UI/runtime chooses one Dolly track, session-only.
- shared/TheaterHotkeys.h + InputBindings.cpp: persisted shared single-VK bindings and pure input decoder. Existing backend owns keyboard/raw mouse suppression; global Start/Stop registered by host.
- EldenRingTimingAdapter: separate native CSFlipperImp scalar. Automatically enabled on Play, with ownership/gating/restoration. No writes at inactive startup.

CameraTransform is positionXYZ, normalized quaternionXYZW, FOVdegrees. Roll is quaternion orientation, not an independently accumulated Euler override. Native matrix encoding and exact copy-phase provenance are detailed in CAMERATOOLS_DEEP_ANALYSIS.md.

## Controls

F3 cycles Player/Free/Dolly; K captures current key; L asks before deleting all. F4 hides editor to navigate. WASD horizontal, E/Q vertical, mouse/arrows look, Z/X roll, Shift fast, Ctrl precision, +/- camera speed, numpad +/- FOV, numpad* FOV reset, numpad2 roll reset. Defaults can be rebound in Settings. F6 stops recording/playback/camera. Only single keys are supported; modifier chords, controller suppression, world pause, HUD toggle and native state slots are not finished.

The .ercam editor supports key timestamp, position/quaternion/FOV, interpolation, Bezier offsets/ease/constant-speed. Numeric edits require Apply; there are no 3D translation/rotation gizmos yet. Path belongs to a replay, loading does not arm writes. Dolly starts beyond20 units are blocked for prototype safety. Render callback stats include lock skips so missed ownership frames are measurable.

## Experimental package / validation procedure

New package: outputs/Cinematic-C6-reference-backends within this independent checkout. Original/stable packages remain unchanged. Use matching EXE and DLL; no injector replacement.

1. Close Elden Ring and the previous Theater host before replacing/loading DLLs; the currently installed native hook cannot safely be hot-unloaded.
2. Point the existing YAFSML DLL entry at this package's TheaterMode.dll, start matching EXE, launch through the existing launcher/offline workflow, then enter a familiar loaded area.
3. Open F4 Camera panel. With writes OFF, rotate the native camera and inspect read-only matrix/FOV telemetry. Do not run IGCS simultaneously (ownership conflict).
4. Use the two-second +.25 X camera probe; observe movement then return to native camera. F6 is emergency stop.
5. Enable Free; hide F4, move/look/roll; switch Player and verify native camera/FOV return. Then test FOV separately.
6. Load a replay from the same area, start playback, world timing automatically follows the sequencer rate. Test1/.5/.25/.1/.05 separately, observing player, an enemy, animation, physics and menus. Stop must restore normal speed. Only after these pass try .01/.005/.001, then2/5/10. Native range stability UNKNOWN.
7. Confirm Free remains responsive at low world scale. Native pause+Free cannot yet be validated because no world pause patch is implemented.
8. Create Dolly keys with K at master timeline points, edit/save/load them; seek must evaluate the same pose. Cuts are session-only, Player/current-Dolly; enable cuts and arm separately.

Logs: %TEMP%/TheaterModeGame.log (CAMERA_RUNTIME and WORLD_TIMING diagnostic lines), %LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log, plus package BUILD_MANIFEST.txt. Send relevant transitions and visual observations, not just successful compile messages.

All new native features remain RUNTIME_VALIDATION_REQUIRED. These instructions are a staged checklist, not a statement that those tests happened.

## Native backend replacement

The 0x681970 post-copy hook and its discovery signature are removed from active source. New exact-profile interception RVA0x3BB458, continuation0x3BB48D; all53 original bytes are checked before hook installation. The independent MASM bridge preserves flags, volatile GPRs/XMM0..5 and Win64 stack alignment around the C++ evaluator. Inactive/invalid state jumps to the MinHook original trampoline; successful override retains native aspect/clipping copies and skips only matrix/FOV writes. Normal original copying restores the camera when overrides stop. The frame evaluator remains driven by this copy interception, not an IGCS Present-loop clone; behavior at near-frozen native timing requires live validation. No safe hot-unload is provided.

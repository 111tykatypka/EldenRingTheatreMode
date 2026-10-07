# C3 — native camera experiment

Date: 2026-10-07. Branch: `codex/cinematic-editor-pass`. Supersedes C1/C2 camera availability statements. **IMPLEMENTED — RUNTIME VALIDATION REQUIRED.**

## Static evidence

Exact executable SHA-256: D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134.

**STATIC_VERIFIED:** Public FreecamMod reference in `research/community-nightly/FreecamMod` uses a camera-copy signature. A read-only scan of executable sections in our hashed analysis copy found exactly one match at RVA **0x681970**. Capstone disassembly verifies function entry, camera fields +0x10..+0x5C, three destination pointers +0x18/+0x20/+0x28, conditional source selection through byte +0xC4, and RET at +0x10E. The Ghidra index identifies one caller at preferred VA 0x140B03A30. Its prototype uses three arguments; the disassembly supports the camera-copy operations, rather than relying on inferred pseudocode alone.

The reference's FieldArea +0x20 GameRend relationship and mode +0xC8 are not treated as SDK layout proof. The implementation obtains the copy function's actual owner argument and reads the +0x20 output it just wrote. **No FieldArea pointer chain, debug camera owner/mode fields, source-camera patches or guessed singleton offsets are used.**

Reference GUI labels native FOV as radians. SDK basis helpers support the matrix layout. The experiment therefore accepts only proper orthogonal bases, homogeneous W near one, plausible aspect/clip values and FOV in (0, pi), converts orientation to normalized XYZW quaternion and FOV to degrees. Semantic FOV conversion and the visible active output remain **UNKNOWN** until tested in game. Another valid camera representation is rejected instead of guessed.

Static export: `.research/camera-static.json`. Reference code/binaries are not included in the package; runtime implementation is independent.

## Native backend

`native_ui/CinematicCameraRuntime.cpp` installs a MinHook detour only after the existing exact runtime identity guard and render initialization. Rust additionally requires unique signature at the exact target RVA; C++ verifies the entry bytes again. Unsupported/missing/modified signatures leave camera control unavailable.

Original native copy always executes first. Experimental overrides write **only** 64 matrix bytes and four FOV bytes into the just-observed +0x20 output on that same callback. Reads/writes use checked Windows memory-copy APIs. No camera pointers are dereferenced from the UI, and retained pointer values are used only as generation comparisons against the current callback, not to restore into old objects.

Writes default OFF. A two-second real-time probe applies +0.25 camera X and automatically releases. Full Free/Dolly writes require a separate explicit UI checkbox. F3 selects Default/Free/Dolly; armed Free uses QueryInterruptTimePrecise elapsed real time; Dolly evaluates the supplied host ReplayTime/anchor/speed and existing shared path evaluator. The DLL does not parse ERPLAY or create another replay clock.

Default/F6/disable/host loss/player-or-offline-context loss/focus loss/output generation change/invalid data stop overrides. Restoration allows the **next normal copy** to provide the current native matrix/FOV; it does not write a stale saved camera back. Input ownership clears immediately. A C++ exception in experimental processing latches the backend off; native original continues. The hook remains resident for process lifetime, consistent with the existing DLL/task design; dynamic DLL unloading is unsupported.

The callback uses a try-lock and skips overrides on contention. This fail-open behavior may cause a visible native-camera frame under contention and requires profiling; stable rendering is not claimed. DirectInput keyboard/mouse-button filtering shares the existing backend; gamepad input is not blocked. Mouse deltas feed camera rotation and may require tuning if the game uses both buffered and state mouse reads. Movement pauses while the editor is visible. Focus loss releases, rather than continuing camera movement from keys typed in another app.

## Dolly keys and controls

- F3: Default → Free → Dolly → Default; each action reveals Camera panel.
- F4: show/hide overlay; hide it for free movement.
- WASD: forward/strafe; Q/E: down/up; mouse/arrows: rotation; Z/X: roll; Shift/Ctrl: fast/slow.
- FOV numeric input applies when armed.
- K: copy the observed/current camera state at host ReplayTime. Replay must be loaded. Same-time key replaces its state; other times add stable-ID keys.
- Camera timeline row shows key diamonds; panel shows times/positions/FOV.
- L: Delete-all modal. Cancel/Escape preserves keys; explicit confirmation clears only editor keys and releases Dolly ownership.
- F6: emergency stop, including camera overrides.

Keys are **session-only**, bound to the loaded replay path. A different replay cannot reuse them silently; clear through confirmation before making a new path. Save/load sidecars, editable gizmos, numeric key transforms, targets, shake, smoothing and camera cuts remain unfinished. Current path defaults to centripetal spline and quaternion SLERP. Dolly start displacement above 20 units is blocked; Free movement is not constrained to that radius.

## Automated results

Release AMD64 host/native library/DLL builds passed. CTest: **14/14 passed**; Rust: **49 passed**, one optional real-file inspection ignored. Native tests cover quaternion/matrix round-trip around rotation wrap, invalid camera rejection, refusal to arm without game context, refusal to insert fabricated keys and Stop state. These are not live game tests.

No camera, Free/Dolly, restoration, input isolation or two-second offset is RUNTIME_VERIFIED/VISUALLY_VERIFIED yet. Existing Enemy test replay root-location blocker is unchanged. NPC/light/Look/ReShade/export code was not redesigned.

## Exact first manual test

1. Close Elden Ring and the old Theater host. Use the matching EXE/DLL in `outputs/Cinematic-C3-native-camera-prototype`.
2. Change the established YAFSML module entry to that package's absolute `TheaterMode.dll` path. Do not copy binaries into the game installation or load CameraTools/FreecamMod simultaneously.
3. Launch the package's `EldenRingTheaterMode.exe` first, then use its existing configured YAFSML launcher. Load any safe offline playable location; remain on foot and stationary. No replay is needed for the first probe.
4. Check normal native camera/gameplay with writes OFF. Press F4 → Camera. Require **Copy hook READY** and **observed YES**. If either is unavailable, send logs and do not enable writes.
5. Click **2-second +0.25 X camera probe**. Expect a small camera shift (not a player teleport), followed by normal native camera after two seconds. Verify no slowdown/crash. F6 stops immediately if needed.
6. Stop here for the initial ownership proof. Send `%TEMP%\TheaterModeGame.log` and `%TEMP%\TheaterModeRender.log`, plus whether the visible camera shifted and returned normally. `CAMERA_COPY_HOOK` and `CAMERA_RUNTIME` lines report the experiment.

Only after that succeeds: F3 until Free, enable experimental writes, hide UI with F4, test small WASD/QE/mouse/roll moves and return via F3 Default/F6. Then load a replay, add K at three timeline positions while moving Free camera; select Dolly and scrub/play the timeline. Test L cancel and confirm. The Enemy test actor replay may still be blocked by root metadata; do not mistake a camera path preview for successful world reconstruction.

Reproducible build: `scripts/Build-CinematicCheckpoint.ps1`. Existing packages are preserved; script refuses overwriting its output directory. Rollback: close game, restore the previous YAFSML DLL path and matching host package.

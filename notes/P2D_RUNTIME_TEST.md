# P2d — next manual game test

New native changes are **IMPLEMENTED — RUNTIME VALIDATION REQUIRED**.
This package is a fidelity/core checkpoint, not the finished full-world cinematic editor.

## Exact package and startup

Use only this duplicate's package:

`outputs/P2d-fidelity-core/EldenRingTheaterMode.exe`

It includes the matching Release x64 `TheaterMode.dll` and compatibility probe.
No rebuilding is required to run the staged binaries. To rebuild from source:

```powershell
powershell -ExecutionPolicy Bypass -File tools/native_replay/build_ghost_prototype.ps1 -OutputDirectory outputs/P2d-fidelity-core
```

1. Close Elden Ring and all other TheaterMode hosts. DLL replacement requires a game
   restart; leaving an old injected DLL in memory mixes protocol versions.
2. Launch this package's EXE first, then use its established offline **Launch Game**
   control. Keep the existing YAFSML loader. The launcher selects the DLL beside this
   EXE through its generated loader configuration. Check the game log for
   `BUILD=P2d-fidelity-core-independent`; another build marker means the wrong DLL
   loaded. Do not replace any original game file or baseline package.
3. Load a save into a safe, flat, on-foot area. EAC must already be absent under your
   established offline workflow; this package refuses operation if its process guard fails.
4. F4 opens the overlay. Open/close it without loading/playing: normal movement must
   remain normal and no body ownership should be logged.

## Test A — new full-precision recording

1. F5 opens the recording-name dialog. Name a new clip and confirm Start.
2. Record 15–20 seconds: stationary, walk, run, turn, a small jump, stop. Remain on foot
   in the same origin/area. F6 stops; wait for `WORLD_FILE: saved ...` before loading it.
3. The library should show the `.erplay`. Its `.erplay.world` sidecar should exist and
   contain version 2; the original files and old v1 recordings must remain unchanged.
4. No `PLAYER_CAPTURE_UNAVAILABLE`, `CAPTURE GAP`, or writer error should appear. If
   one does, send logs; the missing pose is not silently considered captured.

## Test B — root, pose, master clock and Stop

1. Load the new clip. Loading alone must not move/lock your character.
2. Press Play. The player should follow the recorded physics trajectory, turn and
   show the recorded pose. Walking/running must travel rather than animate in place.
3. Press the same Play button again: icon becomes Play, timeline pauses at its exact
   current time and the body holds that pose. Press again to resume there. Repeat with
   Space; button and keyboard must behave identically.
4. Test 0.1x, 0.5x, 1x, 2x. Pose/root should stay synchronized to the timeline. Game/menu
   FPS must not be globally slowed. The game's own unrecorded effects are not frozen.
5. Scrub backward/forward repeatedly and step one sample. Compare the same times
   reached by linear playback. This is a player/available-actor test, not whole-world
   reconstruction acceptance.
6. Press F6 / Stop. After the short existing return cleanup, controls should be normal.
   Keep the overlay open and walk: Stop must not immediately rearm replay or snap back.
7. Close/reopen the overlay: it must remain stopped. Explicit Play or seek is needed
   to request replay application again. At a safety error, Stop then Play retries once;
   there must not be an endless reacquire/teleport/log loop.
8. Load and view an older v1 replay. It should decode, but cannot gain the new file's
   lost precision/hierarchy. Origin and mounted-state guards still apply.

## Test C — companions, separately

Only after the on-foot test, record a separate short clip **entirely mounted** in a
safe open area. Do not use a foot-to-mount transition as an acceptance test yet.
Stop, load it while still mounted in the same origin, then Play.

Torrent must have its own validated body, root, skeleton and pose track to count as
success. A moving rider alone is NOT Torrent success. If Torrent is missing, send
`COMPANION_CANDIDATE`, `COMPANION_RECORDED`, `COMPANIONS_SUMMARY`, skeleton errors and
the full log. The previous null-importer candidate remains unresolved. Do not force
spawn, mounting, death, warp or another risky action for this test.

## Longer-session / measurements

After the short test succeeds, record more than ten minutes while observing FPS and
disk/RAM. There is no ten-minute cutoff. The automated 660-second synthetic test is
not proof of real-game long-session throughput. Send the finalized `WORLD_FILE` line
with MiB/min and drop counts. No game-FPS overhead number has been measured yet.

Do not expect weather, defeated-boss revival, destroyed props, particles, projectile
recreation, cross-region relocation, free/dolly camera, lights or ReShade editing.
Those are unresolved implementations, not features to debug as if already delivered.

## Failure evidence

Send these files, plus the recording and both of its relevant sidecars if shareable:

- `%TEMP%/TheaterModeGame.log`
- `%LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log`
- This package's `BUILD_MANIFEST.txt`

Include which test step failed and a short video/screenshot if the pose/root mismatch
is visual. Logs contain pointer diagnostics for research; replay data does not contain
raw engine pointers.

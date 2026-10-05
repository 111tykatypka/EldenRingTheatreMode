# Elden Ring Theater Mode — experimental tester build

**Not a verified full-combat replay.** Player playback is the retained baseline; nearby existing-character transform playback is a new opt-in experiment. New runtime features need your observations.

## Requirements

- Windows x64, Elden Ring patch 1.17, file/product version 2.7.0.0. Exact supported SHA-256: `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`.
- Your established offline, anti-cheat-disabled YAFSML workflow. YAFSML and game binaries are not bundled. Install Microsoft Visual C++ 2015–2022/2026 x64 runtime if Windows reports missing runtime DLLs.
- Keep the matching EXE and DLL together. Phase5 is a separate preserved fallback.

## Start everything from the host

1. Close Elden Ring and all old Theater Mode hosts. A DLL already loaded in a game cannot be replaced by copying files while that game runs.
2. Extract the entire TesterBuild folder. Launch its `EldenRingTheaterMode.exe` first. No compilation is required to use the package.
3. In Launcher, click **Choose YAFSML...** and select your existing `YAFSML.exe`. Click **Choose Elden Ring EXE...** and select the supported `eldenring.exe`. Paths may contain spaces/Unicode or live on another drive.
4. In Settings choose capture radius **Near (50)** for the first test, Capture Hz **60**, buffer slots **1024**. Click **Save capture settings (next DLL load)**. Keep both experimental actor replay and raw animation requests OFF initially.
5. Click **Start Elden Ring**. The host uses its adjacent `TheaterMode.dll` in a generated launch config under `%LOCALAPPDATA%\EldenRingTheaterMode\launch\YAFSML.ini`; it does not modify the installed loader config or the game files. If you launch YAFSML manually instead, point its module entry to THIS package's `TheaterMode.dll`, replacing the old DLL path before starting the game.
6. Load a save in a familiar flat area containing 3–5 nearby living mobs/NPCs. Stay away from map transitions, cliffs and boss progression for this first transform test. Wait for connected / Player FOUND. Normal movement must still work before replay starts.

If launch fails, inspect the Launcher diagnostic and logs. Optional read-only check from PowerShell:

```powershell
.\EldenRingCompatibilityProbe.exe --game-exe 'D:\Your Game\Game\eldenring.exe'
```

## Record fresh data

7. Press **F5** or Recorder **Start / F5**. Walk/turn around the existing actors for roughly 30 seconds; do not kill them in the first test. Stand still briefly at the beginning and end. Confirm actual samples/actors increase and the recorder stays RECORDING.
8. Press **F6** or **Stop / F6**. Wait for save/final validation. The file is under `%LOCALAPPDATA%\EldenRingTheaterMode\replays\replay_*.erplay`.
9. Open that new replay from Library. Inspect Characters and raw animation IDs; select an actor to inspect recorded HP/model. Player equipment IDs are shown in Inspector when the player is selected (default on opening). Scrub offline after Stop; this does not reconstruct the world.

## Playback in stages

10. Keep the same game session, same map, actors alive and loaded. Walk back near the recorded start. Start displacement is logged and may show a warning; **Cancel** if you are in a different scene or far away. Actor acquisition separately refuses mismatches or more than 20 units of displacement. The prototype cannot respawn or rematch actors across reloads reliably.
11. Settings → Native duration **First 5 seconds**, actor replay OFF, raw animation OFF. Click Inspector **Play / Resume in game**. Check retained player movement, Pause, Resume, and F6 restoration. If baseline fails, stop testing and report logs.
12. Stop. Enable **Experimental existing-character transform replay** in Settings. Leave raw animation OFF. Click **Play / Resume in game**. Expected experiment: matching living actors move/rotate along their recorded trajectories while the player follows its track. They may not animate correctly; report sliding, AI resistance, grounding errors or missing actors rather than treating those as success.
13. Use Pause (holding transforms), Resume, then **F6**. Verify normal player movement and normal NPC/AI behavior return. F6 is a registered global Stop hotkey; the host must report it available before launch. If the game is frozen, callback restoration waits until the game thread resumes. Exit the game if Stop cannot recover normal behavior.
14. Only after the 5-second test is stable, repeat **First 10 seconds**, then **Full replay**. Try 0.5x, 1x and 2x. Record visual rotation and stop behavior separately from animation/combat fidelity.
15. A larger nonlethal recording can use radius 100/200/custom, saved before a fresh DLL load. Do not assume every loaded actor can replay at 60 Hz: watch IPC/capture drops, rejection counts and game responsiveness.

Combat, rolls/jumps, weapon swaps, gestures, damage and death can be **recording/inspection** tests after the initial nonlethal test. This build does not reproduce attacks, damage, inventory changes, death/resurrection or effects. Do not use those as playback acceptance criteria or intentionally progress a boss encounter during replay testing.

## Send results

16. After Stop, Diagnostics → **Collect tester logs**. Send:
    - the **new `.erplay`** you recorded;
    - `%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log`;
    - `%TEMP%\TheaterModeGame.log` (also copied to logs by Collect);
    - `%TEMP%\TheaterModeLocomotionTrace.log` if Trace was enabled;
    - the files under `%LOCALAPPDATA%\EldenRingTheaterMode\launch\` if launch failed;
    - `BUILD_MANIFEST.txt` from the tested package;
    - optionally a short video showing player/NPC motion, Pause/Resume/F6.
17. In `TEST_RESULTS_TEMPLATE.md`, state exact scene, observed character count, capture rate/drops, player/NPC movement, rotation, grounding, AI/control restoration and any crash. Do not mark an unperformed test PASS.

## Developer rebuild

From the repository with the installed VS x64 C++ toolchain, CMake and Rust:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Build-Tester.ps1
```

It builds Release x64, runs both test suites, validates the pinned dependency configuration, and stages a matching package in TesterBuild. Run under a normal Windows token for named-pipe identity tests. No game is launched by the build script.

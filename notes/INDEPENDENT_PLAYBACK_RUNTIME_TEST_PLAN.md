# Independent playback build — manual checkpoint

## Build and launch

No build is needed: matching Release x64 files are already in:

`C:\Users\user\Documents\ChatGPT\elden ring theater mode\EldenRingTheatreMode-independent\outputs\Task4-TimescaleProbe`

Files: `EldenRingTheaterMode.exe`, `TheaterMode.dll`, compatibility probe, source/binary manifests and these status instructions.

If rebuilding, from the independent source directory:

```powershell
& .\tools\native_replay\build_ghost_prototype.ps1 -OutputDirectory "$PWD\outputs\Task4-TimescaleProbe"
```

1. Stop any recording normally. Close Elden Ring, the previous Theater Mode host and any running camera tool. The DLL cannot be replaced in an already-running game. Only one matching host/DLL pair should run because inherited pipes/settings/storage are shared.
2. Start **this output folder's** `EldenRingTheaterMode.exe`, not the original Step2a application.
3. Confirm the existing YAFSML loader and game paths in the launcher. Click Launch Elden Ring. The existing launcher generates its own config referencing the DLL **beside this EXE**. No replacement of the original Step2a DLL or original YAFSML config is needed when launching this way. If using YAFSML manually instead, point its Theater DLL entry to this new DLL's absolute path.
4. Load the normal offline save into the same known safe, flat area where the skeletal replay was recorded. Use a fresh short replay there if unsure of the old location; cross-session coordinate restoration is not fixed in this checkpoint.
5. Press F4 to open the existing overlay. Open a replay with its `.erplay.bones` sidecar and wait for the bone-loaded status.

## A. Controls — world timing stays read-only

Do not set `THEATER_WORLD_TIMESCALE` for this first run.

- Space once: playback should advance. Space again: timeline and replay body hold. Wait three seconds: timeline must not reset or drift. Space again: resumes there, not sample zero.
- Seek to another time while paused, then Space: playback resumes from that point. Holding Space must not toggle every repeat message. Check text entry does not trigger transport.
- Hover the speed box without opening it. Wheel up/down selects successive presets. Check dropdown selection still works and the wheel away from the box retains existing behavior. New presets are 0.10/0.20/0.25/0.50/0.75/1.00/1.25/1.50/2.00/4.00. In this first run only replay timing changes; normal world speed is expected.
- In Replays, click Unload Replay while playing. The replay stops, timeline/selection clears and normal body/input ownership should return after the short restoration path. Close F4 and verify normal movement. File and sidecar should still exist. Loading the same replay again should work.
- F6 remains the emergency Stop. If anything appears wrong, use F6, close the overlay, and check normal input. If the game is unstable, exit normally rather than continuing tests.

## B. Read-only timing probe — send logs before enabling writes

Play five seconds at 1.00, then at 0.50 and 0.10, and Stop. Send the logs listed below. Expected labels:

`TIMESCALE_BINDING=STATIC_VALIDATED mode=READ_ONLY`

`TIMESCALE_OBSERVE root=... scale=... delta=... requested=... active=... write_enabled=false`

The manager pointer should be non-null; normal scalar should be near 1.0; candidate delta should be a finite plausible frame duration. This is evidence for the runtime address, **not** proof that world slowdown works. No automatic runtime comparison has been performed by this session.

## C. Subsequent controlled world-speed experiment

Only after the read-only values are reviewed, close game/host again and launch this host from PowerShell with the opt-in inherited environment:

```powershell
$env:THEATER_WORLD_TIMESCALE = '1'
& 'C:\Users\user\Documents\ChatGPT\elden ring theater mode\EldenRingTheatreMode-independent\outputs\Task4-TimescaleProbe\EldenRingTheaterMode.exe'
```

The launcher then starts Elden Ring through the same YAFSML workflow. Confirm log mode `EXPERIMENTAL_WRITE`. Begin with a short replay at 1.00, then five seconds each at 0.50/0.25/0.10. Observe nearby NPC/environment motion as well as the replay actor. No bone interpolation changes have been made yet, so skeletal sample stepping at 0.10 may still be visible.

Stop/unload and verify world speed returns to normal. Pause should freeze the replay timeline/body but **return the world to normal speed** in this prototype; whole-world pause is not implemented. Resume should reapply selected world speed. Check Stop at each speed. Do not test 2x/4x until these lower-speed restoration tests succeed. Do not run IGCS or another speed mod concurrently. Do not intentionally force loading/player-loss until basic release is verified.

Clear the experiment setting after closing the test host:

```powershell
Remove-Item Env:\THEATER_WORLD_TIMESCALE -ErrorAction SilentlyContinue
```

This removes a shell environment variable, not project/game files. Default subsequent launches are read-only unless the variable is set elsewhere.

## Logs to send

- `%TEMP%\TheaterModeGame.log` — timing binding/observation/apply/reset and bone ownership/draw diagnostics.
- `%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log` — host replay/Space/unload/speed and launcher transitions.
- `%LOCALAPPDATA%\EldenRingTheaterMode\launch\log\YAFSML.log`, if present, for launch failures.
- This output's BUILD_MANIFEST.txt if there is uncertainty about the loaded build.

Report which checks passed, exact failing action, whether normal input/world speed restored, and any crash. Do not treat build success as in-game verification.

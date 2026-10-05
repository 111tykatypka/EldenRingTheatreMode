# Phase 4A — launch from the existing host

2026-10-05 — **IMPLEMENTED — RUNTIME VALIDATION REQUIRED**.

Branch: `phase4-in-game-replay-prototype`, following `bb3285e`. The Rust adapter, pinned dependencies, game-thread callback, player access, both IPC protocols and offline ReplayPlayer are unchanged.

## One-window workflow

Open `Phase4A\EldenRingTheaterMode.exe`, then click **START ELDEN RING**. No separate CMD or injector is required for the normal workflow. The EXE runs the installed YAFSML; its binaries are not embedded or redistributed.

- Default loader: the current Windows Desktop folder's `YAFSML-v0.10.4\YAFSML.exe`. **YAFSML...** selects another installed copy; its absolute Unicode path is saved at `%LOCALAPPDATA%\EldenRingTheaterMode\YAFSML.path`.
- Game: the existing exact-profile executable at `C:\Users\user\Downloads\ELDEN RING\Game\eldenring.exe`.
- Module: `TheaterMode.dll` beside the running host EXE. There is no stale hardcoded stable-output DLL path.
- Configuration: a generated copy at `%LOCALAPPDATA%\EldenRingTheaterMode\launch\YAFSML.ini`. The source `YAFSML.ini` beside the selected loader remains intact. Patch/log/settings values are preserved. Only `theater_mode` is pointed at this host's DLL, and active relative `[dll]`/`[mod]` paths are rebased to the original config directory. Load-condition suffixes are preserved.
- Process creation: Unicode `CreateProcessW` with the explicit YAFSML executable and working directory, quoted `-t eldenring -p <game> -c <generated config> -d <installed YAFSML.dll>`. The helper launcher console is hidden; its configured game logging remains in the copied configuration.

## Guards and state

The UI enables launch only when its original inbound sample pipe exists and global F6 registered. A single-instance host mutex prevents another new host from competing for the pipe/hotkeys. Older hosts are still detected by existing pipe/F6 ownership checks.

The worker refuses an already-running `eldenring.exe`, checks all required files and AMD64 EXE/DLL types, then calls the same `tm_validate_profile` used by the compatibility probe. Exact path, file/product version 2.7.0.0 and SHA-256 remain required. Hashing/config/process monitoring run off the UI thread. It checks the process list again after hashing. Repeated clicks are refused while a launch/session is active.

States: checking → YAFSML started/waiting → connected. **CONNECTED** requires the existing telemetry PID and control IPC, and **Runtime READY / Player FOUND** requires the control module's ready flag. Process creation alone is never reported as successful DLL/player initialization. Missing connection after 120 seconds reports an error; this timeout does not limit gameplay/recording duration. No probe/replay is armed by launching.

Closing the host cancels pending preflight/monitoring and retains its existing control shutdown. It does not terminate the game. Changing or reconnecting this DLL still requires a new game session; no hot-unload behavior was added.

## Validation

Release x64 builds. CTest 4/4 passed: existing ERPLAY, ReplayPlayer and control tests plus launcher tests. Launcher tests cover config preservation, Unicode paths/text, relative DLL/mod paths, condition preservation, Windows argument parsing including quotes/trailing slashes, duplicate/wrong config rejection, missing prerequisites and host-not-ready refusal.

A separate read-only installed-prerequisite test passed against the actual YAFSML installation, Phase4A DLL and game executable: loader/DLL AMD64, source configuration accepted, shared profile accepted, file/product version 2.7.0.0, disk SHA-256 `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`.

These tests did not launch YAFSML or Elden Ring. Actual button launch, runtime DLL connection and the transform write probe await the user's test in `PHASE4A_MANUAL_TEST.md`. The GUI itself has not been manually inspected in this task.

Only the host needs updating for this change. `TheaterMode.dll` remains SHA-256 `25E0CB1539C8E4DB3FA6AB7325844DD4D1EB718FFC2A63E76C90D262FA2F47E0`. No game files or original loader files were written.

## Logs

- Host/launcher: `%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log`, meaningful `LAUNCHER` transitions, profile result, command and paths.
- Loader for the new button: `%LOCALAPPDATA%\EldenRingTheaterMode\launch\log\YAFSML.log`, opened by **OPEN LAUNCH LOGS**.
- Game module: `%TEMP%\TheaterModeGame.log`.

`Start-EldenRing-Phase4A.cmd` / `YAFSML_Phase4A.ini` remain an optional old fallback. If using that fallback, YAFSML's log is under `Phase4A\log`, beside that selected configuration.

## Staged build

`C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase4A\EldenRingTheaterMode.exe` — Release AMD64, SHA-256 `2F89399B408E3938B680FA0B2E9D95612CCABB002C1B546831C0B4F7B4FA1D04`. Only this EXE and the two updated instructions were staged. The DLL hash remains unchanged. Original YAFSML.ini SHA-256 before/after: `51791F90771D4F9785E62AF80495645585F7E8DDEC0590BB733679C90514AF33`.

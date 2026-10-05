# Exact next manual test — developer experimental nightly

## Prepare

1. Close Elden Ring and all older TheaterMode hosts. An already loaded DLL cannot be replaced in its process; restart is required for this new build.
2. Use the packaged files in `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Nightly_ResearchIntegration`. No rebuild is necessary. To reproduce from source: `powershell -ExecutionPolicy Bypass -File scripts\Build-Nightly.ps1` with VS2026 C++, Rust MSVC and the existing pinned dependencies.
3. The YAFSML configuration must load **the new nightly TheaterMode.dll**, not Phase5/TesterBuild. Use the host's existing launcher so its generated config points to the sibling DLL. No replacement of the game EXE/files or loader is needed. If manually using your YAFSML configuration, update only its mod DLL path to the nightly file.
4. Launch the nightly `EldenRingTheaterMode.exe` first. Click its existing Start Elden Ring control, which uses your established offline YAFSML workflow. If the configured loader path is missing, select your existing YAFSML; no new injector is required.
5. Load your usual offline save at a safe, flat area with a living nearby NPC. Stay away from cliffs, elevators, combat and loading transitions for the first diagnostics. Wait for connected/module READY/player FOUND. Never test unsupported versions.

## A. Read-only grounding baseline

1. Replay stopped. In Diagnostics click **Runtime differential trace (10s)**. If the host log reports `queued=0`, wait for the current IPC update and click again; it is not a capture acknowledgement.
2. Stand still for 2s, walk a short flat path, rotate, then stop. This button must not move your character. Window automatically ends after 10s; allow 1s for the worker flush.
3. Click **Collect tester logs**. `TheaterModeRuntimeTrace.jsonl` contains native observations, not synthetic data.

## B. Player replay differential comparison

1. Record a fresh 5–10s short walk in the same loaded area using the existing recorder. Stop and open that real replay. Existing files can also be used if they match this exact area/lifecycle.
2. In Settings disable raw animation, experimental existing-character playback, and selected-NPC-only. Choose **First 2 seconds** initially.
3. Click Runtime differential trace, then Play. Observe grounding and rotation. F6 is emergency Stop; use it immediately on unwanted falling/teleporting. Do not continue to a full fight when grounding fails.
4. Send logs whether the bug persists or changes. Immediate readback correctness is not sufficient: the next physics/model/proxy observations and visible grounding matter.

## C. One NPC only

1. Make a fresh short recording with one nearby living NPC in the current loaded world. Stop/open it without warping, reloading or killing that NPC.
2. In Characters select that **NPC track**, not the player. Settings: enable **Replay selected NPC only (player writes OFF)**; raw animation OFF; native duration **First 2 seconds**. Existing-character bulk checkbox can remain OFF.
3. Start the ten-second differential trace, then Play. Your player should remain under normal control; only the selected NPC is eligible for transform writes. Expected native application is **not yet verified**. If nothing moves, the logs must show whether the host produced a target, DLL received it, lookup/identity/distance rejected it, or writes were overwritten.
4. Test Pause briefly, Resume, then F6 Stop. Only after successful short behavior try First 5 seconds. Do not test bosses/full replay first.

## D. Optional isolated animation-speed experiment

1. Replay and recording stopped, exact nearby NPC track selected and still alive. Diagnostics mode **animationSpeed = 0** → **Run selected NPC experiment (2s)**.
2. Observe whether ONLY that NPC's animation stops, and resumes after ~2 seconds. F6 cancels early. Player control should remain normal. Repeat with F6 after ~0.5s to check restoration.
3. noMove/noAttack/both/noUpdate entries intentionally show BLOCKED and cannot run due to the verified source-layout conflict. Do not override this through Cheat Engine.

## Send back

Click Collect tester logs and send the collected folder plus the real `.erplay` used and the package `BUILD_MANIFEST.txt`. Native files: `%TEMP%\TheaterModeGame.log`, `%TEMP%\TheaterModeRuntimeTrace.jsonl`; host: `%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log` (or the host's Recorder logs button folder). Include what visibly happened: grounded/floating, NPC motion, rotation, speed restore, Pause/Resume/Stop, crash.

Analyzer from project root: `python tools\research_integration\analyze_runtime_trace.py <TheaterModeRuntimeTrace.jsonl> --output trace_summary.json`.
No new runtime feature is marked verified until these results are supplied.

# P2c manual in-game test

**Experimental; not runtime verified.** No automatic mount/dismount, summon spawning or dismissed-actor restoration.

## Launch the correct version

1. Close Elden Ring completely and close every running `EldenRingTheaterMode.exe`. A loaded DLL cannot be replaced in a running process. End any recording first.
2. Launch ONLY:
   `C:\Users\user\Documents\ChatGPT\elden ring theater mode\EldenRingTheatreMode-P2b-independent\outputs\P2c-mounts-summons\EldenRingTheaterMode.exe`.
3. Keep your established offline/EAC-disabled YAFSML setup. Check the launcher's game path is `C:\Users\user\Downloads\ELDEN RING\Game\eldenring.exe`; choose your existing YAFSML.exe if the saved path is absent.
4. Click the existing Start/Launch game button. This host passes the **TheaterMode.dll beside this EXE** into its generated YAFSML launch configuration. You do not need to overwrite P2b or modify the original YAFSML.ini. If you launch YAFSML manually instead, explicitly select the DLL from the P2c folder, not TheaterMode-Current.
5. Load a familiar save and a flat, open area where Torrent is allowed. Stay in the same area for these first tests. F4 opens the overlay. Confirm `%TEMP%\TheaterModeGame.log` contains `BUILD=P2c-companions-independent` and the correct 2.7.0.0 profile.

If a rebuild is needed, from the duplicate run:
`powershell -ExecutionPolicy Bypass -File tools\native_replay\build_ghost_prototype.ps1`.
Its default output is now the duplicate's P2c folder; the historical script name does not mean native ghosts are restored.

## Test A — ordinary player regression

1. Without a replay active, confirm normal movement, menus, camera and FPS.
2. F5, walk/roll and switch weapon on foot for 10–15 seconds, F6.
3. Wait for `WORLD_FILE: saved ...`. Open the new recording in the library and Play. Verify the previously working player poses and weapon changes.
4. Stop (F6 or the timeline Stop button), Unload Replay in the library, close overlay (F4). Verify controls and equipment return. Do this before recording the next test.

## Test B — Torrent, continuously mounted segment

1. Summon Torrent normally. Stay mounted throughout recording and first playback; do not record mounting/dismounting yet.
2. F5, ride slowly in a short loop on flat ground for 10–15 seconds, then F6. Avoid combat, drops and map transitions on this first test.
3. In the event log look for `COMPANION_RECORDED` with buddy/mount evidence. In the game log find `COMPANIONS_SUMMARY` and the saved world-file line. A missing companion or `COMPANIONS_UNAVAILABLE` is a failed discovery test; send logs before continuing.
4. Stay mounted with Torrent alive. Open the new replay and Play. Observe both the rider and Torrent: their positions, rotations and legs should follow the recorded motion. This is an expectation to verify, not a claim it works.
5. Test 0.5x, 1x and 0.1x; check game FPS, jitter and feet/contact. Scrub within the mounted segment only. Stop + Unload + close overlay. Check normal riding, then dismount normally.
6. Optional mismatch check: on foot, try that mounted replay. Expected: a mount-state error and no forced mounting. Stop/Unload afterwards. If the error does not appear, send the log.

If the rider/mount snaps, slides, falls or fights the replay, press F6, Unload Replay and F4 immediately. Do not continue to spirit tests until this is diagnosed.

## Test C — spirit ash / NPC summon already alive

1. Use a familiar spirit-ash-enabled area. Summon through the normal game action before recording. Prefer a single-body summon for the first test; do not kill a boss or dismiss the summon between record and playback.
2. F5 for 10–15 seconds while it moves, then F6. Confirm a separate companion ID and actor track were recorded.
3. With the same summon still alive, load and Play. Observe its recorded root and bones, and whether AI fights the pose. Scrub within the observed segment; Stop + Unload + close overlay. Confirm normal summon behavior returns.
4. Repeat for an offline NPC co-op-style summon only if one is already conveniently available. Do not use online matchmaking.
5. Missing/dismissed summons cannot be recreated by this build. Identical summon groups across a different handle generation may remain unmatched rather than being assigned incorrectly.

## Send these results

- Which tests ran and whether player/Torrent/summon matched the recorded motion.
- Whether mount mismatch was blocked; Stop/Unload restored control; any crash, jitter or FPS change.
- `COMPANION_RECORDED`, `COMPANIONS_SUMMARY`, `ACTORS: ... found`, `ACTOR_UNAVAILABLE`, `COMPANION_REPLAY_BLOCKED`, `WORLD_FILE: saved ... MB per minute` lines.
- Full `%TEMP%\TheaterModeGame.log` if anything fails.
- `%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log` and `%LOCALAPPDATA%\EldenRingTheaterMode\launch\log\YAFSML.log` if present for launch/connection failures.
- `BUILD_MANIFEST.txt` from the P2c folder; if needed the replay plus its `.world` sidecar (both files matter).

Phase 2.3 is not accepted until these live results are confirmed. Do not proceed to the next phase yet.

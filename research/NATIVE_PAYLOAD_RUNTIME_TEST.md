# New payload/action/cadence experiment

This does NOT repeat the old pool-population test. New questions: observed node cadence between game callbacks; complete active pool snapshot; byte changes during manually labeled actions; numeric behavior/event values correlated with live animation/action observation. No ghost creation or transform writes.

1. Build with `scripts/Build-NativeBloodstainResearch.ps1 -ResearchFeature native-payload-capture -OutputDirectory <separate directory>`. Release x64 package is staged in `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\NativeReplayPayloadResearch`.
2. Close Elden Ring and previous Theater Mode hosts. New DLL requires a fresh load; no hot unloading. Phase5 is untouched.
3. Launch `EldenRingTheaterMode.exe` from that directory first. In existing launcher settings select the matching `TheaterMode.dll` from the SAME directory for YAFSML. If previous config points to Hotfix1/Phase5, replace that selection. Do not copy into game installation.
4. Launch game with existing offline workflow. Load any safe flat area. Wait for PLAYER_FOUND. No original replay required. Keep game focused; F10/F11 are scoped to foreground game.
5. Press F10 once: begin capture, marker IDLE. Stay still5s.
6. Press F11: WALK; walk forward5s, then stop. F11: ROTATE; turn5s. F11: ROLL; one roll, wait3s. F11: LIGHT_ATTACK; one safe light attack, wait3s. F11: JUMP; one jump, wait3s.
7. F11: FALL. Only if a harmless short drop exists, perform it; otherwise skip it and report skipped. F11: LAND, then remain grounded3s. These are USER labels, not automatic action detection; fall/land timing will be checked against bytes/live state.
8. F11: FINAL_IDLE; stay still5s. F10: stop capture. Do NOT use F5 or Play; this is native recorder observation, not ERPLAY playback.
9. Normal gameplay should remain unchanged. No ghost should appear. If something fails, exit normally and provide logs. Capture also stops on missing/changed player or invalid layout.
10. Send `%TEMP%\TheaterModeGame.log` and newest `%LOCALAPPDATA%\EldenRingTheaterMode\native-replay\native_replay_*.jsonl`. Game log reports start/markers/stop and queue drops. No console focus needed.

Offline analysis: `python analyze_payload_capture.py <journal> --output payload_actions.json`; `python payload_codec.py <journal> --output decoded_nodes.json`. Logs use monotonic timestamps, not wall-clock action timing. Position coordinates are block/MSB quantized and must not be directly equated with rebased physics coordinates. Observer can miss multiple writes between callbacks; dropped rows are reported, never hidden.

Read-only feature rejects existing write commands and disables game mutation paths. Bounded channel64 batches avoids blocking PostPhysics; diagnostic pool read budget512 is not a replay/actor limit. Per-callback reads/allocations add unmeasured overhead; stop with F10 if performance degrades. High-frequency rows go to JSONL, not ordinary per-frame game log.

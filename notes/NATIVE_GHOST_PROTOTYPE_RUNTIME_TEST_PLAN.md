# ONE test: native ghost CREATE → REMOVE

No build needed if using the supplied Release artifacts. To rebuild, run from repository root:

```powershell
& .\tools\native_replay\build_ghost_prototype.ps1
```

1. Close Elden Ring and all old Theater Mode hosts. The DLL cannot be replaced/reloaded reliably while the game is running.
2. Open the latest `NativeReplayGhostPrototypeStep/EldenRingTheaterMode.exe` from the isolated output. Keep its sibling `TheaterMode.dll` there. Do not overwrite Phase5 or the game directory. The top-right noninteractive HUD shows native status even without Insert. This build consumes commands at the verified native TestNetStep rather than waiting solely for the periodic builder branch.
3. Use the existing host Launch Game/YAFSML workflow with offline, anti-cheat-disabled single-player setup already established by the user. The launcher uses the sibling DLL and stages its own YAFSML config; no replacement of the original loader is required. If launching YAFSML manually, set its Theater DLL path to this new output DLL before launching; do not leave it pointing at an old diagnostics build.
4. Load an existing save in a safe, open, flat area; stand on foot, not mounted. No boss fight, death, warp, grace interaction or map transition during this one test.
5. Wait for PLAYER_FOUND/READY, then make a few seconds of normal walking. Check `%TEMP%/TheaterModeGame.log` for `NATIVE_GHOST: PLAYER_RECORDER_READY` and bridge initialization=1. If installation failed, do not press create; send the log.
6. With Elden Ring focused, press **F10 exactly once** (`NATIVE_REPLAY_GHOST_CREATE_TEST`). It queues the command; creation waits for the original native local replay callsite, up to 60 seconds. The HUD displays the pending message and countdown. Do NOT press F11 while waiting: that cancels creation. Wait for `NATIVE_GHOST_CREATE` with a nonzero actor and `NATIVE_GHOST: OWNED`. If TIMEOUT/precondition/factory error appears, STOP this test and send the log. Do not spam/retry F10 or use Play.
7. After actual creation, wait **5–10 seconds**. A native ghost may appear; visible accurate animation is not this milestone's acceptance condition. Do not move maps or press replay/record controls.
8. Press **F11 exactly once** (`NATIVE_REPLAY_GHOST_REMOVE_TEST`). This queues removal to the native world removal drain. If the native world already removed the ghost automatically, the log may say no owned ghost; report this, do not create another.
9. Wait another **10 seconds**. Check for native disable, slot actor null, DelayDelete enqueue/deleter, ghost/manipulator destructors and data release. No actor reads occur after retirement. If timeout occurs, do not try raw cleanup or repeat the test.
10. Exit Elden Ring normally, then close the host.
11. Send the complete `%TEMP%/TheaterModeGame.log`, `%LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log`, and this output's `BUILD_MANIFEST.txt`. Report whether a ghost appeared, whether removal visibly worked, whether control stayed normal and whether the game crashed. If a crash occurred, include the existing crash/exception log or dump path if available.

F11 can cancel a pending create before it is consumed. No ghost is created outside the original native context, and no generic emergency raw destruction is provided. One attempt per process; only run another test after logs have been analyzed.

Success requires actual runtime evidence for one create, valid native objects, native activation, removal/entry clear/DelayDelete teardown and stable gameplay. Compilation alone is not success. Prototype 2 (Idle→Walk→Stop native movement/animation) is deliberately deferred until this test passes.

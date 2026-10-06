# Build and run

Snapshot: 2026-10-06, implementation `3d97070`. Latest native ghost build compiled; runtime creation/removal remains Unverified. This handoff does not run new implementation tests.

## Toolchain

|Component|Observed version/source|
|---|---|
|Visual Studio|18 Community, generator `Visual Studio 18 2026`|
|MSVC|19.51.36260.0; installed tool directory 14.51.36231|
|MSBuild|18.10.1-1.26427.6|
|Windows SDK|10.0.26100.0|
|CMake|4.3.1 in existing build evidence; project minimum 3.24|
|Rust|rustc 1.99.0, b940084d7, 2026-09-28; edition 2024|
|Target|x86_64-pc-windows-msvc|
|Dear ImGui|Vendored 1.92.5 docking|
|MinHook|Vendored, exact upstream release Unverified|
|SDK|fromsoftware-rs fork revision 3c8c1d7633a99309fb004c9f894ea10b7967d0e0|
|Python research tools|3.10+; stdlib SQLite; optional Capstone 5.0.6|

No vcpkg/conan integration was found in checked build scripts. `adapter/Cargo.lock` is authoritative for exact dependency versions; a generated table is appended below. Do not update the SDK/lock merely to solve an unrelated issue.

## Reproduce latest native ghost package

```powershell
Set-Location 'C:\Users\user\Documents\ChatGPT\elden ring theater mode\EldenRingTheatreMode-phase4'
& .\tools\native_replay\build_ghost_prototype.ps1 -OutputDirectory 'C:\Users\user\Documents\ChatGPT\elden ring theater mode\outputs\EldenRingTheaterMode\NativeReplayGhostPrototypeStep'
```

Script builds Release x64 host/static backend/probe using `build-native-ghost`, then Cargo `--release --locked --offline --target x86_64-pc-windows-msvc --features native-replay-ghost-create-remove` into `adapter/target/native-ghost-prototype`. `THEATER_NATIVE_LIBRARY_ROOT` selects the matching static backend. Output includes EXE/DLL/probe, BUILD_MANIFEST.txt, SOURCE_SHA256.txt and instructions. Without the output argument, script defaults to NativeReplayGhostPrototype, not Step.

CMake path used:
`C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`.
Cargo: `%USERPROFILE%\.cargo\bin\cargo.exe`.

Generic `build_release.bat` builds the default DLL without the ghost feature and defaults to historical outputs. Supply an isolated output argument. It stages TheaterMode.dll and TheaterMode_1_17_diagnostics.dll. A running game can lock the DLL. Build native backend before Rust linking; offline Cargo requires populated caches. Never enable `native-payload-capture` simultaneously with ghost feature: both use F10/F11.

## Test tooling and result limits

CMake/CTest targets cover ERPLAY, player, control, launcher, in-game replay, character/capture/action tracks, modern UI/editor IPC, render backend, worker and locomotion timeline. Rust covers adapter protocols/state; Python covers research/payload/capture tools. `render-dx12-smoke` is not an Elden Ring gameplay test.

Native lifecycle milestone: Release succeeded; automated tests were not run. Existing notes contain older test results, which must not be attributed to the latest prototype. Compilation alone is not acceptance. Recent warnings include GameProfile C4530 and Rust unused imports/dead code/crate naming.

## Exact next live test

1. Close Elden Ring and every old Theater Mode host. No safe DLL hot unload/reload exists.
2. Open EXE from NativeReplayGhostPrototypeStep, with matching sibling DLL.
3. Use established Launch Game/YAFSML offline, anti-cheat-disabled workflow. Manual YAFSML launch must point to this output DLL. No game installation changes.
4. Load a safe flat open area, on foot. Avoid bosses, death, warps, grace and transitions during this test.
5. Wait PLAYER_FOUND and `NATIVE_GHOST: PLAYER_RECORDER_READY`; walk normally a few seconds. Check bridge initialization=1.
6. With game focused press F10 **once**, wait up to 60 seconds for actual CREATE/OWNED or error. Do not press F11 while pending: it cancels. Do not retry F10 or use Play.
7. After genuine creation, wait 5–10 seconds, F11 once, wait 10 seconds and exit normally.
8. Send `%TEMP%\TheaterModeGame.log`, `%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log` and output BUILD_MANIFEST.txt. Include render/crash logs if relevant. Report visible actor, removal, normal control and crash status.

Acceptance requires native objects/activation, ownership, slot clear, DelayDelete/destructor/reference evidence and stable gameplay. Ghost action fidelity is the subsequent milestone.

## EAC and debugging

Testing requires the owner's established offline/anti-cheat-disabled setup. No EAC bypass or disabling implementation exists. Exact setup procedure is not documented; ask owner for their known arrangement rather than invent flags or edit game files.

Visual Studio: Debug → Attach to Process → actual offline eldenring.exe, native x64 debugger, matching DLL/PDB symbols if available. Breakpoints can stall game tasks and IPC leases; prefer guarded logging. Rust release PDB/source stepping availability is Unverified. Never debug an online/EAC-enabled session.

Logs: `%TEMP%\TheaterModeGame.log`, TheaterModeRender.log, TheaterModeCrash.log; recorder log above. Matching source/binary manifests matter more than output folder name. Observed Step artifact sizes: EXE 3,267,072 bytes; DLL 2,258,944; probe 59,392. These are checkpoint sizes, not future build guarantees.

---

## Evidence appendix: `notes/NATIVE_GHOST_PROTOTYPE_RUNTIME_TEST_PLAN.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

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


## Exact locked Rust packages

|Package|Version|Origin|
|---|---|---|
|arrayvec|0.7.8|registry+https://github.com/rust-lang/crates.io-index|
|bitfield|0.19.5|registry+https://github.com/rust-lang/crates.io-index|
|bitfield-macros|0.19.5|registry+https://github.com/rust-lang/crates.io-index|
|bitflags|2.13.2|registry+https://github.com/rust-lang/crates.io-index|
|bumpalo|3.20.3|registry+https://github.com/rust-lang/crates.io-index|
|byteorder|1.5.0|registry+https://github.com/rust-lang/crates.io-index|
|cfg-if|1.0.5|registry+https://github.com/rust-lang/crates.io-index|
|core_detect|1.0.0|registry+https://github.com/rust-lang/crates.io-index|
|dataview|1.1.0|registry+https://github.com/rust-lang/crates.io-index|
|derive_pod|0.1.3|registry+https://github.com/rust-lang/crates.io-index|
|eldenring|0.14.0|git+https://github.com/KamiyamaShiki0704/fromsoftware-rs?rev=3c8c1d7633a99309fb004c9f894ea10b7967d0e0#3c8c1d7633a99309fb004c9f894ea10b7967d0e0|
|encoding_rs|0.8.42|registry+https://github.com/rust-lang/crates.io-index|
|equivalent|1.0.2|registry+https://github.com/rust-lang/crates.io-index|
|from-singleton|3.0.1|registry+https://github.com/rust-lang/crates.io-index|
|fromsoftware-shared|0.14.0|git+https://github.com/KamiyamaShiki0704/fromsoftware-rs?rev=3c8c1d7633a99309fb004c9f894ea10b7967d0e0#3c8c1d7633a99309fb004c9f894ea10b7967d0e0|
|fromsoftware-shared-macros|0.14.0|git+https://github.com/KamiyamaShiki0704/fromsoftware-rs?rev=3c8c1d7633a99309fb004c9f894ea10b7967d0e0#3c8c1d7633a99309fb004c9f894ea10b7967d0e0|
|fromsoftware-shared-stl|0.14.0|git+https://github.com/KamiyamaShiki0704/fromsoftware-rs?rev=3c8c1d7633a99309fb004c9f894ea10b7967d0e0#3c8c1d7633a99309fb004c9f894ea10b7967d0e0|
|fxhash|0.2.1|registry+https://github.com/rust-lang/crates.io-index|
|glam|0.34.0|registry+https://github.com/rust-lang/crates.io-index|
|hashbrown|0.17.1|registry+https://github.com/rust-lang/crates.io-index|
|indexmap|2.14.2|registry+https://github.com/rust-lang/crates.io-index|
|inflections|1.1.1|registry+https://github.com/rust-lang/crates.io-index|
|libc|0.2.190|registry+https://github.com/rust-lang/crates.io-index|
|memchr|2.8.3|registry+https://github.com/rust-lang/crates.io-index|
|multiversion_no_op|1.0.0|registry+https://github.com/rust-lang/crates.io-index|
|no-std-compat|0.4.1|registry+https://github.com/rust-lang/crates.io-index|
|nonmax|0.5.5|registry+https://github.com/rust-lang/crates.io-index|
|num_enum|0.7.6|registry+https://github.com/rust-lang/crates.io-index|
|num_enum_derive|0.7.6|registry+https://github.com/rust-lang/crates.io-index|
|pelite|0.10.0|registry+https://github.com/rust-lang/crates.io-index|
|pelite-macros|0.1.1|registry+https://github.com/rust-lang/crates.io-index|
|proc-macro-crate|3.5.0|registry+https://github.com/rust-lang/crates.io-index|
|proc-macro2|1.0.107|registry+https://github.com/rust-lang/crates.io-index|
|quote|1.0.47|registry+https://github.com/rust-lang/crates.io-index|
|rustversion|1.0.23|registry+https://github.com/rust-lang/crates.io-index|
|scopeguard|1.2.0|registry+https://github.com/rust-lang/crates.io-index|
|serde_core|1.0.229|registry+https://github.com/rust-lang/crates.io-index|
|serde_derive|1.0.229|registry+https://github.com/rust-lang/crates.io-index|
|simdutf8|0.1.5|registry+https://github.com/rust-lang/crates.io-index|
|smallvec|1.16.2|registry+https://github.com/rust-lang/crates.io-index|
|syn|2.0.119|registry+https://github.com/rust-lang/crates.io-index|
|syn|3.0.6|registry+https://github.com/rust-lang/crates.io-index|
|theater-mode-adapter|0.1.0|local project|
|thiserror|1.0.69|registry+https://github.com/rust-lang/crates.io-index|
|thiserror-impl|1.0.69|registry+https://github.com/rust-lang/crates.io-index|
|toml_datetime|1.1.1+spec-1.1.0|registry+https://github.com/rust-lang/crates.io-index|
|toml_edit|0.25.15+spec-1.1.0|registry+https://github.com/rust-lang/crates.io-index|
|toml_parser|1.1.3+spec-1.1.0|registry+https://github.com/rust-lang/crates.io-index|
|undname|2.1.2|registry+https://github.com/rust-lang/crates.io-index|
|unicode-ident|1.0.26|registry+https://github.com/rust-lang/crates.io-index|
|vtable-rs|0.1.5|registry+https://github.com/rust-lang/crates.io-index|
|vtable-rs-proc-macros|0.2.0|registry+https://github.com/rust-lang/crates.io-index|
|winapi|0.3.9|registry+https://github.com/rust-lang/crates.io-index|
|winapi-i686-pc-windows-gnu|0.4.0|registry+https://github.com/rust-lang/crates.io-index|
|winapi-x86_64-pc-windows-gnu|0.4.0|registry+https://github.com/rust-lang/crates.io-index|
|windows|0.61.3|registry+https://github.com/rust-lang/crates.io-index|
|windows-collections|0.2.0|registry+https://github.com/rust-lang/crates.io-index|
|windows-core|0.61.2|registry+https://github.com/rust-lang/crates.io-index|
|windows-future|0.2.1|registry+https://github.com/rust-lang/crates.io-index|
|windows-implement|0.60.2|registry+https://github.com/rust-lang/crates.io-index|
|windows-interface|0.59.3|registry+https://github.com/rust-lang/crates.io-index|
|windows-link|0.1.3|registry+https://github.com/rust-lang/crates.io-index|
|windows-link|0.2.1|registry+https://github.com/rust-lang/crates.io-index|
|windows-numerics|0.2.0|registry+https://github.com/rust-lang/crates.io-index|
|windows-result|0.3.4|registry+https://github.com/rust-lang/crates.io-index|
|windows-strings|0.4.2|registry+https://github.com/rust-lang/crates.io-index|
|windows-sys|0.61.2|registry+https://github.com/rust-lang/crates.io-index|
|windows-threading|0.1.0|registry+https://github.com/rust-lang/crates.io-index|
|winnow|1.0.4|registry+https://github.com/rust-lang/crates.io-index|

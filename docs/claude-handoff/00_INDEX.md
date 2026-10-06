# Claude developer handoff

Snapshot: **2026-10-06**, implementation commit **3d97070**. Repository: https://github.com/111tykatypka/EldenRingTheatreMode. Documentation branch: `claude-handoff`, created from `codex/native-bloodstain-replay-research`.

Elden Ring Theater Mode is a Windows offline structured gameplay recorder and replay editor. A C++ standalone host owns storage, playback clock and interpolation; a Rust DLL accesses the game through pinned bindings and native callbacks; C++ supplies an injected DX12 overlay. Real player capture and smooth transform playback are the established baseline. Complete animations, encounters and world reconstruction are **not verified**. Current work is an isolated native replay ghost lifecycle prototype. Previous create attempts were cancelled or timed out; the latest guarded native-step fix still requires a real game test. Preserve the stable Phase5 build.

## Evidence vocabulary

- **Implemented:** source exists, not necessarily runtime accepted.
- **User verified:** reported observation in the real game.
- **Static verified:** exact target disassembly/bindings support the statement.
- **Unverified:** missing runtime acceptance or other evidence.
- **Guess:** hypothesis requiring investigation.

Compilation, unit tests and Ghidra pseudocode are not in-game acceptance. Older appended reports retain their original statuses; explicit latest findings supersede them.

## Start here

1. Read `01_PROJECT_OVERVIEW.md`, `08_ROADMAP.md`, then `02_BUILD_AND_RUN.md`.
2. Read architecture and engine research before modifying callbacks or ownership.
3. Read replay format and UI integration before changing transport or editor code.
4. Use the reference inventory and decisions log to avoid repeating failed work.

|File|Purpose|
|---|---|
|00_INDEX.md|Entry point, evidence rules and repositories|
|01_PROJECT_OVERVIEW.md|Goals, scope, target and acceptance boundaries|
|02_BUILD_AND_RUN.md|Toolchain, dependencies, exact builds, launch and live test|
|03_ARCHITECTURE.md|Modules, threads, hooks, IPC and data flow|
|04_ENGINE_RESEARCH.md|Native replay backend, engine access, safety and dead ends|
|04a_OFFSETS_AND_SIGNATURES.md|Exact-target field, function, hook and signature tables|
|05_REPLAY_DATA_FORMAT.md|ERPLAY storage, typed tracks, native codec and fidelity limits|
|06_REFERENCES.md|External sources, local research paths and provenance|
|07_DECISIONS_LOG.md|Chronological decisions and owner preferences|
|08_ROADMAP.md|Bugs, acceptance gates and next five tasks|
|09_OPEN_QUESTIONS.md|Unresolved engine and owner questions|
|10_CODE_CONVENTIONS.md|Logging, errors, configuration, hooks, panels and working rules|
|11_UI_INTEGRATION_NOTES.md|Current ImGui layers and new UI integration contracts|
|inventory.json|Actual paths, types, measured sizes and repository membership|

## Repositories

The application has one active repository above. Earlier branch `phase4-in-game-replay-prototype` is historical; implementation currently resides at `3d97070` on `codex/native-bloodstain-replay-research`.

The dependency is https://github.com/KamiyamaShiki0704/fromsoftware-rs pinned to `3c8c1d7633a99309fb004c9f894ea10b7967d0e0`, originating from https://github.com/vswarte/fromsoftware-rs. This is a pinned revision, not an instruction to follow either default branch. ImGui is vendored from https://github.com/ocornut/imgui/tree/v1.92.5-docking. MinHook is vendored; exact upstream release/branch is Unverified.

Reference repositories and detected local checkout branches/revisions are listed in `06_REFERENCES.md` and its source index appendix. A reference repository is not another required application component. Unknown branch names must remain Unverified.

## Original working tree preserved

Before this task, `research/NATIVE_GHOST_LIFECYCLE_BLOCKER.md` was modified. `notes/DEVELOPER_TRANSFER_BRIEF_2026_10_06.md`, `research/ACCEPTANCE_MATRIX.md` and `research/ROLLBACK_RECOVERY_STATUS.md` were untracked. They were preserved and are not staged as implementation changes. The latest local lifecycle report is embedded with explicit local-evidence provenance. Only this documentation bundle is committed; no source or compiled artifacts are changed.

## Local application branches at snapshot

```text
* claude-handoff
  codex/ghidra-runtime-research
  codex/native-bloodstain-replay-research
  codex/nightly-research-integration
  codex/phase6-multicharacter-test
  codex/phase7-runtime-ui
  codex/player-action-replay
  codex/player-capture-fidelity
  main
  modern-theater-rebuild
  phase4-in-game-replay-prototype
  remotes/origin/HEAD -> origin/main
  remotes/origin/main
  remotes/origin/phase4-in-game-replay-prototype
```

## Actual local reference checkouts

Unknown branch remains Unverified; revisions are snapshot observations.

|Path|Origin|Branch|HEAD|
|---|---|---|---|
|C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\dx12-imgui-overlay|https://github.com/kacejot/dx12-imgui-overlay.git|master|e5087b986215d5c2092313458c68779eb4d99304|
|C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\Elden-Ring-CT-TGA|https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA.git|master|7926205c5a2ed236dd31278c4f5579c964ceec35|
|C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\EldenRingHKS|https://github.com/soulsmods/EldenRingHKS.git|main|d88d6441f5fccfdd6a5fd10d493309b680181897|
|C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\EROverlay|https://github.com/koalabear420/EROverlay.git|main|bb445e0ca507a43b2bd5988113b82db2c374c0c5|
|C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\FreecamMod|https://github.com/Logersnamed/FreecamMod.git|master|a4628aaf50d88feeda56f79573cf2842eddec54a|
|C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\Universal-WndProc-Hook|https://github.com/M0rtale/Universal-WndProc-Hook.git|master|e91001c114840ac13a260b3999a4837d2f00a63d|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\ELDENRING-INTERNAL|https://github.com/FriXeee/ELDENRING-INTERNAL.git|main|ab62d3b64495fec46c3ff0e0c1eca1fc52dcbc10|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\EldenRing-SDK|https://github.com/NightFyre/EldenRing-SDK.git|main|37652c02bf91c845aac14c0a4e1ee0f9a965df9c|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\EldenRingModLoader|https://github.com/techiew/EldenRingModLoader.git|master|d5c05cb4b6f5e18151355fa170b4ce5b85202165|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\EldenRingMods|https://github.com/techiew/EldenRingMods.git|master|c36d44ffb2226c12b98ea552726e56e1f223bd7d|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\ERGparamPreloadPatch|https://github.com/KamiyamaShiki0704/ERGparamPreloadPatch.git|main|928b1237c47d492733fb7bec77c26c82cffc6c70|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\ERSoundBankLoader|https://github.com/KamiyamaShiki0704/ERSoundBankLoader.git|main|86bb27cc6f01cc4497d3192e1ca5646efd6418d6|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\fromsoftware-rs|https://github.com/vswarte/fromsoftware-rs.git|main|59fbd3b3b7daaf14aca47c9f73530493dba6bc79|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\libER|https://github.com/Dasaav-dsv/libER.git|main|d8ae5c92719adca4344d8d902b63286b807f562b|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\nightreign_style_hud|https://github.com/KamiyamaShiki0704/nightreign_style_hud.git|main|215a2158bf32e5027dc9167368ca68bcee705368|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\EldenRingSeamlessCoopRelease|https://github.com/LukeYui/EldenRingSeamlessCoopRelease|main|27ea9d7e5f50b35097d53f0ed8c5d28cba121bab|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\fromsoftware-rs-kamiyama|https://github.com/KamiyamaShiki0704/fromsoftware-rs|main|7d4fdd66eda1a35284c170feb4b07b058c005f8f|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\fromsoftware-rs-vswarte|https://github.com/vswarte/fromsoftware-rs|main|59fbd3b3b7daaf14aca47c9f73530493dba6bc79|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\unseamless-coop|https://github.com/micthiesen/unseamless-coop|main|8faed9e1efb1e76e36d253e296daa051f4581253|
|C:\Users\user\.cargo\git\checkouts\fromsoftware-rs-7e356ae17759562a\3c8c1d7|file:///C:/Users/user/.cargo/git/db/fromsoftware-rs-7e356ae17759562a|master|3c8c1d7633a99309fb004c9f894ea10b7967d0e0|
|C:\Users\user\.cargo\git\checkouts\fromsoftware-rs-7e356ae17759562a\3c8c1d7|file:///C:/Users/user/.cargo/git/db/fromsoftware-rs-7e356ae17759562a|master|3c8c1d7633a99309fb004c9f894ea10b7967d0e0|

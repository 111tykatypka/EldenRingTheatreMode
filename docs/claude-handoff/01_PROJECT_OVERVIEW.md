# Project overview

Snapshot: 2026-10-06, implementation HEAD `3d97070`. Implemented does not mean live verified.

## Purpose

Record gameplay once, then produce multiple cinematic views without fighting the encounter again. Intended workflow: normal play → record internal game state/events → save `.erplay` → open replay → seek/play → independent camera track → cinematic shots. This is structured replay data, not OBS/video capture, screenshots or a controller input macro.

The ultimate scope includes player, relevant NPCs/bosses, actions, equipment, combat events, projectiles and world state. Reliable reconstruction takes priority over minimal file size. Camera tracks must remain separate from what happened in the world.

## Exact supported target

Windows x64, Elden Ring PC patch **1.17**, executable file and product version **2.7.0.0**, AMD64. Disk executable SHA-256:

```text
D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134
```

Game: `C:\Users\user\Downloads\ELDEN RING\Game\eldenring.exe`. Other builds must be rejected. Shared validation checks actual runtime executable identity rather than only a configured path. Hashing concerns the on-disk executable, not a relocated mapped image.

Loader: established **YAFSML** offline/modded workflow. The host launches through the existing loader and stages a config referencing its sibling DLL. Do not assume ModEngine2, dinput8 proxy or a custom injector. No anti-cheat bypass is implemented.

## Feature state

|Feature|Current status|
|---|---|
|Task runtime, WorldChrMan, main_player, transforms, pipe sampling|User verified at about 60 Hz|
|Structured streaming recording, validation and ERPLAY files|User verified baseline; writer/reader v2 and v3 implemented|
|Offline timeline, seek, stepping, interpolation, playback speeds|Implemented with historical tests/manual use|
|Runtime player transform replay|User verified smooth path on flat indoor terrain; character slid without animations|
|Standalone modern editor and injected overlay|Implemented; Insert overlay/status HUD observed; broad driver/DPI acceptance incomplete|
|Nearby actor identity/transform/presence and visual capture|Implemented experimental integration; not full NPC replay|
|Nine raw player fidelity tracks|Implemented and audited against real data; observations are not restoration APIs|
|Native recorder/payload analysis|Read-only recorder and native local callsite observed live|
|Native ghost create/remove|Implemented; no successful owned ghost or teardown verified|
|Full actions, damage, items, grace, boss replay|Not verified; raw capture does not reconstruct them|
|Free/follow/look-at/path/dolly camera, cinematic tracks|Planned/research; no verified complete camera backend|
|World checkpoints, AI/RNG/physics restore, streaming|Unresolved, not implemented as complete reconstruction|

## Preserved checkpoints

Stable behavioral golden master:
`C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase5`.

Latest isolated experimental package:
`C:\Users\user\Documents\ChatGPT\elden ring theater mode\outputs\EldenRingTheaterMode\NativeReplayGhostPrototypeStep`.

Do not overwrite Phase5, Tester or Nightly. Latest Git commit is not automatically the safest gameplay build.

## Owner requirements

Simple readable native controls; global hotkeys independent of console focus; resizable DPI-aware layouts; Unicode paths and English/Russian text; no arbitrary duration, resolution, replay-size or actor-count limits. Long recordings stream to disk. Show only measured performance numbers. Resource/page bounds are not product limits.

Owner requested simpler Play/Stop UI without Pause/Resume buttons; internal states and protocol still exist. Mouse wheel alone zooms both directions; Shift+wheel scrolls timeline tracks vertically. Owner must not be required to remember recording start location. Automatic preparation remains a goal, constrained today by scene validity and coordinate rebasing.

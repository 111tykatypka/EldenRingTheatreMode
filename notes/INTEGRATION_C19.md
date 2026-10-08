# Integration of the Codex cinematic / editor / timescale / weather work (C19) into the Claude core

Branch `claude/integrate-c19` (merge commit on top of `claude/npc-lifecycle` 884f6d7). Codex tip 9b957f5, common ancestor 194d68f.
Method: a real three-way `git merge` of the Codex history (all 21 commits after the ancestor), not whole-file copies.
Evidence labels: COMPILE_VERIFIED, TEST (a unit test ran), STATIC, UNVERIFIED (never run in the game).

## What came in (all of Codex's new work)
Free and Dolly cameras, channel camera model and curves, undo/redo, keybindings, viewport and gizmos, timescale, HUD controls, recorded-player aim,
Weather editor, near-plane close-up controls, ERTCAM project files, overlay protocol 12 (host and DLL together), sounds, new tests and the C10-C19 notes / research.
New files under `native_ui/` (camera runtime, MASM camera intercept, timing / HUD / weather adapters), `shared/` (camera math, track, hotkeys...) and `adapter/src/camera_probe.rs`.

## What was kept from the Claude core (untouched on purpose)
Recording and replay of the player, enemies, bosses and Torrent; actor lifetime / puppets / freeze; item-in-hand and weapon-location state;
opt-in equipment writes; arrival from any location with the frame logic and safe return; omission (full update rate);
and the camera-fade override (`camera_fade.rs`), which the owner confirmed working.

## The six overlapping files
| File | Result |
|---|---|
| `adapter/src/bone_replay.rs` | Conflict in `begin_arrival` resolved to the Claude logic (any-location arrival with frame conversion and grace travel). Codex's world timing bridge (`world_timing_tick`), bone-camera publish (`camera_bone_sample`) and the early return after a rejected arrival were merged automatically and are present. A failed travel now keeps the replay loaded (Codex's idea) through `reject()` instead of unloading it. |
| `adapter/src/arrival.rs`, `adapter/src/lib.rs`, `shared/GameProfile.h` | Automatic merge; both sides' changes present (camera / HUD / weather callbacks and constants next to the Claude tasks and offsets). |
| `native_ui/theater/TheaterOverlayUI.cpp` | Automatic merge; the Claude Settings checkboxes (puppets, full update rate, freeze, equipment, Torrent whistle, no near fade) sit in the Settings tool next to Codex's controls. |
| `native_ui/theater/TheaterStrings.h` | Conflict resolved as a union: Claude entries first, then Codex's `Weather`, `WeatherEditor` (enum and table in the same order; the size assertion holds). |

## Semantic collision: foliage
Codex `foliage.rs` cleared asset near-fade values 1 and 2 only while a Free or Dolly camera was owned. Claude `camera_fade.rs` clears every non-zero value
(including -1), the character-model fade ids, and applies from the moment Theater connects; in game the Codex version still faded and the Claude version works.
Two writers on the same table would overwrite each other's saved originals, so `foliage.rs` is now an inert stub and `camera_fade.rs` is the only owner.
Consequence: the camera editor's "prevent asset fade" switch no longer changes any table. The "No fade-out near the camera" setting (option bit 64, on by default) is the control.
The near-plane part of the close-up controls is in the native camera runtime and is unchanged.

## Known issues found while integrating (not caused by the merge)
- `render-backend-tests.exe` returns 7 on the **pure Codex C19 checkout as well** (checked in a separate clone): at 1280x720 the game viewport starts at y=95 while the `##camera-modes` bar ends at y=223 after the earlier Russian / 1.5x scale loop.
  Codex recorded `automated_tests=NOT_RUN` for C19. It may be a test-state artefact or a real overlap of the camera bar and the viewport; it needs a look in the game. All 14 other C++ test executables and the 71 Rust tests pass.
- Nothing from C18 / C19 has been run in the game by anyone yet (Codex's own manifest says so). Weather has static binding evidence only; custom lights are research-only.

## Validation checklist (in this order)
1. Normal world, all overrides off: no slowdown, recording works as before (player, enemies, Torrent).
2. Free camera: read, small controlled write, release; Player restore.
3. Dolly authoring (play and pause), keys, undo, preview, save / load project.
4. Timescale 1, 0.5, 0.25, 0.1, 0.05 with player / enemies / menus.
5. F4 viewport, focus and text gating, HUD toggle, sounds.
6. Weather: Rain / Sunny / Fog, Restore, release on focus loss and loading.
7. Claude regressions: NPC lifecycle, Torrent, item-in-hand, equipment off by default, arrival from far away, no fade near the camera.

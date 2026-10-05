# Modern rebuild status

Branch: `modern-theater-rebuild`, based on `81842b4` with verified Phase5 ancestor
`dbcc315567b4392699f38b6789f84283d5d59068`. Golden Phase5 output hashes checked,
unchanged. No resets, remote history changes, SDK dependency changes or game edits.

## Delivered implementation

- Official pinned Dear ImGui docking + Win32/DX11. Old GDI monitor not compiled.
- Docked library, recorder, custom drawn zoom/pan/scrub timeline, trajectories,
  inspector, bookmarks, launcher/settings/diagnostics, global hotkeys.
- Existing host ReplayPlayer/interpolation/SLERP and dedicated worker retained.
- Full duration default, old 15-unit hard failure removed at host and DLL; explicit
  warning confirmation in editor. Finite, version, target lease and sequence guards
  retained. Map compatibility remains unknown.
- Optional schema-1 character tracks in ERPLAY03; v2/v3 remain readable.
- Read-only bounded NPC discovery at PostPhysics, separate character pipe, source
  clock mapping, lifecycle-as-observed presence, raw animation observations.
- Character groups/selection/visibility and decimated trajectories in editor.
- Experimental earlier local action-input neutralization and read-only grounding
  diagnostics. Game log I/O moved to a bounded worker queue.
- Resume discards samples from before the resume source-clock boundary. Temporary
  replay filenames are reserved to preserve recoverable sessions.
- Recorder Stop is handled while a connected sample pipe is temporarily idle;
  disconnection finalizes the current session rather than waiting indefinitely.

## Evidence

Initial modern shell commit: `840cfda`. Release x64 build passed.
11 C++ tests passed: existing eight + cancellable character IPC, format/recovery/math tests and
headless loaded-player/character ImGui draw construction at five resolutions.
15 Rust tests passed, including character wire size/identity.
Final staged build repeats tests; authoritative results/commit/hashes in manifest.

Real file read and mock IPC full replay passed for the available 105119 fixture:
1069 samples, 17.8149115 s, 2 chunks, 63 raw action events. Old 022417 is deleted;
it was not regenerated or substituted with synthetic gameplay.

**Modern native capture/input changes are NOT runtime verified.** No actual NPC
recording from the new DLL or visual Elden Ring test has been performed in this
session. Earlier user-confirmed Phase5 motion is historical evidence only.

## Next engine checkpoint

Use RUNTIME_TEST_PLAN.md for one consolidated 20–30 second capture and playback
test. Capture must be proven before one-NPC replay ownership or gait work proceeds.
Grounding is instrumentation only; arbitrary Y fixes and root-motion zeroing were
not applied. Full AI/world replay and cinematic cameras remain future phases.

See IMGUI_UI_ARCHITECTURE.md, CHARACTER_REPLAY_DESIGN.md,
CHARACTER_RUNTIME_RESEARCH.md and CURRENT_RUNTIME_LIMITATIONS.md for details.

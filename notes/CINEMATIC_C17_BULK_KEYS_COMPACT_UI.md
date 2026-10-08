# C17 — Bulk Dolly interpolation and compact Camera tools

Base: `4564a19`, branch `codex/cinematic-editor-pass`.

## Changes

- Dolly curves now have **Selected / All keys**, an interpolation dropdown
  (default Smooth), **Apply**, and **Select all**. Existing Ctrl/Shift and box
  selection remain available. All keys includes off-screen keys.
- Bulk edits change only outgoing segment interpolation, preserving timestamps,
  transforms, FOV, easing and handles. Runtime validates the current replay and
  enum, replaces the track under its mutex, and records one undo snapshot.
  Reapplying an unchanged mode does not add history. Alt+Z undoes the bulk edit;
  Alt+Shift+Z redoes it. Save path persists the edited interpolation in `.ercam`.
- Camera tools use compact inline sliders, shorter labels, and collapsed Shake,
  Close-up visibility, Bone camera, Camera cuts, Keyframe details and Diagnostics.
  Removed redundant paragraphs, including the obsolete automatic-preview claim.
  Middle/right slider reset and numeric entry remain available.
- **Layout → Event Log** toggles the log. Its **Hide** button also hides it.
  Visibility is saved in overlay settings. Hiding reclaims tool-panel height;
  log collection and disk logging continue normally.

## Build and evidence

**COMPILE_VERIFIED:** Release x64 C++ host/native library and Rust DLL built successfully. Cargo emitted only its existing crate-name warning.
No tests were requested or run for this UI change. In-game layout, interaction,
undo, persistence and curve playback remain **RUNTIME VALIDATION REQUIRED**.
No camera hooks, world timing, actor replay or loading integration were changed.

## Quick manual check

1. Close the previous host and game; launch the C17 package with its adjacent DLL.
2. Open F4 and a replay with at least three Dolly keys. In curves choose
   **All keys → Smooth → Apply**. Confirm the curve updates; undo/redo once.
3. Select a subset with Ctrl or a selection rectangle, then apply a different
   interpolation using **Selected**. Confirm other keys are unchanged.
4. Save/load the path; check Camera sliders and collapsed sections.
5. Hide the log, restore through Layout, and restart to check visibility persistence.

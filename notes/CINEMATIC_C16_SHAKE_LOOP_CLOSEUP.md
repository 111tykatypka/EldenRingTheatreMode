# C16: unscaled shake, replay looping, camera close-ups

## Implemented

- Camera shake evaluates with elapsed monotonic real time, not ReplayTime. It
  continues with paused playback and ignores world/replay timescale. Active Free
  and Dolly overrides remain required; focus/player/IPC loss still releases them.
  Shake is added to the rendered pose only, so K saves unshaken camera nodes.
  Seek and loops no longer reproduce identical shake at a given replay timestamp.
- Playing timelines automatically wrap at replay duration, preserving overshoot,
  fractional nanoseconds and playing status. Action cursor is invalidated at a
  wrap. Camera clock extrapolation uses the same duration wrap, pending the next
  authoritative host update. Pause/Stop remain explicit; zero-duration replays
  pause rather than spin. Existing actor rewind/reconstruction limitations remain.
- Camera near-Z is exposed in Camera settings, .001 to 1 unit, default .01. The
  exact profile defines +0x58, validated by pinned CSCam offset compile assertion
  and the original camera-copy instruction bytes. Only active camera output is
  changed; the native input and far plane are untouched. Releasing overrides lets
  the original copy restore projection parameters without cached pointers.
- Prevent nearby asset fade uses pinned SoloParamRepository typed mutable rows,
  exclusively on the game callback. Only known AssetEnvironmentGeometryParam
  values 1/2 become 0 (Never disappear). -1 and unknown values are preserved.
  Original values and numeric generation tags are saved; restoration reacquires
  rows and only writes matching still-owned values. No game/regulation/save file
  is modified. Option defaults on while Free/Dolly overrides are active; leaving
  those modes, losing focus/host/player/offline context restores owned changes.

## Evidence / remaining foliage gap

See `research/FOLIAGE_NEAR_FADE_C16.md`. Native near clipping and shader camera
fade are separate systems. Exact visual coverage of the asset override and any
cached asset visibility state require live testing. This build does not claim
that every grass/tree/material fade is removed. GrassTypeParam dithering is logged
read-only; its public enum is only Type 0, not a documented no-fade toggle.

## Status and manual check

Release AMD64 EXE/native library/DLL compiled. No tests added/run in this task.
All new runtime/visual behavior remains unverified.

Package: `outputs/Cinematic-C16-shake-loop-closeup`.
Close old game/host, launch this package's EXE and adjacent DLL via existing YAFSML.

1. Active Free/Dolly camera: enable nonzero shake amplitude. Pause replay; shake
   must continue. Compare .1x, 1x, 2x: shake frequency should remain unchanged.
2. Play near the end: timeline should wrap and continue; Dolly should evaluate
   at wrapped ReplayTime. Check actor/equipment rewind behavior separately.
3. Approach grass/tree. Compare Prevent nearby asset fade on/off, near-Z .01/.1,
   then native Player camera. Report whether disappearance is gradual transparency
   or a sharp clipping plane and whether both grass and trees are affected.
4. Stop/switch Player/Alt-Tab: ensure normal camera and asset visibility restore.
   Send `%TEMP%/TheaterModeGame.log` FOLIAGE_OVERRIDE lines if foliage still fades.

Inherited camera settings and J/Alt+Z features are preserved.

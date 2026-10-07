# Timescale implementation вЂ” C6 replacement backend

This document supersedes copied historical claims about SmoothPose-WorldTimescale. The current active source has the adapter described here; preserved historical builds are unchanged.

## Architecture and source

- shared/TheaterTimescale.h: .001..10 double rate, log mapping, parser, exact IEEE-double IPC, ruler marks without snap.
- native_ui/theater/TheaterOverlayUI.cpp: continuous slider, relative Shift drag/wheel 2%, Ctrl 0.2%, direct numeric input, exact 1 reset with right/middle/double click. Reset changes rate only and never seeks/restarts/toggles playback.
- Host ReplayPlayer remains master; existing snapshots carry timescale and ReplayTime. Free camera uses real dt, Dolly uses host anchor/time/rate.
- native_ui/EldenRingTimingAdapter.cpp/.h: sole native scalar ownership; no timing DLL/clock hook, no old broken implementation.
- shared/GameProfile.h: two checked instruction RVAs DEB30F/DEBE2F, root 458DB58, scale offset 2CC. Root object RTTI: CS::CSFlipperImp (static COL proof).
- adapter/src/bone_replay.rs: small nonblocking state read and copied link check; adapter/src/lib.rs invokes bridge on verified game callback after bone tick. No IPC I/O on game callback.

## Ownership/state

Play/Restart/toggle commands automatically enable the native scalar backend; Stop disables it. The old Camera-panel opt-in checkbox is removed. Inactive startup never applies slowdown. Effective writes require owned Playing skeletal replay, no recording, fresh host (<250 ms), loaded player, existing offline guard and foreground game. Initial native scalar must be approximately1. No live gameplay-only independent slowdown is exposed yet.

Reacquire manager each callback; validate both opcode sites and agreeing RIP root. Saved scalar restored on pause/stop/loss/disabled if the pointer and observed last write still match. External modification or root replacement inhibits writes until inactive. No stale object restoration. Native binary32 precision is distinct from host binary64. Only changes write memory; status formatting once/sec; mutex uses try_lock, never blocks callback. Failure preserves native control.

No true native world pause implemented. Pause restores speed1; replay actors' existing hold behavior remains separate. UI/menu boundaries while active are UNKNOWN. DLL unload with installed hooks is unsupported: close the game before replacement. Abnormal game-task termination may prevent restoration; process exit clears memory but is not a guaranteed graceful unload.

## Evidence and testing

The exact reference setting-to-scalar chain, clamp discrepancy, constructor initialization and QPC camera clock are in CAMERATOOLS_DEEP_ANALYSIS.md. No native stability or all-subsystem coverage claim is made. Offline range/roundtrip/reset/clock tests validate sequencer math; they cannot validate Havok or menus. BUILD_MANIFEST.txt records compile results; runtime is UNVERIFIED.

C6 replaces the legacy file names/backend entry. The native scalar mechanism already matched CameraTools in C5; no second speed hook or old Rust timescale module is added. ReplayClock is retained because the native game scalar cannot replace ERPLAY timeline advancement.

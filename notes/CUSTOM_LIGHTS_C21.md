# C21 — Lights authoring and manual day/night

Branch: codex/custom-lights-prototype. Preceding checkpoint: 32fb534 (C20).

## Delivered

- Lights tab contains creator/editing controls rather than native collection statistics.
- Point/spot **definitions**, selection, move to camera, XYZ, spot pitch/yaw/roll, radius/intensity, RGB wheel, spot cone/softness.
- Shadow enable/quality-level/strength **definitions** and advanced source-radius, depth-bias, scattering and specular color.
- Versioned saved definitions: `%LOCALAPPDATA%/EldenRingTheaterMode/lights.ertlights` v2 (v1 loads).
- Native day/night request through public pinned `WorldAreaTime::request_time`; UI reports observed clock separately.
- Original-time request on explicit Restore, host loss or emergency stop where the same loaded clock context remains available.

**Custom-light definitions do not spawn or illuminate the scene yet.** The tab states NOT rendered; shadow properties are also definitions, not active renderer settings. This is a partial checkpoint, not completion of the requested native custom-light feature.

## Research advancement

Manager fade-completion erase/release is located at preferred VA 141A2B070. Native property UI 141AED460 maps true labels/offsets, correcting +90 from hypothetical color to source radius. Manager lock is non-recursive. Calling an allocation/removal routine in the wrong locked phase can deadlock. Native factory ABI, task ownership, shader units and retained-handle lifecycle remain blockers.

## Build and validation

Release AMD64 EXE + DLL compile. No automated tests were requested/run. No in-game or visual verification performed. No game/Claude core files modified. Prior C19/C20 packages preserved.

## Package usage

Close Elden Ring and previous Theater hosts. Start `outputs/Cinematic-C21-lights-editor/EldenRingTheaterMode.exe`, using the existing offline YAFSML workflow. Any explicit loader DLL path must use this C21 folder's TheaterMode.dll.

Load a known daytime exterior. F4 -> Lights exposes the new layout. Moving Day/night sends a native time request; observed clock should eventually follow. Area-specific lighting can prevent a sun change. Restore time requests the original time. This changes **native world time, which may be autosaved**; it is not render-only lighting isolation, and restoration across loading/process termination is not guaranteed.

Create Point/Spot currently creates an editor definition at the observed camera transform. Select it, move to camera, edit properties and save/load its setup. No visible illumination is expected yet. Do not interpret the creator button as a successful native allocation.

Diagnostic logs: `%TEMP%/TheaterModeGame.log` (LIGHTING_TIME), `%TEMP%/TheaterModeRender.log`; host log `%LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log`.

Next implementation: controlled native point-light creation/removal, then spot/shadow rendering. No request for a full custom-light runtime acceptance test until that backend is connected.

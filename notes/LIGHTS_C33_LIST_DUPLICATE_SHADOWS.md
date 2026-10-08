# C33 — light list, duplication and native shadow requests

Date: 2026-10-08. Branch: codex/custom-lights-prototype. No new commit or push.

## Changes

- Scrollable scene light table replaces the dropdown: on/off checkbox, selected light name, Point/Spot type and submission state. Full names and IDs are available on hover. Enabled-only filter; total/enabled/submitted counts.
- Status is backend submission telemetry, not proof of visible illumination. Defined = renderer disabled; Pending = enabled but no owned body reported; Submitted = backend reports an owned native body; Off = definition disabled. Cleanup can briefly retain bodies during loading or lock contention.
- Duplicate selected copies every definition property, position, orientation and shadow settings to a new unique ID and name, selects it and preserves all originals. No native object pointer is copied. The copy occupies the same position initially; move its viewport handle. Global rendering is not automatically changed.
- Experimental dynamic shadows toggle applies only to custom lights. Enable Cast shadows on each desired light; edit strength, required quality level and depth bias. Experimental global gate is off on startup; saved per-light intent does not automatically enable it.
- Existing day/night behavior untouched. Softness and scattering remain unavailable.

## Evidence / limitations

See research/FORCE_DYNAMIC_SHADOWS_REBORN_ANALYSIS.md for exact target disassembly and the distinction from the 1.16 data replacement mod.

STATIC_VERIFIED: native point and spot render packet eligibility reads shadow flag +AD, strength +94 and quality threshold +B8. Native property editor supports bias +B4.
COMPILE_VERIFIED: Release x64 C++ host/backend and pinned offline Rust adapter.
RUNTIME_VERIFIED / VISUALLY_VERIFIED: not yet performed for C33. No tests were added or run for this request.

## Package and manual check

outputs/Cinematic-C33-lights-list-shadows contains the updated EXE/DLL and prior sounds/LUT assets. Earlier packages are preserved.

1. Close the host and exit Elden Ring before replacing an in-use DLL. Point the existing YAFSML workflow at the new package DLL, then launch through the new package host using the established offline workflow.
2. Load a playable area, enable lights, create one point light near an object. Check row selection and on/off. Duplicate; verify a second row appears with copied settings, then move the selected copy.
3. Leave shadows off first. For the shadow check, enable Experimental dynamic shadows and Cast shadows on one small-radius light. Try required quality level 1 with the game shadow setting enabled. Compare with the checkbox off. This requests shadows; it is not a guarantee of visible results.
4. Repeat for one spot light. Watch FPS and shadow artifacts, then disable the gate and verify illumination remains. The gate does not force every world light to cast shadows.
5. Save/load setup to confirm independent light IDs and properties. Check the existing native cleanup on disable/stop.

Log: %TEMP%/TheaterModeGame.log (NATIVE_LIGHTS lines). Send the log and visible result if requests do not produce shadows.

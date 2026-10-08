# Particle Spawner C25

## Implemented

- Added a `Particles` toolbar tab beside Weather and Lights.
- Added a categorized catalog covering ambient, environment, weather, fire, smoke, magic, combat, water and custom effect labels.
- Added emitter definitions with stable IDs, enabled/disabled state, loop, duration, repeat delay, scale, intensity, position and pitch/yaw/roll.
- Added “Add emitter at camera”, “Move emitter to current camera”, save/load (`%LOCALAPPDATA%/EldenRingTheaterMode/particles.ertparticles`), selection and delete/clear controls.
- Added viewport emitter markers and simple camera-plane drag movement for the selected emitter.

## Deliberate limitation

The downloaded Force Dynamic Shadows Reborn reference contains only `regulation.bin` and compressed SFX bundles. It contains no native particle factory or effect-registration code. The particle tab therefore stores and edits emitter definitions, but does not claim to create an actual Elden Ring effect yet. The status text says this explicitly.

The catalog IDs are editor labels, not proven Elden Ring effect IDs. They must not be passed to the game until a target-version effect lookup/spawn path is found and guarded for Elden Ring 1.17 / 2.7.0.0.

## Build status

C26 follow-up: full C++ Release build passed using Visual Studio's developer environment and Ninja outside the sandbox. The Rust Release DLL linked successfully with the newly built static libraries. The particle lifecycle tests passed. No Elden Ring runtime or visual test was performed.

## Next native research step

Unpack the mod's SFX BND/DCX files and compare them to exact 1.17 originals with a version-aware SoulsFormats/Smithbox tool. Correlate effect IDs with runtime effect managers or existing public bindings. Add a separate guarded native adapter only after factory arguments, ownership and cleanup are statically and live validated.
## Runtime foundation added

The first runtime-safe layer is now present:

- `shared/ParticleProtocol.h` defines a versioned, finite-value-checked command contract for spawn, update, remove, and clear.
- `src/particle_track.hpp/.cpp` provides a standalone scheduler ready to consume ReplayTime. It supports duration, repeat intervals, looping, rewind cleanup, duplicate-ID rejection, and quaternion validation. It is not yet wired to native emission or the live replay clock.
- `src/particle_track_tests.cpp` covers scheduling, looping, rewind cleanup, and malformed rotations.

These commands are data only. The game adapter has not been given a raw CSSfx/FXR write path. A future DLL bridge must consume commands only on the verified game callback thread and reject them when the native effect subsystem is unavailable.

## C26 game-side integration

- Added `NativeParticleBackend` and `adapter/src/particles.rs` for opt-in native VFX inspection on the existing game callback.
- Added **Inspect native VFX** in the Particles tab. It reports whether CSSfx and the exact-target debug dispatcher are readable; this button does not spawn effects.
- Found and byte-verified the native debug dispatcher at RVA `0xD980B0` through the supplied CT. See `research/PARTICLE_SPAWN_RESEARCH_C25.md` and `research/particle_dispatcher_c26.json`.
- Creation reaches protected code. Native mode 1 cleans shared effect resources, so it is unsuitable for editor-owned remove/clear. Native spawning remains unavailable pending ownership/lifetime resolution.
- Fixed repeated per-frame spawn, sequence regressions, missing expiration remove, and repeat-delay handling in the previous scheduler. Clear commands refer only to editor-owned objects.
- Preset numbers remain explicitly editor template IDs, not engine FXR IDs.


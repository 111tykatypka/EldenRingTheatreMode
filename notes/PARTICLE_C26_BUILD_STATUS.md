# Particle system C26 checkpoint

Date: 2026-10-08. Branch: `codex/custom-lights-prototype`. Changes remain uncommitted.

## Result

COMPILE_VERIFIED: full C++ Release build and linked Rust Release DLL.

TEST_VERIFIED: particle lifecycle test executable passes spawning transitions, paused evaluation, repeat delay, duration expiry, seek cleanup, duplicate IDs, and malformed command rejection.

STATIC_VERIFIED: SHA-checked 2.7.0.0 image contains one match of the supplied CT's VFX dispatcher signature. RVA `0xD980B0`; mode 0 reaches `0xD978A0` then protected code at `0x54EA58E`; mode 1 reaches shared cleanup at `0xD91FD0`.

RUNTIME_UNVERIFIED: the added native inspection, UI layout and saved emitter behavior have not been tested in Elden Ring by this session.

NOT_IMPLEMENTED: native particle creation, per-emitter ownership/removal, world transform manipulation, engine FXR catalog mapping, and exact effect-age reconstruction on seek. This package is a buildable authoring/diagnostic checkpoint, not a functioning world-particle spawner.

## Package

`outputs/Cinematic-C26-particle-foundation` contains the new host EXE and DLL, copied sounds and the prior compatibility probe. Original C24 and other stable packages are preserved. These runtime pipe/settings paths are shared with the original application; do not run multiple host/DLL pairs simultaneously.

## Build

Native: Visual Studio 18 x64 developer environment, CMake Ninja, Release, `build-particle-c26-release`, `BUILD_TESTING=OFF`.

Adapter: `cargo build --manifest-path adapter/Cargo.toml --release --locked --offline --target x86_64-pc-windows-msvc` with `THEATER_NATIVE_LIBRARY_ROOT` pointing to `build-particle-c26-release`. Its `Release` and `shared/Release` directories contain copies of the newly built native libraries for the existing Rust build script.

Lifecycle tests: compiled directly with MSVC C++20 into `build-particle/particle-track-tests.exe` and executed successfully. The tests are synthetic protocol/lifetime fixtures, not gameplay evidence.

## Remaining native work

Trace the protected mode-0 creation path and identify the effect-instance handle, ownership, transform update, per-instance cleanup, and required task phase. The cheat table's all-effect cleanup must never substitute for editor-owned cleanup. The scheduler is ready to consume the master ReplayTime but remains disconnected until those bindings exist.

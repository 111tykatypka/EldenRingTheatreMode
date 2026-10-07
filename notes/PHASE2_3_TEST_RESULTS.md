# P2c test results — 2026-10-07

**No in-game test has been performed for this build.**

- Adapter: **28/28 passed**, Release x64, locked/offline Cargo dependencies.
  `cargo test --manifest-path adapter/Cargo.toml --release --locked --offline --target x86_64-pc-windows-msvc -- --test-threads=1`
- Host/native: **12/12 passed**, Release x64 CTest.
  Configure `build-native-ghost` with `BUILD_TESTING=ON`, build Release, then `ctest --test-dir build-native-ghost -C Release --output-on-failure`.
- Static reference-index utility: **1/1 passed**; Unicode, hierarchy, query, overwrite protection and connection cleanup.
- Covered additions: role matching guards, release-time bounds/gaps, invalid scalar reads/bools, mounted-state mismatch, context serialization/backward queries/malformed data, catalog+poses+context batch roundtrip, old world layout compatibility, dangling actor references and trailing chunk rejection.
- Existing regressions include ERPLAY, replay clock/player, playback worker, timeline, IPC, launch config and overlay/backend unit behavior.
- Not verified: live buddy enumeration/layout, mount relationship semantics, control flags for companions, ride physics versus pose writes, actual FPS/MB per minute, user-visible playback/Stop restoration, auto-warp with companions.

Build artifacts are experimental. Use `RUNTIME_TEST_PLAN.md`; preserve the original P2b package for rollback. Stop at this checkpoint for user results.

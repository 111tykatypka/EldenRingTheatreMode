# C13b: launcher startup readiness

## Root cause

C13a restricted F5/F6 registration to the foreground game, but the launch button
still required `app.stop_hotkey && app.sample_pipe_ready`. With no game running,
`stop_hotkey` was false by design. The button therefore stayed disabled indefinitely.
The same obsolete requirement was passed to the launch worker's preflight.

## Fix

Both UI readiness and launch preflight now use named-pipe server readiness alone.
Already-running and busy-launch guards remain intact. Dependency, AMD64, exact
EldenRing_1_17 version/hash validation and YAFSML configuration checks are preserved.
Recording shortcuts still register only while the connected game is foreground.
The waiting diagnostic now describes IPC availability, rather than hotkey setup.

## Delivery and verification

Package: `outputs/Cinematic-C13b-launch-readiness`.
Only the host was rebuilt. The DLL is unchanged from C13a.
Release host compilation succeeded. No new tests were added or executed for this
hotfix. Actual YAFSML/game launch still requires user confirmation.

Close the previous host, launch this package's EXE and use its adjacent DLL via the
existing YAFSML workflow. With the game stopped and the IPC server available, Launch
should be enabled even while this launcher or another application has focus.
After launch, F5/F6 should respond only when Elden Ring is foreground.

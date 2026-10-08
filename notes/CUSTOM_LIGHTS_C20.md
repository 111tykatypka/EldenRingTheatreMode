# C20 — Native light inspection checkpoint

Branch: codex/custom-lights-prototype. Base: 9b957f5036b48ff0dc9d5cfb8dfb58d77526dbae.

Release AMD64 compilation succeeded. No automated tests were requested/run. No game was launched or visually tested here. Existing source/core and earlier packages are preserved.

## What is delivered

Lights toolbar and read-only native inspector: actual manager discovery, collection counts, point/spot vtable classification, IDs, raw point spatial floats, paged observation and loading/disconnection handling. No actual custom light is spawned. Creation remains blocked by deferred renderer ownership/cleanup and callable ABI/task proof.

See research/CUSTOM_LIGHTS_RESEARCH.md for exact-target evidence, implementation and unresolved lifecycle details.

## Exact next runtime check

1. Close Elden Ring and all Theater hosts. Do not hot-replace a loaded DLL.
2. Launch outputs/Cinematic-C20-lights-inspection/EldenRingTheaterMode.exe.
3. Use the established offline YAFSML launcher. If the loader has an explicit DLL path, select TheaterMode.dll **from this C20 folder**, not C19/core.
4. Load any safe known location in the normal playable world. No replay is required.
5. Press F4 and choose Lights. Click Refresh. Check for a captured snapshot, A/B counts, and Point/Spot/Other rows. Empty counts can be legitimate; guard failures are not success.
6. Enable Monitor native lights. Move through an area with existing lights; inspect both collections/pages. A changing-collection message should recover on a subsequent scan.
7. Disable Monitor; the last snapshot stays until world/host context disappears. Load another safe area; confirm diagnostics do not retain old rows through loading, then Refresh again.
8. Close overlay; normal lighting and gameplay should be unchanged. This build never calls native light creation or writes lighting memory.

Send a Lights-tab screenshot, location, and LIGHTS lines from %TEMP%/TheaterModeGame.log. For crashes/startup failures also send %TEMP%/TheaterModeRender.log and %LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log.

Known limits: observation is best effort, not atomic; native pointer reuse can defeat the generation heuristic. Raw spatial units are not proven. No light color/intensity editing, light tracks or custom-light creation yet.

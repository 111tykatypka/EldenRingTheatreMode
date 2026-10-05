# Modern editor architecture

IMPLEMENTED, build verified; visual/runtime validation pending.

Golden source `dbcc315567b4392699f38b6789f84283d5d59068` is present in Git history.
No reconstruction from binaries was needed. Modern branches from `81842b4` (the
Phase5C diagnostic continuation). The historical Phase5 output is not modified.

Dear ImGui v1.92.5-docking is vendored unmodified from official commit
`3912b3d9a9c1b3f17431aebafd86d2f40ee6e59c`, including official Win32/DX11
backends and MIT license. https://github.com/ocornut/imgui/tree/v1.92.5-docking

`modern_main.cpp`: native window, DX11 swapchain, resize, DPI, single-instance lock,
global hotkeys, graphics device-loss shutdown. MultiViewport disabled. Layout and
settings live under LocalAppData, not the game directory.

`modern_ui.cpp`: docking, library selected by absolute path, recorder, timeline,
trajectory drawn with ImDrawList, inspector, bookmarks, launcher, diagnostics.
It copies backend state before NewFrame and dispatches queued actions after Render.
There is no GDI preview and no replay clock in the UI render loop.

`editor_backend.cpp`: extracts the existing monitor backend without changing the
verified sample pipe, launcher, ReplayPlayer or dedicated 120 Hz playback worker.
Workers never call ImGui. `replay_mutex` protects mutable player/controller state;
the renderer does not hold it. Filesystem operations and validation currently run
after rendering on the UI thread: very large library scans can delay UI input, but
cannot change the replay worker's clock. This is a remaining responsiveness limit.

The legacy `monitor.cpp` is retained as historical reference and is not built.
The normal executable target now builds the modern editor only.

Start distance: finite validation remains. The 15-unit hard failure was removed in
both host and DLL; UI warns above a configurable threshold and requires explicit
Play Anyway. Map compatibility remains unknown; no claim of safe cross-map playback.

Initial checks: modern Release x64 host built; existing eight C++ tests passed;
fourteen Rust tests passed after changing the distance test to explicitly cancel
its newly successful session before checking subsequent guard cases.

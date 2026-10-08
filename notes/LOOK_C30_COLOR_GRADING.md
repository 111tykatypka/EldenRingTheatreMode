# C30 — Look tab: real SDR color grading

2026-10-08. Independent checkout, branch `codex/custom-lights-prototype`.

## Implemented

Look -> Color grading:

- Enable color grading (off at process startup).
- Exposure -5 to +5 EV, default 0.
- Contrast 0 to 2, default 1.
- Saturation 0 to 2, default 1; 0 is monochrome.
- Reset color values; keeps the enable state and replay time unchanged.
- Middle/right click resets each slider; Ctrl+click accepts exact values.
- Disable checkbox gives an A/B comparison without losing values.
- Value persistence via existing overlay settings; enable is not persisted. Renderer resize retains current enable state while recreating fenced GPU resources.
- Existing character update-quality control retained in a collapsible Rendering quality section, with accurate experimental wording.

## Architecture

`native_ui/ColorGrading.h/.cpp`: thread-safe settings/status plus renderer-owned DX12 fullscreen triangle pass. No native camera/player/PARAM offsets. Shader source is original code, not copied from proprietary CameraTools. The existing host, named pipes, recorder and replay clock are unchanged.

`native_ui/TheaterRenderBackend.cpp`: uses the current per-backbuffer allocator/fence/scene texture, descriptor heap and direct queue. Copies completed scene into a shader-resource texture, applies grading to backbuffer, then draws Theater UI. In F4 editor mode it copies the graded image back into the viewport texture before ImGui draws the scaled viewport. This second copy prevents the viewport from displaying an ungraded image or applying grading twice.

No new Present hook, queue, timer or image-history loop. Resources are allocated per existing swap-chain slot and reused after its fence. Resize releases the pass only after existing GPU synchronization. Disabled/neutral values add no grading draw/copy; existing F4 viewport copies remain unchanged. Grading continues outside F4, including clean preview, if enabled with a connected loaded player and game focus. Unfocus or missing live player suspends it.

SDR guard: only R8G8B8A8_UNORM or B8G8R8A8_UNORM plus a containing output reporting SDR/sRGB via IDXGIOutput6::GetDesc1. Output check cached for one second. Unknown/HDR output, non-8-bit format, allocation or pipeline failure bypasses grading. This is a conservative output/format guard, not an inspection of an undocumented native tone-mapper or proof of every HDR configuration. Advanced-color monitors may be refused even with SDR game content.

Microsoft references:

- https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12
- https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/ns-dxgi1_6-dxgi_output_desc1

## Math / limits

Reads final display-referred scene, decodes sRGB, multiplies by 2^EV, applies linear contrast around 0.18, adjusts saturation around Rec.709 luminance (0.2126/0.7152/0.0722), clips to SDR range, encodes sRGB. Source alpha retained. Neutral settings bypass this math.

This is post-tone-map image exposure, not engine auto-exposure; clipped highlights cannot be recovered. Native HUD already rendered into the scene is graded; Theater UI is drawn afterward and remains unchanged. HDR/PQ/scRGB, LUTs, white balance, DOF, vignette, color-track animation and ReShade bridge are not implemented. No GPU performance numbers are claimed. Grading requires at least one full-resolution scene copy plus fullscreen draw; F4 viewport requires an additional copy. Memory/cost depends on resolution and swap-chain buffers.

## Build evidence

COMPILE_VERIFIED: C++ Windows x64 Release native library/host and linked Rust Release DLL using existing locked offline dependencies. HLSL VS/PS compiled with Windows SDK fxc (vs_5_0 / ps_5_0) as a shader build check. git diff --check passed. Runtime D3DCompile/PSO creation, queue behavior, actual colors, resizing and performance still require in-game validation. No automated implementation tests were added or run for this request.

Package: `outputs/Cinematic-C30-look-color-grading`. Earlier packages preserved.

## Exact manual check

1. Close old Theater host and Elden Ring to unload the previous DLL.
2. Run this package's EldenRingTheaterMode.exe and configure the existing YAFSML workflow to load the adjacent TheaterMode.dll. Launch through the existing offline workflow.
3. Load a playable world with game/display in SDR. F4 -> Look -> Enable color grading.
4. Set saturation to 0: scene should become monochrome while Theater UI retains its colors.
5. Reset saturation (middle/right click). Set exposure +1 then -1; brightness should increase/decrease. Change contrast and compare with Enable off/on.
6. Reset color values; image should return to neutral. Move sliders again, close F4 and check grading continues.
7. Reopen F4, resize editor viewport and game window; check graded viewport, no flicker/black frame and no cumulative darkening.
8. Disable grading; original colors should return immediately. Restart: saved values remain but Enable defaults off.

If unavailable, send the Look status text, HDR/display settings and `%TEMP%/TheaterModeRender.log`. If colors or performance look wrong, disable grading and use C29 while investigating. No game files are changed by this feature.

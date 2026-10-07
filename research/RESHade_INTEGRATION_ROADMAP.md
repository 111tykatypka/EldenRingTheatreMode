# ReShade integration / DX12 coexistence roadmap

2026-10-07. Research/design only. No ReShade installation or changes to the game folder.

## Existing renderer

[STATIC_VERIFIED] TheaterRenderBackend hooks DXGI Present, ResizeBuffers and swapchain creation through MinHook; renders the overlay before forwarding Present. It owns DX12 command lists/allocators, per-buffer fences and descriptors. A pending fence skips overlay drawing; resize waits up to two seconds and rejects unsafe resource release. These protections do not prove compatibility with another hook chain.

[UNKNOWN] Installed ReShade version, swapchain proxy ordering, descriptor/state conflicts, resize/fullscreen coexistence, HDR/color space and input capture interactions. No joint runtime/visual test occurred. Hook success is not evidence that both overlays render correctly.

## Recommended approach

Prefer an optional ReShade add-on bridge where the user's installed build supports it. Keep the core recorder independent. Use documented effect-runtime lifecycle and typed uniform APIs; pin the compatible API and expose a capability handshake. Official interfaces: [reshade_api.hpp](https://github.com/crosire/reshade/blob/main/include/reshade_api.hpp), [reshade_events.hpp](https://github.com/crosire/reshade/blob/main/include/reshade_events.hpp). This supports a design proposal, not a claim that an add-on has been integrated.

ReShade's DXGI implementation provides swapchain/proxy handling: [dxgi.cpp](https://github.com/crosire/reshade/blob/main/source/dxgi/dxgi.cpp). Study actual installed ordering before altering our hooks. Avoid adding another uncoordinated Present interception merely to animate effect parameters.

A PostProcessTrack stores effect/uniform names, types, values and explicit interpolation rules. Evaluate from master ReplayTime; resolve handles after effect reload; reject missing/type-mismatched uniforms. Preserve prior user values and restore only handles still owned by the same runtime generation. Never redistribute user shaders/presets without appropriate permission.

## Limits / validation plan

Temporal shaders accumulate history: setting uniforms at a new timestamp does not reconstruct earlier frames. Seek may require a documented history reset or pre-roll; otherwise mark output approximate. Depth-buffer availability and focal units must be verified per shader; do not present invented physical aperture/DOF controls.

First user-supervised matrix: Theater only, ReShade only, both; repeat windowed/borderless, resize, overlay toggles, depth access and replay stop/unload. Inspect fences, device errors and input ownership. Only after stable coexistence, test a single typed parameter with seek and restoration. Runtime/visual compatibility: UNKNOWN.

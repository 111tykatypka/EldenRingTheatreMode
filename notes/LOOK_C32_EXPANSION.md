# C32 — Look effects and wind continuation

## Status

COMPILE_VERIFIED: C++ Release x64, Rust Release x64 locked/offline, HLSL VS/PS strict optimized shader compilation. Runtime / visual testing NOT performed. No automated tests added or run in this task. Existing Rust crate-name warning only. Independent branch remains `codex/custom-lights-prototype`; changes are uncommitted with the prior C25–C31 work. No push or changes to main/original builds/game files.

Package: `outputs/Cinematic-C32-look-wind`.

## Look controls

- Color: existing exposure/contrast/saturation plus separate vibrance [-1,+1]. Vibrance scales chroma more strongly for less saturated pixels; this is an original heuristic, not a proprietary grading algorithm or skin-protection model.
- LUT: asynchronous load of UTF-8/Unicode path, unload, 0–100% blend. Paste a filesystem path without shell quotes. 3D `.cube` only, dimensions 2–256 per Cube specification, red index fastest. Supports TITLE and DOMAIN_MIN/MAX. Rejects malformed/incomplete data, duplicate/late headers, nonfinite values, combined/1D tables and input-range variants. Existing LUT retained on load failure. One loader at a time, cancel on unload. File budget 1 GiB, line budget 4096 bytes prevent runaway allocation/parsing; these are parser safeguards, not recording limits.
- Lens: vignette amount/radius/softness; radial chromatic aberration in pixels; barrel/pincushion lens distortion. Clamp at image edges; no overscan/crop compensation yet.
- Texture: monochrome film grain amount/size/speed (0 freezes grain); four-neighbor linear-light sharpening. Grain uses wall-clock time independently of replay/timescale. It is a procedural grain approximation, not a measured film-stock simulation.
- Right/middle click sliders reset individual defaults. Reset Look values resets numeric controls, keeps master enable unchanged and LUT loaded. Master enable provides bypass. Numeric settings persist; enable OFF on startup. LUT file selection itself is session-local and must be loaded again after restart.
- Compact collapsible Color/LUT/Lens/Texture sections reuse current scrollable tool panel. Existing rendering-quality controls retained.

## Renderer details

Existing DX12 queue and backbuffer fence remain authoritative; no new Present hook. One full-screen pass after native tone mapping, before Theater UI; graded scene is also copied into the editor viewport. Native game HUD belongs to the scene and receives effects. Theater widgets remain unaffected.

Pipeline order: warped/aberrated image -> optional linear-light sharpen -> exposure/contrast -> saturation/vibrance -> SDR encode -> trilinear Cube LUT -> vignette -> grain -> SDR clamp. LUT expects display-referred SDR RGB, not a camera-log input. Out-of-domain inputs clamp to LUT edges; output clamps to SDR.

LUT source is immutable CPU data published from a worker. Each backbuffer slot owns an immutable upload-buffer SRV and metadata. Replace that slot's LUT only after the existing renderer has verified its fence complete. Retain other slots until safely reused; release on existing fence-safe renderer cleanup. No in-flight descriptor rewrites. Root signature uses a scene SRV table, 28 constants, a raw root SRV and a clamp-linear static sampler. LUT disabled uses a valid dummy buffer.

Default effects are neutral/off and bypass the pass. Sharpen/aberration add scene reads; LUT adds eight buffer reads per pixel. Large LUTs retain one upload copy per backbuffer and may cost significant memory/bandwidth. Performance has not been measured in game. Existing conservative SDR RGBA8/BGRA8 + output-color-space guard remains; HDR is unsupported.

## Remaining Look work

Bloom, glow/diffusion and clarity/local contrast need properly scaled intermediate/downsample/blur passes and GPU lifetime management; not implemented here. They should not be advertised as native HDR bloom when derived from the already tone-mapped scene.

Native fog density/color/start/end require exact profile-bound scene parameter discovery. Camera near/far clipping is not fog distance. No fake fog sliders or unverified engine writes added.

## Wind continuation

See `research/WIND_SYSTEM_C31.md` plus `research/wind_force_flow_c32.json`. New `tools/wind_force_trace.py` regenerates a targeted hash-checked disassembly/caller export.

Static verification confirms native lookup 141CAEAD0 reads registry active +118, table +20, slot count +AC. Registry global VA 1447FA308 referenced by wind-related accessors. Native appearance pointer is at record +48. 141C956B0 obtains a 4x4 result through appearance vtable +118; 141C957D0 obtains another value through +198. 141CB32D0 derives a changing value through 141C972E0, which uses virtual getters +100/+F8/+160. Do NOT label these as force/direction/strength from their numeric values alone.

Weather > Wind diagnostics adds opt-in read-only observation of registry slots/readable appearance records. Runs once a second on existing callback, guarded exact-build lookup bytes in GameProfile, loaded/offline/focus/host checks, ReadProcessMemory only. Samples up to 256 slots as a bounded diagnostic budget, not a gameplay or recording limit. Rechecks root/table/count and discards visibly changed snapshots. This does not guarantee an atomic registry snapshot; no virtual calls, retained/dereferenced native pointers or force writes. Logs `WIND_INSPECT` on status transitions. Runtime presence is still unverified.

Direction, native cloth and arbitrary physics-force controls remain UNKNOWN/unimplemented. Existing C31 response-strength implementation is preserved. This diagnostic is the next step toward verifying a native wind binding, not completion of global wind control.

## Sources

Cube structure/order/domain handling follows [Adobe Cube LUT Specification 1.0](https://kono.phpage.fr/images/a/a1/Adobe-cube-lut-specification-1.0.pdf), an Adobe-authored document hosted as a mirror. Implementation and example LUTs are original; no sample implementation copied.

## Files

`native_ui/ColorGrading.h/.cpp`, `native_ui/TheaterRenderBackend.cpp`, `native_ui/theater/TheaterOverlayUI.cpp`, `native_ui/WindController.h/.cpp`, `adapter/src/wind.rs`, `shared/GameProfile.h`, `tools/wind_force_trace.py`, wind evidence, this note and separate package.

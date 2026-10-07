# Rendering references and feature availability — C4

Date: 2026-10-07. Research does not establish visual runtime verification.

## CameraTools v1018: actual decompilation examined

[CONFIRMED] The existing Ghidra export is saved at `../research/igcs-camera-v1018` relative to this independent repository. `PROVENANCE.json` identifies the supplied DLL as SHA256 `1A1DA1FDBEB9F3EB85EF29469FF9EE2A1322108D47F107C11F137D4E13A077EB`. Managed WPF client sources and native pseudocode/index are already retained. Re-exporting the entire DLL adds no evidence.

[CONFIRMED] Querying native function `18020a1c0` shows registrations named `AOB_CAMERA_ADDRESS_INTERCEPT`, `AOB_CUSTOM_ASPECT_RATIOS_ADDRESS` and `AOB_PLAYER_COORDINATES_INTERCEPT`, including the camera-write instruction pattern. It associates the camera intercept with DLL global `1802a19a0` and stub `18006c110`; these are reference-DLL preferred addresses, **not game addresses to call**. Pseudocode constructor types remain inferred. Managed settings and two named pipes document external client/game module separation. No recovered code or binaries are copied into Theater Mode.

[CONFIRMED] Exact-game disassembly at preferred VA `140681970` loads GameRend output cameras and copies camera fields, including the float at `+0x50`. The independently installed hook uses this profile-guarded function and checks its bytes before enabling. Matrix basis, FOV conversion and hook/read/write results are independently validated. The public [CameraTools documentation](https://opm.fransbouma.com/Cameras/eldenring.htm) establishes the reference feature set, not our feature verification.

[IMPORTANT CORRECTION] The old reference memory note's time-manager slot `0x358DB58` is superseded by the exact-game disassembly result `0x458DB58` in `REFERENCE_CAMERA_TIMING.md`. Do not import the stale address. This pass does not change the existing timescale implementation or patch pause mechanisms.

## FreecamMod: actual source is the stronger reference

[CONFIRMED] Source saved at `../research/community-nightly/FreecamMod`, revision `a4628aaf50d88feeda56f79573cf2842eddec54a`. [Upstream](https://github.com/Logersnamed/FreecamMod) supplies MIT-licensed source, so source inspection provides types and intent more accurately than decompiling a build of unknown provenance. No additional reference binary was installed or executed.

Inspected `src/core/free_camera.cpp`, `src/core/game_data/field_area.h` and timeline/input design. The source uses a debug camera plus native mode enum, matrix basis columns, radian FOV, real delta, velocity decay, state save/restore and independent freeze options. Its layout places cameras at GameRend +18/+20/+28 and debug camera +D0, with enum +C8. These offsets are source evidence for its supported build, **not authorization to write every field in our target**. Theater Mode currently overrides only the verified copied output matrix/FOV, leaving native debug mode and freeze flags untouched. Independent exponential movement damping and the existing pure spline evaluator avoid copying implementation code.

## Custom lights

[CONFIRMED] Exact-target RTTI queries return `GXSR::GXPointLight` at preferred VA `143d2d9b0`, `GXSR::GXLightManager` at `143d2d518`, and a light manager task wrapper at `143d2e430`. Existing shader research contains point/spot lighting passes. These identify subsystem candidates.

[UNKNOWN] The SQLite xref query for the point-light type name returned no entries; incomplete analysis and PE-relative RTTI mean that result does not prove no references exist. Construction/register/deletion ABI, renderer ownership, shadows and unload task are still unproven. A callable factory cannot be inferred from a type name or shader. Native light spawning stays unavailable. Next targeted proof: resolve PE-relative type descriptors/COLs/vtables, inspect actual allocating callers and paired deletion/map-unload callers, then read-only enumerate owners on the proven task. Do not call an inferred constructor from the render/IPC thread. Existing `research/CUSTOM_LIGHTS_RESEARCH.md` details track/reconciliation design.

### Additional binary proof obtained during this pass

`tools/ghidra_query/resolve_rtti.py` now resolves MSVC x64 relative complete-object locators from the hash-verified PE. It validates locator signature and self RVA, then follows absolute locator pointers to candidate virtual tables with executable entries. Saved result: `research/cinematic_rtti_c4.json`. These are preferred VAs, not ASLR runtime pointers.

| Type | Descriptor | COL | Virtual table |
|---|---|---|---|
| GXPointLight | 143D2D9A0 | 1433B5588 | 142F15630 |
| GXSpotLight | 143D2D9F0 | 1433B5668 | 142F15790 |
| GXLightManager | 143D2D508 | 1433B4CD8 | 142F116C8 |
| GXFfxSceneCtrl | 143D37F28 | 1433C7310 | 14305D5E8 |

[HIGH CONFIDENCE] Point-light construction candidate `141A44DF0` assigns the verified vtable and initializes embedded state. It has callers `141A2A8E0`, `141A2AA10`, `141B19D40`, `141B1A730`. The first two wrappers allocate 0x1F0 bytes aligned to 0x10, construct, acquire/release a reference-counted subobject, and pass the object to `141A27D20`. `research/point_light_factory_disasm_c4.json` preserves actual wrapper machine instructions; `research/point_light_registration_c4.json` preserves the candidate registration function for comparison. Descriptor names/meaning of four-component constructor data are not established; do not label them RGB/position without further evidence.

[HIGH CONFIDENCE] `141A44E60` is a point-light destructor candidate. Virtual slot one `141A44E90` assigns the point vtable, calls embedded cleanup at +1B0 and base cleanup, then conditionally deallocates 0x1F0 bytes when flag bit one is set. Exact disassembly corroborates this structure. Slot zero `141A45220` calls virtual slot one with flag zero then invokes a separate allocator interface; it is not a safe general delete API to call blindly. Caller context and ownership must be preserved.

[UNKNOWN] Although these are substantially stronger lifecycle candidates than RTTI strings, the native manager instance, list locks/task affinity, exact descriptor ABI and teardown fencing remain unproven. No allocation or reference-count changes are executed in the game. Next targeted addresses are `141A27D20` and the actual light-manager update/remove paths, not generic whole-database searches.

## LUTs and post-processing

[IMPLEMENTED / CPU ONLY] `shared/CubeLut.h` parses an independent subset of 3D .cube: TITLE, LUT_3D_SIZE, DOMAIN_MIN/MAX, comments and red-fastest RGB sample order. It validates finite values, exact lattice size, unsupported directives and domain extent. It performs trilinear interpolation with domain clamping. The 2–256 edge bound controls import memory; 1D/shaper/combined formats are explicitly unsupported. Tests use identity and invalid-data fixtures. This does **not** yet alter the game's pixels.

The [OpenColorIO IridasCube reader](https://github.com/AcademySoftwareFoundation/OpenColorIO/blob/main/src/OpenColorIO/fileformats/FileFormatIridasCube.cpp) is the primary format reference; no code was copied. Real grading requires a typed color resource/3D texture and shader, DX12 descriptor/resource-state/fence ownership, declared linear/sRGB/HDR conversion and a verified placement in the render chain. An ImGui control alone is not a grading implementation. The existing backend only draws UI; scene-copy, HDR and depth access are not established. DOF/aperture/motion blur controls remain unavailable where no reliable engine or effect API exists.

## ReShade integration

[CONFIRMED API] Official [effect-runtime API](https://github.com/crosire/reshade/blob/main/include/reshade_api.hpp) provides effect/uniform enumeration, `find_uniform_variable`, typed metadata and `set_uniform_value_float`. Performance mode can replace uniforms with constants, so discovery must fail visibly rather than invent a handle. Setters do not automatically save a preset; the bridge must not overwrite user presets. [Events](https://github.com/crosire/reshade/blob/main/include/reshade_events.hpp) provide lifecycle/effect callbacks for an optional add-on.

[DESIGN] Use an optional, version-pinned add-on bridge that publishes capability/runtime-generation information to Theater Mode. Resolve typed handles after effect load; evaluate tracks at host ReplayTime and apply on the documented ReShade callback, capture prior values and restore only within the same generation. Effect history/depth are separate capabilities. Seek needs history invalidation/pre-roll where supported. Do not pretend a typed DOF uniform is a physical lens measurement.

[UNKNOWN] No installed ReShade version/capability or joint DX12 hook ordering is verified. ReShade was not installed into the game directory. Native grading and the add-on bridge remain unimplemented; compatibility is not claimed. See `research/RESHade_INTEGRATION_ROADMAP.md`.

## Bone camera

[IMPLEMENTED] Current-player selected bone + model root is copied read-only after the existing Draw_Pre bone replay callback. Camera offset uses the bone's composed basis. Root/quaternion math tests pass. F6/context loss restores the native camera. [UNKNOWN] Visual coordinate alignment and render-phase lag; no universal head bone or NPC-bone targeting is claimed. See camera editor report for exact data flow.

## Particle spawner

[CONFIRMED] Pinned SDK exposes CSSfx/WorldSfxMan/GXFfx resource/container layouts. Exact-target RTTI includes `GXFFX::GXFfxSceneCtrl` at preferred VA `143d37f38`. Local `../research/references/fxr-ws-reloader/fxr_reloader/patcher/src/game/game_data.rs` shows CSSfx traversal to a resource manager and FXR definitions, allocator/prepare/patch operations. This is **resource reloading, not a validated live emitter creation/removal API**.

[UNKNOWN] Native effect-instance constructor ABI, attachments, task ownership, reference count, seed/state and map-unload deletion. Never equate FXR resource loading with particle spawning. No debug-spawn field writes or guessed calls are added. Next proof follows CSSfx scene instance creation, attachment handles and deletion completion; an owned emitter must be created once per lifecycle interval and reconciled on seek, not spawned every frame. Non-deterministic particle simulation needs state capture or an explicitly approximate restart. Existing `research/CUSTOM_PARTICLE_ROADMAP.md` records this architecture.

## Remaining development, not hidden behind controls

Camera track persistence/numeric editing/Bone backend are built; render-world path gizmos, actor Follow/LookAt selection, persistent settings, camera-motion recording and content-hash sidecar identity remain missing. GPU LUT grading, ReShade add-on, native lights and particles remain unavailable. Runtime visual verification of new camera features has not occurred in this pass. No new game test is requested here.

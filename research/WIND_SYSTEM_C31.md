# Wind control research / C31 implementation

Target: exact Elden Ring 2.7.0.0 / patch 1.17 profile and existing pinned SDK. Separate independent build; game files and original packages unchanged.

## What is actually implemented

COMPILE_VERIFIED: Weather > Wind response has a 0–3x native **response strength** multiplier, enable and restore. It scales `GrassTypeParam.wind_amplitude` and recognized wind-enabled `AssetEnvironmentGeometryParam.wind_effect_rate_0/1` relative to originals. This is not wind velocity, a global wind vector, or a guarantee of physics forces on every object.

STATIC_VERIFIED: saved Paramdex ER `Defs/GrassTypeParam.xml` describes windAmplitude as sway amplitude and windCycle as cycle/speed. Grass orientationAngle is in the placement/model-distribution fields; its name does NOT prove wind direction. It is deliberately untouched. SDK public getters/setters exist at the pinned revision. AssetGeometryParam's two wind rates are pre/post destruction response coefficients. Its wind-type values are preserved, not forced on unsupported models.

UNVERIFIED: loaded renderer objects may cache these PARAM values. A successful row write does not prove the visible grass changed. Secondary grass LOD PARAM tables are not changed. Zero original strength remains zero; u8 grass amplitude saturates at 255. Response multiplier range 0–3x is a conservative prototype UI bound, not an engine wind limit.

## Threading, ownership, restoration

`Weather UI -> WindController -> request snapshot -> Rust wind::tick -> freshly acquired SoloParamRepository -> typed rows`.

Only existing PostPhysics game callback writes. No raw native offsets were added. Repository and row numeric addresses are identity checks only; they are never dereferenced from saved state. Owned originals are restored if the newly acquired row has the same identity and still holds our last written value. External edits are preserved. Once a value changes or enable changes, enumerate/apply; no per-frame whole-table scan. Restore on loading, focus loss, host loss, explicit restore and overlay emergency stop. If repository is temporarily unavailable, retain originals for a later restore attempt; if replaced, abandon stale identities. No disk PARAM/save mutations.

Strength is persisted; enable defaults OFF at startup. Invalid values disable requests. Diagnostics report applied row counts and `WIND_RESPONSE` lines in `%TEMP%/TheaterModeGame.log`. SDK panic marks faulted and stops application. Rust catch_unwind does not catch access violations; rely on verified SDK table validation and exact profile, not catch_unwind as memory safety.

## Native wind / cloth investigation

STATIC_VERIFIED observations in exact hash-checked analysis copy:

| Evidence | Finding / limit |
|---|---|
| `WindStrength` string VA 142bc83a0; reference 140dc383b | Dynamic name-table load in constructor 140dc3430. This reference is not a directly writable wind scalar. |
| `Disable Wind For Plants` VA 14305f310 | Debug registration 141ca2602 passes byte address 1447fa35a; consumer 141c95f69 tests it. Direction/strength not exposed here. |
| RTTI `CSFD4WindForceSfxIns` VA 143d059b8 | Type descriptor/COL scan yields vtable 142bc1c48. Constructor 140d8c7a0; creation caller 140d91d10. This is a native wind-effect candidate, not a verified safe allocation API. |
| Virtual slot 7 -> 140d8d9c0 | Copies an 0x50-byte descriptor and calls 141c94e60 for live instance. Descriptor layout/lifecycle not established; not invoked. |
| 141c95cc4..141c96354 | Native render wind producer gathers values from active records into 0x80-byte wind buffer entries. Accessors include 141c956b0 (transform-like data), 141c957d0, 141cb32d0. Exact public ABI and ownership not established. |
| Read +D450 double in graphics object | Used in wind buffer generation. Could be timing; NOT proven wind strength and deliberately untouched. |
| `hclSimpleWindAction`, windDirection/minSpeed/maxSpeed/frequency reflection names | Havok has cloth wind representation. These names do not establish how to acquire live action instances safely. Older HKX2 layouts cannot be ported blindly. |
| WeatherParam.WindSfxId | Outdoor wind FXR ID, not a scalar. Changing that ID alone cannot give continuous direction/strength controls. |

Saved evidence: `wind_trace_c31.json`, `wind_debug_disasm_c31.json`, `wind_strength_disasm_c31.json`, `wind_update_disasm_c31.json`, `wind_update_body_c31.json`. Disassembly checked against hash-validated analysis image. Partial Ghidra export is a candidate index, not full ABI proof.

Saved DSMapStudio help identifies WindArea regions as applying wind physics to cloth and WindSFX as visual wind. These references identify separate subsystems, not a single universal global control. Existing particles use an owned native FXR handle but do not yet establish WindForce descriptor scaling/rotation.

## Remaining work

C32 continuation: `tools/wind_force_trace.py` and `wind_force_flow_c32.json` verify native registry lookup/accessors in disassembly. Optional Weather > Wind diagnostics now reads the registry using exact GameProfile guards, without virtual calls or native writes. See `notes/LOOK_C32_EXPANSION.md` for fields, observation limits and current runtime status. Direction/cloth still unavailable.

1. Read-only instrumentation of active native wind records and descriptor identity/lifetime.
2. Verify field roles by callers and setters before rotating/scaling any instance. Trace whether foliage and Havok consume the same force field.
3. Establish native synchronization/update phase; render-buffer production may run on another thread. Do not write its arrays in PostPhysics without a concurrency proof.
4. Add direction and cloth controls only after above; currently explicitly unavailable in UI.
5. Visually compare grass/trees at 0x, 1x, 2x and restore, with camera fixed and same weather. If no effect, investigate cached renderer data rather than declaring success from row counts.

Build: C++ Release x64 and Rust Release locked/offline succeeded. Only existing Rust crate-name style warning. No automated tests or live game tests executed for this request.

External source consulted: [GPARAM authoring reference](https://soulsmodding.com/doku.php?id=tutorial%3Amodifying-gparams), which describes area/weather graphics overrides. It does not document a live global cloth-wind API.

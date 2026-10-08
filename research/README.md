# Persistent research reference library

Start here when continuing camera, lighting, weather or particle work in the independent project.
Updated 2026-10-08. This is filesystem project memory, not a promise that another assistant automatically remembers chat history.

## Saved references and provenance

`research/REFERENCE_MANIFEST.json` records the six current local repository snapshots (URLs, exact commits and dates), the downloaded FXR CSV fingerprint and generated catalog fingerprint. Refresh with `python tools/reference_manifest.py`; this reads local metadata only.

`research/references/` contains Yabber, SoulsFormats, BinderTool, Paramdex, DSMapStudio and misspia-elden-ring-data. Treat them as reference checkouts. Do builds/conversions in separate scratch/output directories when practical; preserve original inputs and exact-version snapshots. No reference code needs decompilation when its source is already present.

Detailed inventories/findings:

- `research/ARCHIVE_PARAM_TOOLING_REFERENCE.md`
- `research/format_tools_reference_inventory.json`
- `research/MISSPIA_ELDEN_RING_DATA_REFERENCE.md`
- `research/misspia_elden_ring_data_inventory.json`
- `research/SOULSMODDING_REFERENCE_INDEX.md`

Public links are recorded there, including the Souls Modding hub, FXR ID and component-notes spreadsheets. The ER ID tab CSV is saved as `research/fxr_reference_sheet.csv`; the FXR Notes document has only been inspected through its introduction, not fully exported/decoded.

## Current particle resume point

C29 output: `outputs/Cinematic-C29-fxr-reference`. Matching host and DLL; original builds preserved.

The user confirmed C27 visible effects, including 7191190. Current native preview owns one stable handle, calls native create/stop/release on the existing game callback, and expires after two real seconds. This does not establish full multi-emitter/timeline spawning, all-effect coverage or density controls.

`notes/PARTICLE_C27_NATIVE_PREVIEW.md` — API discovery and controlled preview.
`notes/PARTICLE_C28_CATALOG.md` — search/favorites/custom labels.
`notes/PARTICLE_C29_REFERENCE.md` — embedded community descriptions.

`native_ui/ParticleReferenceData.inc` is generated from the CSV with `tools/import_fxr_reference.py`. User names are kept separately in LOCALAPPDATA. 7191190 has bank/resource metadata but no descriptive text in that snapshot. Do not invent a semantic name.

Next useful research: decode the actual FXR resource, investigate documented emitter parameters, then implement owned transform/lifetime updates and connect to master ReplayTime. Do not mutate shared definitions to change one authoring emitter without understanding resource ownership.

## Runtime research and existing evidence

- Camera/timing: `research/CAMERATOOLS_DEEP_ANALYSIS.md`, `research/CAMERA_SYSTEM.md`, `research/TIMESCALE_IMPLEMENTATION.md`.
- Lighting: `research/CUSTOM_LIGHTS_RESEARCH.md`, `research/FORCE_DYNAMIC_SHADOWS_REBORN_ANALYSIS.md`, C24 ABI/guard evidence JSONs.
- Weather: `research/WEATHER_SYSTEM_C19.md`.
- Wind: `research/WIND_SYSTEM_C31.md` — experimental reversible foliage response, native wind/cloth candidates and missing direction/lifecycle proof. Not a verified global wind force control.
- Quality/foliage: `research/CAMERA_QUALITY_C23.md`, `research/FOLIAGE_NEAR_FADE_C16.md`.
- Native particles: `research/PARTICLE_SPAWN_RESEARCH_C25.md`, C26/C27 trace JSONs, `tools/particle_control_flow.py`, `tools/particle_spawn_trace.py`.
- Query tools: `tools/ghidra_query/research_query.py`; the large Ghidra database remains in the parent workspace's `research/ghidra-eldenring/export/research.sqlite`, not this source tree.

Keep game version 2.7.0.0 / patch 1.17 and SHA256 D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134 guards. A data schema/file parser does not prove a runtime ABI or current native offset. Existing original SoulsFormats FXR parsers in the saved sources accept versions 4/5 only; verify an actual resource's header before choosing a decoder.

## Working authorization and boundaries

The user authorizes using these resources for future project research, including appropriate execution or decompilation where needed. Choose source inspection first for open-source projects. Reference-tool execution is not evidence a runtime feature works, nor an instruction to patch the game/save or execute every downloaded file. Preserve original game/reference files; use separate outputs. Keep proprietary reference code/assets out of our distributed source; implement independently from evidenced behavior where applicable.

Record source URL/commit, target version, exact file/function, confidence and runtime evidence for each finding. Label static/compile/user visual observations separately. Never claim full runtime verification from compilation or a single non-null native handle.

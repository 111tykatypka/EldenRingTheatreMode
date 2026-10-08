# Souls Modding reference hub — project research index

Reviewed 2026-10-08.

Source: https://soulsmodding.com/doku.php?id=er-refmat:main

## Priorities

| Reference | Project use | Evidence / limitation |
|---|---|---|
| [Particles](https://soulsmodding.com/doku.php?id=er-refmat:particle-list) | FXR descriptions, source-bank/resource associations | CONFIRMED link points to the same spreadsheet already integrated into C29; not a second independent catalog |
| [FXR Notes](https://soulsmodding.com/doku.php?id=er-refmat:particle-notes) | Investigate emitter properties, particle emission/density and effect structure | CONFIRMED linked sheet documents FXR components; specific property tabs still need inspection before implementation |
| [All in One Sheet](https://soulsmodding.com/doku.php?id=er-refmat:all-in-one-sheet) | Cross-reference VFX, TAE, character and behavior IDs | Hub lists this scope; detailed contents not inspected in this pass |
| [Map Overview](https://soulsmodding.com/doku.php?id=er-refmat:map-overview) | Coordinate/map metadata research | Page accessible; no new coordinate conversion or runtime binding derived here |
| SpEffectVfxParam, SfxBlockResShareParam, AssetModelSfxParam, FootSfxParam | Candidate resource-usage and attachment cross-references | These are hub index entries, not proof of spawning or runtime structure layouts |
| WeatherParam, Gconfig_EffectQuality, Gconfig_LightingQuality, CameraFadeParam | Future weather/rendering investigations | Index references only; no engine memory edits authorized by this index alone |

The SpEffectVfxParam page currently has no content. SfxBlockResShareParam could not be fetched by the web tool. Do not treat these entries as completed research.

## Actual linked sheets

- FXR ID index / introduction: https://docs.google.com/spreadsheets/d/1gmUiSpJtxFFl0g04MWMIIs37W13Yjp-WUxtbyv99JIQ/edit?gid=31255113
- ER FXR data imported into C29: same document, gid 866341224.
- FXR component notes: https://docs.google.com/spreadsheets/d/12hKQg5kBvOJ_M0Udoz5GqS_2RX-d8YtaBapwpSJ2Csg/edit?gid=1424830463

The ID spreadsheet credits community contributors, hosted by Rayan. Its introduction gives an approximate numbering scheme, including seven-digit 7xxxxxx as cutscene-related. Therefore 7191190 is a **candidate cutscene-family effect**, not a verified named effect. Its actual row has no description. Keep that distinction visible and do not replace its unknown description with a guessed name.

The FXR Notes introduction explicitly says it is a work in progress with potentially inconsistent fields. It credits CCCode, Challenger Andy, Rayan, The12thAvenger and others. The visible introductory tab does not itself establish which native property changes emission rate, scale or intensity. Detailed action/property tabs must be checked against decoded FXR content and the exact target runtime.

## Next concrete particle research

1. Inspect the relevant FXR Notes property/action tabs, retaining their source and confidence.
2. Read/decode the selected 7191190 definition and associate its native effect nodes with documented properties.
3. Determine whether requested scale/intensity/density should change instance parameters or an independent cloned definition. Avoid mutating shared game FXR resources merely to edit an authoring emitter.
4. Connect only supported properties to editor controls and report unsupported ones.
5. Continue native owned-handle transform, lifecycle and ReplayTime work; catalog data does not solve those runtime contracts.

C29 catalog import and runtime behavior are unchanged by this documentation update. No new build or game test was performed.

## Additional structured content reference

`research/MISSPIA_ELDEN_RING_DATA_REFERENCE.md` documents the misspia/elden-ring-data snapshot and full file inventory. Useful candidate content labels for equipment/bosses/summons; no FXR catalog or 7191190 mapping was found. Keep SpEffect IDs separate from native FXR IDs.

## Archive / PARAM / map tools (2026-10-08)

See `research/ARCHIVE_PARAM_TOOLING_REFERENCE.md` and `research/format_tools_reference_inventory.json` for the five supplied repository snapshots. The exact Paramdex ER fields support future usage joins; DSMapStudio demonstrates ER regulation reading. The inspected original/vendored FXR parsers only accept versions 4/5, and BinderTool's default source lacks an Elden Ring GameVersion. Decoder compatibility must be checked before extracting target FXRs.

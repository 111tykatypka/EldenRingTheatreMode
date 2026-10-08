# Archive / parameter / map tooling — particle research references

Reviewed 2026-10-08. Local reference snapshots are under `research/references/`; checkout sources were not edited and no project scripts, unpackers or repackers were executed. Game files remain untouched. Commit/file inventory and selected ER schema field lists are recorded in `research/format_tools_reference_inventory.json`.

## Findings from actual code

### Yabber — container unpacking reference

Source: https://github.com/JKAnderson/Yabber
Commit: d98cca857e637272657f4af9dce7c7116ad77b03

CONFIRMED: container unpacker/repacker using SoulsFormats (`Yabber/Program.cs`), with the documented older-game scope and BND/DCX support. README excludes large dvdbnd archive pairs from its supported workflow. GitHub marks this repository archived. It is not a native Elden Ring particle-creation API. GPL-3.0 license present. Current ER archive/compression compatibility must be established on an actual file before using it; do not select it solely because the extension matches.

### JKAnderson/SoulsFormats — binary format reference

Source: https://github.com/JKAnderson/SoulsFormats
Commit: 9c0da6b0721372b34f36b4be503ec020dca1043a

CONFIRMED: .NET binary container/data library. `SoulsFormats/Formats/FXR3.cs` recognizes FXR versions 4 and 5, with enum names DarkSouls3 and Sekiro. It does not recognize version 6 in this checkout. This is a concrete compatibility limitation, not permission to force a newer file through the older parser. Target FXR header still needs inspection. GPL-3.0 license present. No source was linked or copied into Theater runtime.

### BinderTool — archive extraction reference

Source: https://github.com/Atvaark/BinderTool
Commit: eb46414d2ecbe0445479eaccc11b4dd76610b214

CONFIRMED: the cloned default checkout's `BinderTool.Core/GameVersion.cs` contains Common, DarkSouls2, DarkSouls3, Bloodborne. The inspected source has no Elden Ring branch. This differs from the broad repository About description visible on GitHub; source/commit is the controlling evidence for this snapshot. MIT license present. ER support may exist in another branch/fork but is UNKNOWN here. Do not run its old key/archive path against ER based on the page title.

### Paramdex — strongest immediate schema reference

Source: https://github.com/soulsmods/Paramdex
Commit: ff7245e524329bc3eab00036723d2bd53384cedf

CONFIRMED from `ER/Defs`:

- `SpEffectVfx.xml`: `midstSfxId`, `initSfxId`, `finishSfxId`, associated sound IDs and dummy-poly attachment fields.
- `AssetModelSfxParam.xml`: `sfxId_0`, `dmypolyId_0` and subsequent slots.
- `BulletParam.xml`: bullet/hit/flick FXR references and effect-deletion policy fields.
- `SfxBlockResShareParam.xml`: `shareBlockRsMapUidVal` describes another map's resource reference, including a numeric map encoding example.

These are PARAM field schemas; row values must come from the exact target regulation/data. A schema or row-name file is not a resource lookup result, native object offset or spawning function. `ER/Names/SpEffectVfxParam.txt` currently contains only one row label (57000); it is not a comprehensive FXR description catalog. No root LICENSE file found in this snapshot. Retain source provenance and check upstream terms before bundling data/code.

### DSMapStudio — ER regulation/map loading reference

Source: https://github.com/soulsmods/DSMapStudio
Commit: 97bc264cbc3e82a783eb017f57edf651f8ba000b

CONFIRMED: `src/StudioCore/ParamEditor/ParamBank.cs` calls `SFUtil.DecryptERRegulation`; the implementation is available in `src/Andre/SoulsFormats/SoulsFormats/Util/SFUtil.cs`. `AssetLocator.cs` is a useful path/content-resolution reference. These provide an architectural example for an offline reader.

Its vendored `src/Andre/SoulsFormats/SoulsFormats/Formats/FXR3.cs` likewise recognizes versions 4 and 5 only. ER regulation support is therefore not proof of ER FXR-parser support. Root license MIT, with separate dependency licenses. Submodules were not initialized, and it was not built/launched. Long Windows filename checkout errors were resolved by completing the two missing tracked files with per-command `core.longpaths=true`; no global Git configuration was changed. Tracked checkout files now present.

## Recommended use in Theater

Create an offline metadata extraction tool separate from the in-process DLL:

1. Read a copied target regulation/archive or an already-extracted reference resource into a research/output directory.
2. Select a decoder that actually recognizes its container, compression and FXR header versions. Current candidate alternatives already referenced by our project: WitchyBND and EvenTorset/fxr. Compatibility must still be demonstrated on the selected file.
3. Match PARAM types using Paramdex metadata, not XML filenames alone. Join actual row values to `initSfxId`/`midstSfxId`/`finishSfxId`, asset and bullet FXR references.
4. Emit versioned metadata `(FXR ID, usage source, bank, row/table identity, confidence)` for the existing browser, without native pointers.
5. Investigate 7191190's actual FXR nodes and resource references before exposing density/intensity controls. These schemas do not themselves decode its particle-emission settings.

Offline decoding enriches labels and property research. Native creation/stop/update/lifetime remain separate contracts handled by Theater's guarded native backend. Do not use a spell/bullet/SpEffect gameplay action as a substitute for a neutral cinematic emitter.

## Current result

All five references downloaded and inspected at recorded commits. No new runtime feature, complete extraction pipeline, compile test or gameplay verification claimed. C29 remains the current package.

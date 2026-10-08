# C29 — community FXR reference integration

Source supplied by the user:
https://docs.google.com/spreadsheets/d/1gmUiSpJtxFFl0g04MWMIIs37W13Yjp-WUxtbyv99JIQ/edit?gid=866341224

CSV snapshot retrieved 2026-10-08 via the public tab export. Raw file: `research/fxr_reference_sheet.csv`. Generator: `tools/import_fxr_reference.py`. Embedded output: `native_ui/ParticleReferenceData.inc`, with source URL, snapshot date and CSV SHA256 in its header. Regenerate locally by running the script from the repository root; it does not download anything.

## Actual data

11,912 unique positive uint32 IDs, no duplicates. 3,814 rows have behavior or useful-info text; other rows retain unknown semantic descriptions. Columns: ID, BND, RESOURCES, REFS, ORIGIN, COLOR, EFFECT BEHAVIOUR, USEFUL INFO.

ID 7191190 has banks `sfxbnd_m11, sfxbnd_m19` and resources `s00001_3m.tpf, s42071_a.tpf, s42072_n.tpf`. Origin/color/behavior/useful-info fields are empty. This does not identify what its visible effect is. Do not derive a guessed name from a texture filename or bank number.

## Browser behavior

Saved user name -> sheet usage text -> sheet behavior -> Unknown effect. Sheet-derived labels explicitly carry `/ Sheet`; each row retains its FXR ID even when multiple rows share a description. Reference sheet details show bank/resources/origin/color/behavior/usage/references. Names are descriptive metadata; these are not engine-provided native names or proof of 2.7.0.0 behavior.

Search matches numeric ID, user name/category, sheet usage/behavior/origin/color/bank. Described effects only excludes entries with neither a user name nor sheet behavior/usage. Favorites remains available. User annotations are stored separately and never overwritten by the embedded reference snapshot. No network request occurs in the runtime.

The catalog is embedded so DLL placement does not depend on auxiliary CSV paths. The spawn list still intersects with resident resources; it does not try arbitrary unresident IDs. Native preview/create/cleanup calls are unchanged from C27.

## Package and evidence

`outputs/Cinematic-C29-fxr-reference` contains matching Release host/DLL and existing sounds/compatibility probe. Previous packages are preserved. C++ and Rust Release builds passed; `git diff --check` passed. The UI integration has not been tested in Elden Ring. Prior user feedback confirms visible C27 FXR spawning, including 7191190; it does not validate all entries.

To use: close the old host/game, run this package's EXE and select the adjacent DLL in the existing YAFSML workflow. Load the world, F4 -> Particles -> Inspect native VFX -> select emitter. Enable Described effects only, or search a description such as `fogwall`; only loaded matches appear. Select an effect and open Reference sheet details. To see 7191190, turn off Described effects only and search its numeric ID.

The sheet is community reference material attributed to its linked document. No blanket license or version-compatibility guarantee is asserted. For public redistribution, retain attribution and verify the data publisher's permission/license.

# C28 — searchable resident FXR catalog

## User runtime evidence

The user reports C27 creates visible particles, including FXR 7191190. This verifies a visible effect for that reported ID; it does not establish its semantic identity, all IDs, transform conventions or every lifecycle path. No name has been assigned to 7191190 without evidence.

## Why the catalog was difficult

C27 displayed every resource as `FXR <number>`. The numbers are distinct (the native catalog is sorted/deduplicated), but descriptions were absent. These are loaded effect definitions, not a list of active particle instances. The data read from the native resource list gives IDs and pointers, not human-readable descriptions. The browser contains all resident definitions, not just effects useful for cinematics.

C28 changes:

- Search by numeric ID, saved name or category (ImGui text-filter semantics: comma-separated alternatives; `-term` excludes).
- Favorites-only filter and shown/resident counts.
- `Unknown effect [FXR N]` for unnamed IDs; unique UI IDs for every row.
- `My effect name`, `My category`, `Favorite effect`, `Save effect label` for observations.
- User annotations persist as UTF-8 quoted strings in `%LOCALAPPDATA%/EldenRingTheaterMode/particle_catalog.ertfxr` (version 1), written via temporary file and replacement. No native pointers or game resources are stored.
- Keeps native creation/stop mechanism unchanged from C27.

No automatic effect previews on selection, mass spawning or fake descriptions. Favorites filter only currently resident effects; an annotated resource unavailable in the current world does not become spawnable by naming it.

## Research distinction

An FXR definition can describe an entire visual effect; visible particle count is not necessarily the number of create calls. Current preview queues one native creation per click and owns one handle; it never creates an effect every UI frame. If 7191190 produces too many particles, native scale/intensity settings are still unconnected and cannot reduce its density yet. Its FXR structure/resource usage needs inspection before modifying emission parameters.

Primary tooling references:

- https://github.com/EvenTorset/fxr — binary FXR editing; its `name` accessor generates a numeric filename, not a semantic description.
- https://github.com/EvenTorset/fxr-reloader — enumerates resident FXR IDs / fetches resources, useful for future decoded previews and resource inspection.
- https://github.com/ividyon/WitchyBND — archive/resource unpacking, useful to associate numeric FXRs with source banks/content.

Those sources do not provide verified identification of 7191190 here. Source-bank/TAE/parameter references can identify usage, but usage labels must be distinguished from visually verified descriptions. No external catalog was bundled or claimed complete.

## Build and use

Package: `outputs/Cinematic-C28-particle-catalog`. Windows x64 Release host and Rust DLL. UI changes are COMPILE_VERIFIED; user naming/filter persistence remains runtime unverified.

Close the old host and game, launch this package's EXE and load the adjacent DLL with the existing YAFSML workflow. F4 -> Particles -> Inspect native VFX -> select emitter. Type `7191190` into Search to narrow to that ID. Preview it. Give it a name that describes what you see, optionally a category and favorite, then click Save effect label. Clear search to browse, or enable Favorites only for your useful effects. Preview behavior remains the explicit two-second C27 experiment.

The real catalog-naming work is unfinished; annotations are a practical interim browser improvement, not an automatically decoded particle library.

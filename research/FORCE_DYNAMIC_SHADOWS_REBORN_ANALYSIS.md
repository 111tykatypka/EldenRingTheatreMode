# Force Dynamic Shadows Reborn (FDSR) â€” local reference analysis

Date: 2026-10-08  
Local reference: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\eldenringforcedynamicshadows`  
Nexus page: <https://www.nexusmods.com/eldenring/mods/5066?tab=description>

## Scope and safety

The downloaded directory was inspected read-only. It was not copied into the game directory and no original game files were changed. The folder contains data replacements, not an injected DLL, executable, source project, or camera/lighting API.

## Inventory

| Class | Count | Approx. total | Observed role |
|---|---:|---:|---|
| `regulation.bin` | 1 | 2,027,712 bytes | Encrypted/packed regulation data replacement; no plain-text header was visible |
| `*.parambnd.dcx` | 2 (one `.prev`) | included in 135,277,408 DCX bytes | System parameter bundle and identical local backup |
| `*.ffxbnd.dcx` | 97 | included in 135,277,408 DCX bytes | Effect/SFX bundles, split by character (`c####`), map (`m##`) and common effects |
| `*.bak` | 8 | 9,756,624 bytes | Backups of selected mimic effect bundles |
| Total files | 107 | 147,068,560 bytes | No EXE, DLL, PDB, source, script, JSON, XML or config was present |

The DCX files have the expected FromSoftware compression/container signatures (`DCX`, `LDCS`, `DCP`, and either `DFLT` or `KRAK` in the wrapper). `regulation.bin` begins with high-entropy bytes rather than an unpacked regulation header, so it should be treated as an encrypted/packed game data artifact until processed with a compatible regulation tool.

Selected hashes:

```text
regulation.bin                                      A8B2A2F0B1426086B28BC8B3CB9B175C8AB0C320917CC6ADE93F32FFE3379CE5
param/systemparam/systemparam.parambnd.dcx         72C603DA3DA7DD73C99A838039FE9776F231B26BEAC073A509A62D3A5DA5EC54
param/systemparam/systemparam.parambnd.dcx.prev    72C603DA3DA7DD73C99A838039FE9776F231B26BEAC073A509A62D3A5DA5EC54
```

The `.prev` system parameter file is byte-identical to the active file in this download. It is not a second version to merge.

## What the public description confirms

The author describes FDSR as forcing dynamic shadow casting from light sources. Its stated changes include real-time dynamic shadows for torches, lanterns and other static light sources, moving lantern light in front of the lantern, and making torches work regardless of their raised/lowered state. The installation is `regulation.bin` plus the `sfx` directory through Mod Engine 2. The page identifies version 0.9, last updated 2024-10-22, and specifically mentions an update for Elden Ring 1.16. This is not evidence of compatibility with our strict 1.17/2.7.0.0 profile.

Source: the Nexus description and permissions section linked above.

## What can be inferred from the files

### Confirmed

- The mod is a replacement-data mod. There is no native code to decompile and no runtime IPC or hook implementation to reuse.
- `sfxbnd_c####` files target character/effect packages; `sfxbnd_m##` and the long `m60_*`/`m61_*` names target map/effect packages; the two `commoneffects` files are global effect packages.
- The mod distributes many bundles instead of only one global table, which is consistent with changing effect definitions across many maps/character effect sets.
- The package is intended to be loaded by Mod Engine 2 rather than by TheaterMode.dll.

### High confidence

- The dynamic-shadow behavior is likely achieved by edited SFX/effect records and system parameters that turn on or alter shadow-casting flags, light placement, or effect attachment parameters. The filenames alone do not identify the individual record IDs.
- The mod does not provide a general-purpose custom point/spot light factory. Its lights are existing game effect/light resources attached to torches, lanterns and other static sources.

### Unknown until the bundles are unpacked and compared

- Which regulation rows and SFX entries were changed.
- The exact effect IDs, shadow flags, radius/attenuation values, light transforms, or quality settings.
- Whether any changes affect the global shadow-distance/LOD system rather than only individual light effects.
- Whether the 1.16 package can load safely on 1.17. Do not infer this from the page description.

## Relevance to Theater Mode

This reference is useful for the planned lighting and high-quality LOD work, but at a different layer from our native backend:

1. **Dynamic shadow behavior:** use FDSR as a behavioral reference for the desired result (static torch/lantern sources casting shadows on nearby geometry).
2. **Effect data research:** unpack a copy with a compatible regulation/SFX tool and diff it against the exact 1.17 base files. Record only field-level findings and IDs in a research note.
3. **Runtime architecture:** keep Theater custom lights as native, owned point/spot objects created by `NativeLightBackend`; do not replace that API with wholesale data-file installation.
4. **LOD investigation:** FDSR may reveal data parameters related to shadow distance or light-effect quality. It cannot prove that an NPC animation/character-update LOD is controlled by the same parameters.
5. **Replay safety:** data-file changes are external to ERPLAY and must not be silently baked into replay metadata. A replay should record the active game/mod profile if reproducibility depends on these effects.

## Recommended next research step

Make a separate unpacked working copy, never the game installation or this original download. Use a version-aware regulation/SFX parser (for example Smithbox/SoulsFormats tooling) to:

- decompress the DCX wrappers;
- enumerate BND entries and effect IDs;
- compare FDSR files with the exact 1.17 originals;
- extract changed fields and map them to shadow/light semantics;
- validate that no 1.16-only row layout is being applied to 2.7.0.0.

Only after a field-level diff is available should we consider adding optional Theater settings or a profile-level compatibility check. Do not copy `regulation.bin` or these bundles into the game as part of Theater Mode, and do not merge their binary assets into the source handoff package.

## Bottom line

FDSR confirms that Elden Ring's existing effect/light data can produce dynamic shadows for static light sources, but it does not expose the native mechanisms needed for Theater's editable, camera-positioned custom lights. It is a valuable data-diff reference for shadow flags, effect attachment and quality parameters. The next useful action is controlled unpack-and-diff against 1.17, not decompilation or direct reuse of the downloaded binaries.


## C33 — native custom-light shadow controls (2026-10-08)

**STATIC_VERIFIED:** exact-target point packet builder `0x141BEFB00` and spot packet builder `0x141BF0D60` consume the same base-light properties:

| Offset | Type | Meaning supported by native debug UI / render consumer |
|---|---|---|
| +0xAD | byte | Shadow request |
| +0x94 | float | Shadow intensity; must exceed zero |
| +0xB8 | uint32 | Required renderer shadow quality threshold |
| +0xB4 | int32 | Depth bias, native UI range -7 through 7 |

The point condition at VA `0x141BEFE48` and spot condition at VA `0x141BF1756` require all of: request enabled, positive intensity, per-view shadow permission (settings byte +1), and renderer quality (settings uint +4) at least the light threshold. Disassembly corroborates decompiled conditions. The feature compares exact instruction bytes at both RVAs before permitting shadow writes. GameProfile owns the new offsets and guard bytes. These functions are inspected, not called by Theater.

Evidence: `light_shadow_packet_c33.json`, `light_shadow_guards_c33.json`, `light_shadow_spot_packet_c33.json`, `light_shadow_spot_guard_c33.json`; previous native debug property editor `0x141AED460` research. Base image SHA256 remains D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134.

**COMPILE_VERIFIED:** C33 writes those properties only on Theater-owned native point/spot lights through the existing Draw_Pre backend. Global experimental gate defaults off, separate from saved per-light intent. Disabling the gate clears +0xAD on the next successful backend update (approximately 30 Hz, lock/context permitting). Existing removal and release paths remain unchanged. No game, regulation, SFX or mod files are altered.

**UNKNOWN:** shadow map allocation/budget, participating caster geometry, light-count limits imposed by renderer, actual FPS cost and visible shadows. The per-view gate can deny requests. The quality control is not a resolution setter. Native render-packet eligibility does not prove the complete shadow pass works for freshly created lights. Runtime and visual validation remain required.

**Not implemented:** forcing all existing torch/lantern/world lights to cast shadows like FDSR. The mod's regulation and effect bundles have not been unpacked/diffed against 1.17. No compatibility claim is made for its 1.16 replacement data.

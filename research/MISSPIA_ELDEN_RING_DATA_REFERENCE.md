# misspia/elden-ring-data — reference assessment

Reviewed 2026-10-08. Source: https://github.com/misspia/elden-ring-data
Local snapshot: `research/references/misspia-elden-ring-data`.
Commit: `d77f19e1d16de8b3af595fb78ddcb6347086e5bf` (2025-03-12).
Inventory: `research/misspia_elden_ring_data_inventory.json`.

## Confirmed contents

90 non-Git files, including 24 JSON datasets plus CSV/HTML inputs and a TypeScript conversion pipeline. `src/Cleanser.ts` parses source tables and writes formatted JSON. This is content/reference data tooling, not an injected game SDK or memory adapter. No repository scripts were run and no dependencies installed. The reference checkout is retained unchanged.

Selected counts: bosses 158; player bosses 20; spirit ashes 64; magic 171; weapon-stat rows 2,717; armor rows 578. These counts are records in this snapshot, not a claim of complete or current game coverage. Weapons include affinities and variants; row count must not be described as the number of distinct weapon models.

Search across source, JSON and CSV found no `fxr`, `7191190`, `sfxbnd`, `residentSfx` or `sfxId` matches. Source schemas do contain SpEffect fields. SpEffect IDs are a different domain from FXR IDs; never import them into the FXR browser by numeric similarity.

## Relevant uses

- Named equipment/magic metadata could annotate existing recorded IDs once the exact table-ID domain is matched.
- Boss records include encounter names and locations; candidate reference for actor labels, not enough to map a live ChrIns to an encounter.
- Spirit-ash metadata may help summons labeling after identity/lifecycle access is established.
- README links lead to additional modding/schema tools; links are discovery pointers, not verified current integrations.

## Constraints

No verified link between this dataset and FXR definitions; no new particle labels are imported. The formatted JSON can omit columns present in raw source schemas, so downstream joins must inspect actual output keys. A name or human-readable location is not a native warp ID or BlockPosition conversion.

`package.json` declares ISC; no standalone LICENSE file was found. Do not assume the declaration establishes rights to every upstream spreadsheet or game-derived content item. Preserve provenance if any data is bundled later.

No explicit 2.7.0.0 / patch 1.17 compatibility guarantee was found. The package version `1.0.0` is the tooling version, not the executable version. Validate ID joins using current game data before use. No game changes, runtime writes or new build were needed for this reference inspection.

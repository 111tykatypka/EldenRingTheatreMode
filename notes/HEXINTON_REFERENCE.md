# Hexinton v8.0.4 reference — static findings

Date: 2026-10-07. No Cheat Engine scripts were executed; no game memory was read or written.

## Provenance and compatibility

- Source: `C:\Users\user\Desktop\eldenring_all-in-one_Hexinton-v8.0.4.CT` (original unchanged).
- Bytes: 7,931,419. SHA-256: `b083aafa642051003da8ed4aa7495a0619fc1680651b093d7f9e7b7ac0829e2d`.
- [CONFIRMED] XML `CheatEngineTableVersion="52"`; 12,901 entries; 342 dropdown lists.
- [CONFIRMED] The enable script sets `tablever=0x2000700010000` and describes **patch 1.17.1 / file version 2.7.1.0**. Its comment reports an update on 2026-09-09. That comment is author-provided evidence, not our live verification.
- Theater Mode remains guarded for **patch 1.17 / 2.7.0.0**, SHA-256 `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`.
- [UNKNOWN] Compatibility of this table's addresses, scripts and layouts with our target. There are stale absolute addresses and older scripts alongside updates. Never treat the whole table as a coherent SDK.

## Useful lookup locations

Paths below are entry hierarchy paths in the CT, not filesystem paths. Index IDs are generated traversal IDs; use `entries.ce_id` for original CE IDs.

| Information | CT location | Intended use |
|---|---|---|
| Named grace IDs | `[ Enable ]/[ Fast Travel and Warp ]/Choose Grace` | Location labels and warp destination candidates |
| Warp calling convention / signature | `[ Enable ]/[ Fast Travel and Warp ]` | Cross-check existing `arrival.rs` / `GameProfile.h` integration |
| Current map | `[ Player Status ]/FieldArea:MapID` | Research map metadata |
| Last/target grace | `[ Player Status ]/LastGrace`, `TargetGrace` | Arrival diagnostics |
| Character level and attributes | `[ Character Data ]/Attributes` | Optional capture metadata; not an instruction to change stats |
| Parameter classes and field names | `[ Param Patcher ]/[ ParamPatcher ]`, `[Alt Param Patcher]/BaseParamClass` | Cross-check pinned generated param types |
| Mount / summon parameters | `Buddy Param`, `Advanced Mode/RideParam`, `GameSystemCommonParam` | Phase 2.3 entity and relationship research |
| Ride state | `[ Character Data ]/ChrRideModule`; `[ NPC ]/Character Spawner/[Character Spawner]/Debug Chr Data/[Physics/Position]/[Ride] 29` | Read-only lifecycle probes before any mount control |
| World progression flags | `[ World / Npc Flags ]` | Named flag semantics; keep replay writes optional and disabled by default |
| Character categories | `[ Player Status ]/ChrType`, `TeamType` | Candidate labels, not sufficient proof of summon ownership |

## Concrete evidence and limitations

### Warp

[CONFIRMED] The table defines a wildcard Lua warp pattern, resolves its callable entry at the scan result plus two, loads the event manager's imitation at `+0x18` and proxy at `+0x08`, and passes the selected grace ID minus `0x3E8` (1000).

[CONFIRMED] This matches the conceptual call and constants already documented in our `shared/GameProfile.h` (`TM_AOB_LUA_WARP`, `TM_OFF_LUA_EVENT_MAN_*`, `TM_VAL_GRACE_ID_BIAS`). This is static corroboration only; it does not prove a unique match or safe invocation on 2.7.0.0.

Small examples from the named lookup: The First Step `1042362951`; Academy Gate Town `1037442950`; Table of Lost Grace `11102950`; Haligtree Roots `15002954`. These are the table's **grace IDs**, not generic map IDs, player handles or param IDs. Verify against the game's actual grace data before using them.

### Mounts and summons

[CONFIRMED] Ride entries identify module access through `ChrIns +0x190`, module table `+0xE8`, a node at `+0x10`, ride state at node `+0x50`, ridden-character marker at module `+0x33` and mounted marker at `+0x163`.

[HIGH CONFIDENCE] The state `+0x50`, marker `+0x33` and mounted field `+0x163` agree conceptually with the pinned SDK's `CSRideNode.ride_state`, `CSChrRideModule.is_ride_character` and `is_mounted`. Actual offsets/layout must still be checked for this executable; no new raw offsets have been added to runtime code.

[CONFIRMED] Table labels are sometimes ambiguous or wrong: its `RideParamID` entry points to module `+0x20`, whereas the pinned SDK places `ride_param_id` in `CSRideNode`, and its `Horse_X` points to module `+0x70`, whereas the SDK documents `mount_data` beginning after module `+0x40`. Do not copy these chains directly.

[CONFIRMED] The pinned SDK warns that `last_mounted` can be null after loading an already mounted save, and that mount data exists on the rider, not the mount. A cached `last_mounted` pointer is not a reliable current rider/mount relationship.

[UNKNOWN] Summon ownership, lifetime and safe mount/dismount APIs on 2.7.0.0. `ChrType`/`TeamType` alone cannot identify Torrent or spirit ashes reliably. Existing actor replay cannot recreate absent or despawned bodies.

### Parameter / ID handling

[CONFIRMED] Some param roots are literal `7FF4...` addresses, and older scripts contain literal player offsets. Those are process/version-dependent research clues, never valid runtime constants by default.

[CONFIRMED] Dropdown names, field names and scripts are indexed. Param rows, item IDs, event flags, grace IDs and actor handles must stay in distinct namespaces. `BuddyParam.npcParamId_ridden` is a useful bridge to investigate, not proof of a currently active actor.

## Reusable query tool

`scripts/index_cheat_table.py` uses Python's XML parser and SQLite FTS5. It is static only: no Lua evaluation, assembler execution, injection or process attachment.

```powershell
python scripts/index_cheat_table.py index 'C:\Users\user\Desktop\eldenring_all-in-one_Hexinton-v8.0.4.CT' '.research/hexinton-v8.0.4.sqlite'
python scripts/index_cheat_table.py query '.research/hexinton-v8.0.4.sqlite' 'RideParam OR BuddyParam' --limit 15
python scripts/index_cheat_table.py query '.research/hexinton-v8.0.4.sqlite' '"Fast Travel"' --full
```

Indexing refuses to overwrite an existing database. Queries open the database read-only. Schema:

- `metadata(key,value)`: source path/hash/size/table version and embedded inventory.
- `entries(id,ce_id,parent,path,description,variable_type,address,offsets)`: searchable hierarchy; offsets retained in XML order. CE displays offset chains differently from a left-to-right C pointer expression.
- `texts(id,entry_id,kind,text)`: descriptions, assembler/Lua text, dropdowns, addresses and top-level comments.
- `search(path,kind,text)`: FTS5 copy of text for fast search.

There are 24,487 indexed text blocks. Ten encoded forms and one encoded embedded file are inventoried by name and length, not decoded or executed. Other XML attributes such as bit ranges are not currently indexed; inspect the original XML when those matter. Local generated databases contain third-party research text and are ignored by Git; original CT is not bundled in distributables.

## Next use

1. Use named lookups to improve diagnostics and metadata in the duplicate.
2. Cross-check selected param definitions with the exact pinned SDK and Ghidra/disassembly.
3. For Phase 2.3, instrument current ride-node state and currently enumerated actor identities read-only; verify live transitions before attempting mount ownership writes or spawning.
4. Keep the existing game version/hash guard unchanged.

# Debug tools reference — 2026-10-08

Local research is preserved outside the build tree:
`../../research/debug-tools-reference/README.md` (relative to this notes folder).

Inputs: workspace `debug tools/Elden.Ring.Debug.Tool.0.8.6.2` and
`debug tools/er-save-manager_2.0.1_Windows`. Originals were only read and their
1,662 hashes rechecked unchanged. Neither tool was launched.

Research includes a complete inventory, exact-release public sources,
three decompiled managed modules, queryable SQLite catalogs, exact-target
signature scan and short disassembly evidence.

Catalogs: 451 locations, 419 graces, 1,384 events, 942 quest steps, 3,887
base/DLC/mod item rows, 7,226 param fields and 194 older param-pointer entries.
All 419 grace unlock flags correlate with the second catalog.

Seven selected reference signatures match uniquely in SHA-256-verified
Elden Ring 1.17 / 2.7.0.0. This is STATIC_VERIFIED evidence only. Object
lifetimes, current param offsets and weather-write safety remain unverified.

Important cautions:

- Save coordinates/map layout are not automatically live Havok coordinates.
  Preserve the replay coordinate-origin guard; old missing metadata is not fixed.
- Item IDs require category and game/mod namespaces; grace EntityID, LastGrace
  (+1000 in the reference), map IDs and event flags are different identifiers.
- Save-manager RideGameData describes persistent Torrent state, not skeleton
  transforms or live mount reconstruction.
- The save-manager license restricts reuse/redistribution; Debug Tool is GPL-3.0.
  Do not package reference code, catalogs or icon assets into Theater builds.

Current C18 EXE/DLL and runtime integration were unchanged in this research.

# P2d validation — 2026-10-07

## Automated / static evidence

- Release x64 C++ host + native overlay build: PASS.
- Host/native CTest: **12/12 PASS**. Updated editor test proves one authoritative
  Play/Pause toggle, preserved timestamp, loaded vs requested ownership, paused hold,
  Stop disarm and explicit seek request. Pipe test uses a temporary local fixture,
  not Elden Ring; it does not prove engine application.
- Rust Release tests: **43 PASS, 0 FAIL, 1 ignored** (optional real-file inspection).
  New tests cover every stored float bit, overflow/truncation, actual hierarchy/cycles,
  skeleton identity, master-clock pause/seek/speed, invalid write data, malformed
  chunk sizes/counts/events, worker finalization validation, a 250-bone file/identity
  round-trip, empty-track refusal, and an 11-minute synthetic timestamp
  recording without the old cutoff.
- Legacy fixture inspection: separately run against the real `Torrent test.erplay.world`;
  685 samples decode. Physics root changes; chunk anchor is constant. No fixture was
  modified. This proves reader compatibility/data evidence, not a new game test.
- Synthetic storage fixture: 200 player + 90 actor frames; lossless local/model
  round-trip verified. Storage/performance depends on actual motion; no boss-fight
  data-rate or recording-overhead claim is made.

## Not performed

New DLL game launch, new v2 live capture, rendered hierarchy retention, Stop/ownership
behavior in Elden Ring, cross-DPI visual acceptance, mounted Torrent pose, sustained
real recording, FPS/CPU/latency measurements, file-system crash recovery and optional
ReShade coexistence were NOT tested in the game during this checkpoint.

No new feature is labeled RUNTIME_VERIFIED or VISUALLY_VERIFIED.
See `P2D_RUNTIME_TEST.md` for the exact next test and `P2D_ROADMAP_STATUS.md` for every
unfinished roadmap category. Earlier inherited reports do not supersede these limits.

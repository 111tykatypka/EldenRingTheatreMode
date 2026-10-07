# Phase 2.3 — first companion replay checkpoint (P2c)

Date: 2026-10-07. Branch: `codex/p2b-continuation`.

**IMPLEMENTED — RUNTIME VALIDATION REQUIRED. Phase 2.3 is not accepted as complete.**

Original Claude source and `TheaterMode-Current` P2b are preserved. Experimental output lives only in this duplicate's `outputs/P2c-mounts-summons`.

## Feasibility and evidence

| Category | Achievable in this checkpoint | Evidence / limits |
|---|---|---|
| Torrent as a separate recorded entity | Root + full skeleton pose on existing body; interpolated on master replay time | [CONFIRMED static] pinned SDK documents `WorldChrMan.summon_buddy_chr_set` as containing Torrent and spirit ashes; live layout/render result unverified |
| Spirit ashes | Separate actor IDs and pose/HP tracks for enumerated active bodies | [HIGH CONFIDENCE] same actor-pose pipeline; set membership is recorded, not guessed from a model name |
| Offline NPC co-op summons | Separate pose tracks; explicit `WhiteSummonNpc` / `WhitePhantom` annotations | [CONFIRMED static] SDK types; presence and semantics need game testing |
| Mounted/dismounted state | Read-only observations at approximately 4 Hz, current pair-node handle mapped to recording ID | Approximate transition timing; no forced mount/dismount calls |
| Dismissed/dead/missing companions | Reported as unmatched; not recreated | Spawning and lifecycle restoration are not implemented |
| Full mount/dismount/summon lifecycle replay | Not yet possible with this implementation | Requires lifecycle proof and a separate approved ownership/spawn design |

The Hexinton table targets **2.7.1.0**, not this game's **2.7.0.0**. No table script or address was adopted blindly. New fields use the exact locked SDK's public structures; numeric profile constants remain centralized in `shared/GameProfile.h`.

## Changes

- `companions.rs`: read-only active buddy-set discovery and scalar ride observations. ReadProcessMemory copies do not materialize invalid Rust bools/enums. Entry load status, physics owner, ride-module owner and pair-node owner are checked. Invalid fields are unavailable, not manufactured values.
- Distance-list and buddy-set bodies are combined and deduplicated. Companions inside the existing 100 m recording radius are full-rate even beyond the 30 m near zone.
- Buddy-set membership, ridden-body evidence and summon-type annotations are stored in an optional versioned world track. They remain separate concepts; membership alone does not prove a specific summon type or owner.
- Current `CSPairAnimNode.counter_party` handle resolves the player/mount relationship to a pointer-free recording ID. `last_mounted` is not used or cached.
- Companion bodies use the existing actor root/bone interpolation and shared timeline, not inputs or animation commands. No global game-time writes added.
- Mount-state mismatch stops body application and gives an explicit error. It never silently invokes a guessed mount/dismount API. Stop, match your mount state and Play again; recordings crossing transitions are not fully reconstructible yet.
- Catalog, poses and companion metadata enter the bounded writer queue atomically. Announcements are retried after queue pressure; dropped actor samples are counted and reported.
- Matching reserves each live body immediately, preventing two tracks from selecting it in one pass. Different handles are accepted only for a unique recorded/live semantic companion candidate with matching NpcParam. Groups of identical ashes do not receive guessed ordinal matching.
- Actors outside their recorded span or after a >500 ms observation gap are released instead of held indefinitely. Those gaps are not mislabeled as proven death/spawn events.
- Missing control guards or mismatched poses prevent root-only sliding. Restoration changes only owned no-move/no-attack bits rather than clobbering unrelated debug flags.
- MB/min is calculated from source timestamps, rather than assuming every recording is exactly 60 Hz.
- New build identifies itself in the game log as `BUILD=P2c-companions-independent`.

## File compatibility

ERPLAY is unchanged. ERWORLD1/version 1 adds optional track 5/kind 7; old readers skip it. Old world files without the track remain readable. Detailed field layout: `docs/WORLD_COMPANIONS_FORMAT.md`. The new reader checks companion references, duplicate actor IDs, context ordering and incomplete trailing chunks.

## Resource and runtime limits

- The 4096-entry buddy-set scan bound is a corrupt-layout/resource guard, not an actor recording limit. It is adjustable in GameProfile; an exceeded guard emits a diagnostic.
- New scalar reads and set discovery have not been benchmarked in-game. Existing actor sampling still allocates pose buffers and caches compressed file bytes in memory during playback. No performance number is claimed.
- Existing-body control restores flags/gravity/root, but does not reconstruct mount ownership, AI internals, HP/death, equipment of summons or missing actors.
- Actor poses are written at the existing PrePhysicsSafe task; mounted pairing/physics may overwrite them. This is a specific live-test question, not a solved result.
- Spawn/despawn visibility and backward restoration of dismissed summons are not implemented. Weather and the other P2b limitations remain.
- Startup/launcher/replay storage use existing application data and IPC names. Run one host only. The original sources/build files are isolated; runtime user settings/replay storage are not a second namespace.

## Acceptance boundary

Build/unit tests are not game acceptance. Follow `PHASE2_3_RUNTIME_TEST.md`. Stop at this checkpoint; do not proceed to props, effects or cameras until user results are reviewed.

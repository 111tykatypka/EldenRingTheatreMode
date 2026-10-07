# Conversation and current-code review

## Scope

Reviewed the supplied conversation and the local Claude project transcript
`C:\Users\user\.claude\projects\C--Users-user-Documents\451d3772-e00a-41d9-8fcc-ae112abda3b9.jsonl`.
Visible dialogue was read; tool calls were indexed and the latest status replies and selected implementation evidence inspected. This is not a claim to have audited every raw tool result, every attachment, hidden reasoning or other Claude conversations.

## What the development history establishes

1. The exact 2.7.0.0 adapter, recurring game task, local player access and transform sampling were user-verified earlier. Preserve this path and its version guard.
2. Transform-only replay visibly slid the character without animation. Reproducing actions through input/animation commands did not solve reliable scene reconstruction.
3. Native bloodstain playback briefly worked, but sparse payloads and lifecycle limitations made it unsuitable as the primary backend. The user later chose replaying their own character's recorded skeletal pose.
4. Local and model-space bone recordings provided the major breakthrough. The user confirmed successful own-player playback and timeline synchronization. The relevant pose data is hkQsTransform; interpolation reconstructs model poses from local transforms with an inferred hierarchy.
5. A SDK/debug-flags mismatch caused crashes: the older field location overlapped a callback pointer. The current profile uses a corrected location with a structural callback guard. This is a concrete reason to cross-check new SDK/CT fields rather than blindly trust layouts.
6. There was also confusion from launching older DLLs. Build paths and manifests must identify the exact experimental DLL, and only one host should run.
7. Global time-scaling experiments caused a reported slowdown; its exact causal mechanism was not proven. The user confirmed recovery. Current P2b removes that experiment, and the latest phase specification explicitly requires replay speed to use its own clock at normal game FPS.
8. The earlier independent Codex changes were merged into Claude's source. Therefore the older independent checkout is no longer the best starting point. The new duplicate starts from the latest P2b commit.
9. P2b adds equipment, arrival/warp, world clock/optional flags, nearby actor pose replay and a compact chunked world file. The user's quoted status explicitly says those additions have **not been tested in game**. Compile/unit success does not change that.

## Current code and gaps

- Host: C++ native UI, replay reader/player/timeline and playback clock; Rust game adapter with pinned fromsoftware-rs; shared game profile and IPC structures.
- `adapter/src/bone_replay.rs`: own-player pose ownership, world-file loading, equipment/world-state restoration and actor playback integration.
- `adapter/src/actors.rs`: nearby enumeration, identity matching and root/bone application to existing live actors. No missing-body spawning or complete actor lifetime reconstruction.
- `adapter/src/world_file.rs`: separate versioned/chunked world tracks, background disk writer. This does not yet imply bounded-memory loading for arbitrarily long playback.
- `shared/GameProfile.h`: exact-build guard and research-backed constants. Keep runtime changes centralized and supported by evidence.
- Actor IDs, mount relationships and summon ownership need lifecycle proof. A remembered pointer or a ChrType label is insufficient.
- Weather, absent/dead boss recreation, later actor spawning and the later camera/editor phases remain incomplete.
- P2b actor recording currently discards some `try_send` errors; identity announcements can be lost under queue pressure. Actor matching's `taken` list is not updated during the same matching pass. These are source-review findings, not observed live failures, and should be covered before expanding entity replay.

## Direction

Continue incrementally in the new duplicate. Next phase is mounts/summons, starting with read-only identity and ride-state verification. Use the latest phase prompt as the development order, preserve existing playback and normal-frame-rate timing, and require explicit user runtime results for each experimental release.

The new Hexinton table is a useful lookup and static corroboration source, but it targets 2.7.1.0. Its details and query tool are documented separately in `HEXINTON_REFERENCE.md`.

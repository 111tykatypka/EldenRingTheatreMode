# P2e actor observation extension

Status: COMPILE_VERIFIED. Native death/corpse reconstruction remains UNKNOWN.

This additive extension keeps ERWORLD version 2 and existing pose codecs. Track 7,
kind 9 is observation schema 1. Old readers skip this unknown track; old files have
no observations, so their reader retains legacy existing-body behavior. A breaking
observation schema must use a new kind or explicitly versioned payload.

Chunks retain the existing 44-byte header, packed-payload CRC32 and zero-run codec.
An observation is exactly 56 bytes, little endian, with no padding or pointers:

| Offset | Type | Field |
| --- | --- | --- |
| 0 | u64 | source-clock timestamp_ns |
| 8 | u32 | ReplayActorId, nonzero |
| 12 | u32 | character_id |
| 16 | i32 | npc_id |
| 20 | u32 | model_id (SDK character_id) |
| 24 | u32 | known mask: body=1, flags=2, HP=4, pose=8 |
| 28 | u32 | observed flags: death=1, render enabled=2 |
| 32 | u32 | raw SDK backread_state |
| 36 | u32 | raw SDK chr_set_cleanup |
| 40 | i32 | HP, observation only |
| 44 | i32 | max HP, observation only |
| 48 | u32 | availability: 0 observed in range, 1 not observed |
| 52 | u32 | reason: 0 normal, 1 enumeration/radius gap, 2 pose unavailable, 3 topology change |

Backread/cleanup numbers are preserved without invented enum meanings. Missing
observations have known=0; default zero values must not be interpreted as live state.

The recorder emits state changes plus one-second refresh snapshots, transactionally
with actor catalog/pose/companion data. Failed queue submissions do not advance the
committed observation state. Each actor's timestamps strictly increase; reference IDs
must occur in the actor catalog. Finalization and reader validate these constraints.

All times share the existing game source clock. Playback uses the existing host
ReplayTime mapped to source time. No actor clock is integrated. Actor-local binary
search selects the latest observation at/before T, independently of previous seeks.
Pose interpolation still uses existing timestamp/continuity checks; snapshots cannot
authorize holding a pose beyond a pose-track gap or last frame.

Meaning of evaluation:

- ObservedAlive: SDK death flag was clear. HP=0 alone is not death.
- DeathFlagged: SDK death flag was set. This does not prove death-animation completion.
- Unknown: not observed or no valid body/flags. This does not prove engine despawn.
- Dying, corpse/ragdoll, genuine spawn/despawn, boss phase, damage source, reward and
  model replacement events are NOT IMPLEMENTED native semantics.

The existing-body playback backend refuses pose application on death/render mismatch
or missing pose availability and logs ACTOR_RECONSTRUCTION_REQUIRED. It does not clear
death flags, change HP, spawn a puppet, hide a live body or resurrect an enemy.

## Read-only inspection

```powershell
python tools/inspect_actor_lifetimes.py 'path\fight.erplay.world' --at 13 --at 5 --at 13 --output actor-lifetimes.json
```

Offsets are seconds since the first player chunk timestamp. The inspector validates
chunk envelopes/CRC and observation records, not skeletal payloads or native behavior.
Observation times are absolute source-clock nanoseconds in the output.

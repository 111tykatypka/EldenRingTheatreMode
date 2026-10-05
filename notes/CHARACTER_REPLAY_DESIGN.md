# Character tracks — Modern schema 1

IMPLEMENTED / UNIT TESTED; native capture requires runtime validation.

ERPLAY02 and ERPLAY03 readers remain. A new overall version is unnecessary for
optional observations: ERPLAY03 already has CRC-protected typed chunks. Type 3 is
the new character track; old v3 readers skip this optional type after CRC checking.
The existing player CHNK, action type 2, header and footer layouts are unchanged.

Character records are **112 bytes**, explicitly encoded little-endian; never use
`sizeof(CharacterRecord)` as a disk/wire layout (C++ alignment differs):

| Offset | Type | Field |
|---:|---|---|
| 0 | u16 | schema = 1 |
| 2 | u16 | kind: registry=1, transform=2, presence=3 |
| 4 | u32 | flags: presence 0=unobserved, 1=observed |
| 8 | u64 | session observation identity, not a pointer |
| 16 | u64 | active replay timestamp ns |
| 24 | u64 | native FieldInsHandle, registry only |
| 32 | u32 | event entity ID, registry only |
| 36 | i32 | NPC param ID, registry only |
| 40 | i32 | BlockId, registry only |
| 44 | u32 | raw ChrType, registry only |
| 48 | f32[3] | physics position |
| 60 | f32[4] | orientation XYZW |
| 76 | 32 bytes | existing ActionState; NPC semantics Unknown |
| 108 | u32 | reserved = 0 |

Registry appears once per identity, before transforms/presence. Transform records
zero registry fields. Validation rejects duplicate registry, missing registration,
regressing actor time, bad schema, flags, non-finite transforms, invalid quaternion
or action values, bad CRC/length, and records beyond player duration.

Recorder drains character snapshots no later than the current player timestamp.
It uses the SAME RecordingSession source origin and paused duration. Pre-start and
pre-resume observations are discarded. Character chunks flush alongside player
chunks; a 4096-record pending threshold bounds writer memory independently of
recording length. Reader keeps compact per-actor time/offset indexes and reads
records lazily. UI caches at most 3000 preview entries per actor. These previews
are decimated, not an exact rendered world simulation.

Recovery preserves checksum-valid optional chunks. Character count is derived from
records, not stored in the v3 footer. Removing a whole optional track cannot be
detected by a footer count; this is a documented limitation of optional v3 tracks.
An authenticated full-scene format would need a stronger manifest/version.
Individual allocation is limited to 64 MiB per chunk to reject hostile lengths;
the writer emits much smaller chunks. There is no total file/duration limit.

## Character IPC

The existing player and replay-control pipes are unchanged. New local pipe:
`\\.\pipe\EldenRingTheaterMode_1_17_Characters`.

Host is inbound server; DLL worker is writer. Header is 40 bytes:
magic CHRT/u32, version/u16=1, flags/u16, count/u32, reserved/u32=0,
sequence/u64, source timestamp/u64, callback mailbox drops/u64. Then count records
in the 112-byte layout. Snapshot flags: 2=incomplete/resource budget; 4=world/player
unavailable. Native metadata accompanies observations in IPC; host stores registry
metadata only once. PID must match the existing player pipe peer. Maximum 16384
rows, schema/sequence/time/order/numeric checks; malformed stream is disconnected.

DLL publishes a latest snapshot under try_lock. It never blocks the game callback
on IPC/file I/O. Host has a 16-frame queue; overflow drops oldest with a counter.
Missing frames are gaps, never fabricated interpolation evidence. An incomplete
snapshot does not create false disappearance events.

Identity combines native handle plus current address for internal comparison only.
No address is transmitted/stored. If absent from a discovery pass, a later
observation gets a new ID. An incomplete budget-limited pass can also split lifetimes;
no absence event is emitted from that pass. This conservatively splits range re-entry; it is NOT a
proof of native spawn generation. Spawn, death, respawn, AI actions are unavailable.

## Future evaluation

Player and character transform tracks share replay time. Camera/editor input and
future camera tracks remain separate. Future track kinds: actions, objects,
physics, destruction, VFX, environment/weather, vegetation, camera, audio, markers.
These have no fabricated data or active controls in this build.

Before NPC native playback: prove capture across several ordinary actors, validate
identity through range exit/loading, then test scoped ownership of ONE existing
actor. Do not spawn duplicates, disable global AI, or replay entire populations.

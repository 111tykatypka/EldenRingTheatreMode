# ERWORLD1 companion extension

Container version remains 1. Player/actor pose and ERPLAY layouts are unchanged.
Optional chunk: **track 5, kind 7**. Header is the existing 44-byte CHNK header; packed bytes use the existing zero-run codec and CRC32. A chunk is independently decodable.

Each observation has seven unsigned varints, in this order:

| Field | Representation |
|---|---|
| time | absolute game monotonic nanoseconds, u64; same source clock as all other tracks |
| id | u32 recording actor ID; 0 = main player |
| category | bitmask: 1 buddy-set member, 2 ridden-body evidence, 4 WhiteSummonNpc, 8 WhitePhantom |
| ride_flags | bitmask: 1 ride observation valid, 2 mounting, 4 mounted, 8 ride-character |
| ride_state | SDK node state: 0 none, 3 mounting, 5 riding, 7 dismounting; meaningful only when valid |
| ride_param | i32 preserved as its u32 bit pattern and encoded as a varint |
| mount_id | u32 recording ID of the player's observed current mount; 0 = unresolved/not mounted |

No game pointer is serialized. The current pair-node handle is resolved against the live bodies captured in the same batch and replaced by a recording ID. Unknown relationship is explicitly 0, not an inferred pointer.

Capture is approximately 4 Hz plus first actor announcements. Category bits describe evidence, not a promised full actor taxonomy. Buddy membership includes both Torrent and spirit ashes; it is not a spirit-ash type on its own. `ride_flags=0` means unavailable; zero state/param in such an observation must not be interpreted as a known unmounted state.

Observations are sorted nondecreasing by time. For a given actor and T, use the most recent observation at or before T. Seeking backward evaluates that same prefix; there is no forward-only accumulated ride state. A pure unit test exercises backward queries, roundtrip and malformed/truncated payload rejection.

ActorInfo metadata and all frames/context of a callback enter the writer as one queue message. If the queue is full the batch drops; identity announcements are not marked sent and will retry. Readers reject dangling context IDs and duplicate/zero actor IDs. Older readers ignore track 5; new readers accept files without it. This is observed metadata, not an executable mount/dismount instruction or complete spawn/death timeline.

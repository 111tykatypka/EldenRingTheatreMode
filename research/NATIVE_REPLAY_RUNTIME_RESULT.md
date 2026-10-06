# Native replay read-only runtime result — 2026-10-06

## Result

PARTIALLY VERIFIED. Native recorder/object/pool population and local manipulator observed. Native bloodstain ghost playback NOT VERIFIED: user reports no ghost in offline session, and all observed ghost-set active counts are0. Do not ask user to enable online mode. Absence does not disprove native playback.

Exact-build profile passes path/version/product/AMD64/disk SHA/image checks in game log. Probe journal native_replay_21240_124323613409900.jsonl: captured immutable snapshot2426308bytes, SHA256979679b26882470e199c1ae58a107c50013f8cb2a5441f5878342af0c5e00b0c (snapshot, not game hash).1262rows, parser errors0, queue drops0. Raw snapshot kept outside Git in sibling research/native-replay-runtime.

## Runtime evidence

-139 recorder snapshots;139.1788874s observation span;0.99153Hz diagnostic cadence. This is NOT native replay frame rate.
-Recorder vtable/owning player match139/139; pool capacity60; count0..59.556 head/tail/free pointer checks inside pool and aligned0x248.
-Active-tail primary payload sizes72..206;121 distinct hashes. All observed node lengths bounded256; secondary payload0. Decoder/action semantics still UNKNOWN.
-Live player36 distinct positions and31 quaternions. Coordinate ranges include initial unloaded/block-origin change; do not treat that as teleport or wrong memory. Native recorder's converted coordinate space differs from physics local coordinates.
-Local actor: PlayerIns, PadManipulator literal type1; ChrCtrl owner matches. Nearby sampled actor: PlayerIns chr_type5 / ComManipulator literal type5. These are observations, not exhaustive actor classification.
-275 manipulator snapshots: owner+a8 matches corresponding actor. Previous probe read+a0 and logged false failures. Fixed in source, no engine writes involved. Corrected build requires another load only for that diagnostic field, not to repeat proven recorder observations.
-Ghost set112, capacity15, active0. No ReplayGhostIns/type3 observed.

## Limits / next research

No write, ghost spawn, manipulator substitution or unknown virtual calls performed. No game FPS overhead measured; existing CAPTURE_STATS refer to another capture subsystem. Need decode primary payload and trace local ghost actor creation/lifetime from native bloodstain consumer. Candidate duration-controlled pool is not yet proven extensible to minutes. Preserve Phase5 transform playback. Next controlled spawn experiment must wait until parameters, refcount ownership and teardown are understood; offline absence alone is not authorization to call an unknown constructor.

Tests/build results are tracked separately. Source and runtime data do not yet meet full minimum acceptance.

# Full replay system: Step 1 design and the decisions it needs

Branch `claude/v3-ui-phase1`. Written 2026-10-07 before any Step 1 code.

## What the ghost actually is (facts from our logs and the static research)

- The working ghost is the game's own **ReplayGhostIns**, driven by a native **ReplayManipulator** that reads native replay frames (two encoded 256-byte payloads per frame, built from physics/control/module state; codec not fully decoded).
- The game builds those frames from the player's own **ReplayRecorder**, which keeps only a rolling window of a few seconds (39 to 56 frames in your tests).
- The manipulator **only plays forward**: it advances a cursor through the frames. No seek, speed or pause API has been found.
- The frames look like *control/state input* fed into the character's behavior graph (the same family as online player sync), not stored poses. That is why the ghost animates correctly. It is also why it jitters slightly: it interpolates between sparse frames, and positions get corrected after the animation moves the body.
- Appearance and equipment come for free: the factory builds the ghost from the player's metadata.

So the ghost proves we can **spawn a correct-looking character and have the game animate it**. It does not give us **exact, scrubbable** animation. Your definition of done (section 2: same pose at any T, from any direction, animation time within 1 frame) needs more than native ghost playback.

## Decision 1: how puppets are driven (asking before building)

**A. Native ghost playback.** Record the native frames continuously and play them back through the game's ReplayManipulator.
- Fast to a first result, and animations look native.
- Forward-only. Seeking means despawning, respawning at the nearest stored segment and fast-forwarding, so it's approximate and takes time. Accuracy is limited by the sparse native frames (the jitter you saw). The frame format is only partly decoded.
- Cannot meet the 1-frame / 2 cm targets.

**B. Direct pose driver (recommended target).** Spawn the puppet with the native ghost factory (correct look and equipment). Detach its manipulator so nothing simulates it. Then each frame set position/rotation and force the animation state directly from our own recording.
- The binary contains Havok's own state save/restore types: `hkbBehaviorGraphInternalState`, `hkbClipGeneratorInternalState`, `hkaDefaultAnimationControl` (local time) and FromSoftware's `CSDefaultAnimationControl`. Havok designed these for exactly this (networking/replay). If we can capture and restore that state per sample, seeking to T is exact and identical to playing from 0.
- Needs new reverse-engineering: locate the character's behavior graph and its clip generators at runtime (via the RTTI above), then prove a write sticks for one frame. I'd do this as a **research spike** with 2 or 3 short in-game tests before committing.
- Risk: if the graph can't be restored safely, we fall back to setting the clip ID and local time on the active animation controls only, which handles the base animation but not every blend.

**C. Hybrid.** Build A first for a quick working replay, research B in parallel, then swap the driver. This is more total work, and A's limits would be visible meanwhile.

My recommendation: **B**, starting with the spike. If the spike fails, fall back to A and tell you exactly which accuracy targets can't be met.

## Decision 2: file format version number

The old recorder already writes files with magic `ERPLAY03` (the "capture-fidelity" files are format 3, not 2). Calling the new format "v3" would collide with files you already have. I propose **ERPLAY v4** for the new system, with the structure you described (header, world state, entity registry, ~1 s self-contained chunks with keyframe + deltas + events, checksum, compression, footer seek index, crash recovery by chunk scan). v2 and v3 files would show "old format, not supported".

Compression: zstd (better ratio than LZ4, still fast). It isn't vendored yet; I'd add the official single-file build.

## What Step 1 will contain once you choose

1. Recorder (in memory): per tick, the player's position, rotation, velocity, and the full animation state (option B: behavior-graph/clip state; option A: native frames). Equipment/appearance IDs are stored once plus on change.
2. Puppet: spawned with the native factory, manipulator detached (B) or attached (A). AI, collision damage and gravity off.
3. Master clock: play, pause, speed, step and scrub drive "state at T".
4. Accuracy log: per sample, position error (cm) and animation ID/time error between the recording and the puppet, so we measure the targets instead of guessing.
5. Cleanup on stop/unload/new recording/exit, and a 10-cycle test with zero errors in the log.

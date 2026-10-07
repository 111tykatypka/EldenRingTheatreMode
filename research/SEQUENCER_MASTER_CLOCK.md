# Sequencer master clock contract (design, not a new world implementation)

## Current source facts (STATIC_VERIFIED)

`launcher_main.cpp` is the active host entry. `editor_backend.cpp` owns Player and a dedicated playback worker (~120 Hz requested). Player uses steady_clock and integer nanoseconds; the new timescale keeps a fractional-ns remainder. UI transport commands execute under the replay mutex. `ingame_editor_server.cpp` snapshots timeline and maps it to recorded source time. The native bone link uses QueryInterruptTimePrecise to extrapolate between snapshots, bounded at 250 ms. Rust selects/evaluates a pose/root on PostPhysics and applies it on PrePhysicsSafe. Draw_Pre captures/measures skeleton state. This is player playback, not a complete scene clock.

## Proposed authority

Retain host Player as the first master; extend its evaluation context when multi-track support is added instead of creating a second clock. Authoritative context: `{session_id, seek_generation, evaluation_sequence, replay_time_ns, source_segment, timescale, playback_state}`. Real delta advances replay time once. UI/free editor camera use real unscaled delta. Camera paths, editor lights and tracks use replay time. Native scalar mirrors timescale for supported live systems; it cannot make unknown AI reproduce recorded outcomes.

Snapshots need host timestamp paired to native monotonic time, not the receive timestamp alone. Current receive-anchored extrapolation adds variable transport-delay lag; future synchronized clock/latency mapping must be measured. Add generation fencing on seek/load/unload. All tracks evaluate one context once per frame and commit a coherent frame; do not let each actor extrapolate its own time.

## Evaluation and seeking

`Evaluate(T, context)` produces immutable Theater-owned values. Discrete channels use last valid value/event interval; continuous channels interpolate validated brackets; jumps/warp/lifetime changes are cuts. `Seek(T)` builds from nearest valid snapshot <=T plus ordered events. Pose interpolation operates only inside a compatible skeleton/map/lifetime segment.

Two stages: workers decode/copy owned data; game callback validates current objects and applies a frame transaction. Worker threads never retain/dereference engine actors. Apply plan is rejected if generation or streaming/collision prerequisites change; stale events do not fire after seek. Return a frame acknowledgement containing actual committed T and unavailable tracks. Preview can be partial, but must label missing systems rather than pretend WorldState(T) is complete.

Pause sets playback state, not timescale zero. All replay tracks hold T; unscaled editor navigation remains active. Native live-world freeze is a separate ownership mechanism and must not block the task needed to stop/resume. Stop cancels pending frames/events, releases effects/objects, restores temporary native ownership and input. Explicitly distinguish stop, finish, pause and unloaded.

Snapshot/index intervals are configurable/adaptive (initial research target 1–5 s), with bounded decoding work and chunk manifests. Event ordering includes timestamp, actor ID and stable sequence. Reverse seek rebuilds state and reconciles active intervals; it must not simply execute side-effect events backward. Audio/VFX need backend-specific seek/reset rules. Temporal ReShade accumulation must reset or remain visibly approximate after arbitrary seek.

## Diagnostics and tests

Compare expected vs applied time/position/quaternion/pose/counts/events per committed evaluation sequence. Record queue drops, generation rejection, source gaps and missing subsystems. No fabricated dropped-count zero/latency values. Test change-rate-while-playing/paused, sub-ns progression, repeated near-end seeks, disconnect, actor loss/replacement, replay unload and restore. Existing focused clock/IPC tests pass; whole-world synchronized commit/seek does not exist yet.

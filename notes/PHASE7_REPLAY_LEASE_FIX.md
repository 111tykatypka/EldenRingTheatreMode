# Hotfix4 — random replay stops

User reports random early stops. Latest host log confirms actor replay disabled,
successful near-start checks, STARTING -> PLAYING -> ERROR. Game log repeatedly
reports REPLAY_STOP detail=2, with occasional subsequent detail=9. This is not
the configured five-second end or solely a start-position mismatch.

[CONFIRMED] detail=2 is the replay connection lease check failure. The predicate
requires now >= heartbeat and age <=500 ms. PostPhysics captures now before
character capture/research/trace work; the IPC thread can publish a newer heartbeat
or request before replay.tick reads it. Comparing that fresh heartbeat to callback
entry time falsely fails even with an active connection. Playback::frame likewise
rejects a target newer than the outdated callback time. [HIGH CONFIDENCE] This
race explains the random stops; old logs did not record heartbeat age/reason.

Fix: read queued request and heartbeat, then sample current monotonic time. Use
that current time for connection and target lease checks. The PreBehavior replay
connection check follows the same ordering. No future-time tolerance is added;
disconnect, actual future clocks, heartbeat expiry >500 ms and target expiry
>250 ms still reject. No timeouts are increased. Generation/STOP checks unchanged.
Rejected connections now log callback/heartbeat/checked times, age and reason as
REPLAY_LEASE_REJECT. This distinguishes actual stalls from the old timing race.

Regression reproduces false rejection at callback=100, heartbeat=101, then succeeds
with clock sampled after the snapshot (102). It also exercises Playback target
freshness and retains expiry, disconnect and genuine future-time rejection.

## Next test

Close Elden Ring and host; launch Phase7_Runtime_UI_Hotfix4/EldenRingTheaterMode.exe
and use its existing Launcher for the new sibling DLL. DLL changed; game restart
is mandatory. Load the same area and the latest short recording. Keep player-only
XZ diagnostic enabled, actor playback/selected-NPC/raw animation disabled. Choose
First 10 seconds and return to THAT file's initial position (the user has since
made a newer recording; do not use the old recording's coordinates blindly).
Beginning -> Runtime differential trace (10s) -> Play. Avoid movement input.
Expect replay to reach the selected limit; verify Pause/Resume/F6 and normal control.
If it still stops, inspect new REPLAY_LEASE_REJECT and REPLAY_ERROR entries.
No in-game success claimed before the user verifies the new DLL.

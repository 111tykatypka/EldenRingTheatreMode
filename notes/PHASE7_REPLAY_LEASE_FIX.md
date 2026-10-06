# Hotfix4 — random replay stops

## Stationary runtime test — completed

User reported done; logs confirm recording replay_2026-10-06_060803.erplay:
419 samples, 6.9673637 s, 59.993997 Hz, 1 chunk, 214980 bytes. Session
107757156000001 accepted exact start, actors OFF, XZ_ROTATION, and advanced through
the entire file to native REPLAY_STATE=3 / REPLAY_FINISHED and host acknowledged
FINISHED. No lease rejection or error stop in this session. After startup, game
callback/apply rates approximately60 Hz and host IPC approximately120 Hz.
Recorded stationary position matched requested/applied XZ; this verifies one
stationary replay completion, not moving playback, animation, full XYZ grounding,
NPC ownership or pause/resume. Next: fresh short movement recording returning to
its visible original point before stopping; First 5 seconds player-only XZ test.

## Latest Hotfix4 session

Read-only inspection: running host is Phase7_Runtime_UI_Hotfix4, game PID19652.
Runtime reached PLAYER_FOUND. Latest attempted Play calls are rejected by the
host near-start guard BEFORE REPLAY_BEGIN (HOST_REPLAY_STATE=6, session=0).
Loaded recording start=(-5.28739,1.60188,-5.92740), live=(-9.78130,1.58076,-4.69554),
distance=4.65974 units. Actor playback disabled. These latest attempts do not
exercise the lease fix. Older detail=2 entries belong to preceding sessions and
must not be attributed to this new DLL without an accepted new replay session.

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

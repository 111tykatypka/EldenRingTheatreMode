# C13c: continuous Dolly authoring during replay

## Root cause

`camera_runtime::timeline` automatically enabled `dolly_preview` on a stopped-to-
playing transition whenever a matching track had at least two keys. The camera
hook then evaluated the saved track rather than processing manual movement, and
`free_input` became false. Outside the path range, evaluation held an endpoint,
explaining the jump to the last key and the apparent camera lock.

Separately, paused timeline changes triggered path evaluation in manual authoring,
so pause/seek updates could also replace the current editable pose.

## Fix

Replay transport updates now update only master timeline metadata. They neither
enable preview nor write the authoring pose. Removed the implicit paused-seek snap.
Dolly authoring remains manually movable with replay playing, paused or seeking.
K still captures the unshaken editable pose at authoritative current ReplayTime.

Explicit `Preview Dolly path at ReplayTime` and camera cuts retain their existing
evaluation behavior. With preview off and cuts disabled, camera movement continues
to use real time independently of replay speed. Switching to Dolly selects authoring
as before. Camera safety, focus gating and replay-change/disconnect release remain.

## Manual check

Close the old host and Elden Ring, then use the EXE and adjacent DLL from
`outputs/Cinematic-C13c-dolly-authoring` through the existing YAFSML workflow.

Load a replay, select Dolly, keep path preview/cuts off and close F4. Press K,
move, press K again, then press Space to play. The camera should stay at its current
pose and remain movable. Move and capture more K keys while playing; pause/resume
several times without switching camera modes. Enable explicit path preview in F4
to watch the authored track, and disable it to return to authoring.

Status: Release build only; no new tests added or executed. In-game behavior still
requires user confirmation. This package also includes the C13b launcher fix.

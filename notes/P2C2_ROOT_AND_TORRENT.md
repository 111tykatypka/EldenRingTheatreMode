# P2c2 — restore root trajectory and discover mounted companion bodies

2026-10-07; independent branch `codex/p2b-continuation`.

## Direct evidence

- User: player bone poses animate but walking/running/jumping stay in place;
  Torrent not captured.
- Existing parser decoded `Torrent test.erplay.world`: 685 player frames;
  physics position ranges X [-12.258703,11.007728], Y [2.8891125,9.670378],
  Z [-0.3543443,27.906324]. Stored `place.global` (ChrIns.chunk_position) is
  constantly [-16,-104,-80]. The previous root formula replaces recorded XYZ
  with this constant minus a live-derived offset, cancelling the trajectory.
- Mounted recording logs: buddy set capacity 80, status 4: one entry, status 0:
  79 entries. The Active-only filter discarded that entry throughout recording.
- A later status-2 buddy candidate had character_id/npc_id 8002, ride-character
  flag, valid physics, but no pose importer/skeleton. Read-only external RPM
  confirmed importer at ChrIns+0x398 was null for that candidate. It is not
  evidence of the active rendered Torrent's skeleton layout.
- The game closed before the requested read-only mounted-body inspection could
  be completed. No additional game writes or runtime success claimed.

## Changes

Player and actor root playback now retains the interpolated recorded physics
position and quaternion. Bone local/model tracks remain separate, evaluated on
the same replay time. No animation commands or simulated animation root motion
are introduced: locomotion displacement comes from recorded physics XYZ.

Origin/block-anchor metadata is now used only as a coordinate-frame guard. It
is not treated as a moving actor position. For same-origin/same-anchor playback,
the stored physics trajectory is applied directly, including vertical motion.
Unverified conversion across different origins stops application rather than
guessing a translation. Automatic travel relying on the old false position is
blocked for different origins in this experimental build. Cross-region placement
is a remaining limitation, not a working feature. Old files retain their raw
physics samples; no replay-format change is needed for this correction.

Buddy discovery includes Active (2) and ReadyForActivation (4), with readable
physics-owner validation retained. Initializing, unloading and unknown statuses
remain excluded. Full skeleton-count and pose-array checks still decide whether
a body can be recorded. The capture pipeline stores every accepted companion's
own local-space hkQsTransforms, model-space hkQsTransforms and independent root
transform, not the player's pose. Model-space is skeleton/model space; a world
bone transform is composed with that actor's root, not mislabeled as local pose.

Actor pointer/count scalar probes now use ReadProcessMemory rather than unchecked
volatile dereferences. Diagnostic candidate logs include importer/skeleton
addresses. The SDK model-ID 8000 hint is not used to bypass role/lifecycle guards;
live evidence already shows a related body can have ID 8002.

## Validation

Release adapter: 30 tests passed, one real-file inspection ignored by default.
The real-file inspection was run explicitly and passed using the actual fixture
above. New tests cover constant anchor vs changing root and allowed/excluded
buddy statuses. Compilation is not proof of mounted-body rendering.

## Next manual test

Package: `outputs/P2c2-root-motion`. Close the game and all hosts, launch this
package's EXE and use its established offline launch button. It loads the DLL
next to this EXE; do not replace original packages or game files.

1. On foot, record walking/running/a small jump for 10 seconds with F5/F6.
   Stay in that area, load and Play. Confirm both translation and pose.
2. Unload the replay and return to normal control. Mount Torrent before F5,
   record 10 seconds without dismounting, then F6. Stay mounted for Play.
3. Report whether both rider and Torrent move/animate. Send the game log and
   recorder log. Expected recording evidence: COMPANION_SCAN accepted >=1,
   COMPANION_CANDIDATE with non-null importer, equal positive skeleton counts
   and pose_arrays=true, followed by COMPANION_RECORDED and nonzero summary.
4. If no importer is present for the status-4 rendered body, keep the game
   running mounted without recording/playback for the next read-only inspection.

Missing bones cannot be recovered from previous recordings. Automatic mounting/
dismounting, missing body spawning and cross-origin conversion remain unresolved.

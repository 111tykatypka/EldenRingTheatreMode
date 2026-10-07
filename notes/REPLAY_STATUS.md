# Theater Mode replay status (claude/v3-ui-phase1)

Main line. Includes Codex's independent-development commits d4deae4..c2a8f88 (merged 2026-10-07) plus review fixes.

Working and confirmed by the owner in game:
- F5/F6 records the player's 150-bone pose (local + model space) and physics transform beside the replay (`<replay>.erplay.bones`).
- A loaded replay drives the player body from the overlay timeline: play, pause, scrub.
- Continuous playback speed slider (Codex; owner confirmed the slider works).

Changed in review, NOT yet confirmed in game:
- Bone interpolation between recorded frames is ON by default (THEATER_POSE_INTERPOLATION=0 turns it off). It only runs while a replay owns the body, so it cannot affect menus or normal play.
- Model space is rebuilt from the interpolated local pose through a skeleton hierarchy learned from the recording (parent * local, Havok setMul), so limbs stay attached. Bones the hierarchy does not explain interpolate on their own.
- One bad bone keeps the earlier frame instead of stopping the replay.
- No interpolation across recording gaps or teleports (> 1.5 m between frames).
- Accuracy log compares the drawn body with the interpolated recording and skips the settle frames after a start or seek (the old number showed a false 749 cm).

Still quarantined: world timescale writes (adapter/src/timescale.rs). Startup slowdown cause unknown; the recovery build ran at 60 Hz. Re-enable separately, read-only diagnostics first.

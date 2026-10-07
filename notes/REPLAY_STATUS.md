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

## Phase 1.1 cleanup (2026-10-07)
Removed: native ghost (C++ prototype, fingerprints, Rust layout/appearance), the top-right ghost HUD
(game-side messages now go to the overlay event log via tm_render_event), native bloodstain research,
the position-only in-game replay (host in_game_replay, adapter replay_runtime/transform_replay/actor
replay/local input/start guard/return), the transform probe, locomotion/runtime traces, ownership
probe, world-timescale writes (the spec says the replay clock must never slow the game), the old
modern_ui/monitor host UIs and the animation-request experiment. The control pipe is now a status
link only (control_link.rs). Recording (samples, actions, characters) is unchanged.

Facts kept from the removed research (move into GameProfile when used):
- ChrIns debug flags are at +0x538 (constructor RVA 0x3E7409); +0x530 is a callback pointer
  (RVA 0x3F8FD0), which is why writing the SDK's debug_flags crashed. Bits 0x10/0x20 were
  cross-checked as noMove/noAttack; +0x539 holds noUpdate.
- CSLuaEventManImp: lua_event_proxy at +0x8 (is_load_wait), lua_event_script_imitation at +0x18
  (lua_warp_bonfire_entity_id at +0x1C, is_wait_reentry_to_map) - warp/load observation for 1.4.

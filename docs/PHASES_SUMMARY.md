# Theater Mode: next phase, combined summary and test checklist

One running document for Phases 1 to 3.4 (owner asked for one test pass at the end).
Build: `outputs\EldenRingTheaterMode\TheaterMode-Current` (BUILD_NAME.txt says which build).
Every claim below is "built and unit-tested" unless it says "confirmed in game".

## Phase 1: cleanup + playback quality

### What changed
- **1.1 Old ghost removed.** The top-right ghost box and its code are gone (native ghost prototype,
  ghost look, native bloodstain research, the position-only in-game replay on host and game side,
  transform probe, traces, ownership probe, world timescale, old launcher UIs). About 4,000 lines.
  Game-side messages (bone replay, arrival, equipment) go to the overlay event log (Debug panel).
- **1.2 Smooth at any speed.** Each game frame picks one replay time and uses it for both the bones
  and the root (they were a frame apart). A replay clock runs between timeline updates (~20/s from
  the host), never steps backwards while playing, and takes seeks at once. Bones interpolate per
  frame (rotations SLERP, translations/scales lerp) and model space is rebuilt through the skeleton
  hierarchy learned from the recording, so limbs stay attached. All frames are decoded in memory;
  nothing is decompressed per frame. The game's own speed is never changed.
- **1.3 Speed slider.** 0.01x to 4x, logarithmic, marks at 0.1 / 0.25 / 0.5 / 1 / 2 (magnetic,
  Shift/Ctrl for fine), double-click or right-click resets to 1x, exact value box, mouse wheel.
  Speed is a host timeline property (ready for speed ramps later).
- **1.4 Right place.** Every frame records the map block, its origin block and the global (map chunk)
  position. Playback converts recorded positions into today's physics space, so map tile streaming
  and tile crossings don't misplace the body. If the replay was recorded in another map or more than
  400 m away (overworld), the player fast travels with the game's own grace warp to the nearest grace,
  playback waits until loading has finished (game's load-wait flag, then 1.5 s stable), places the
  body with gravity off, and checks it is within 0.5 m (3 tries) or shows a clear error.
  Limitation: places with no grace in their area can't be travelled to; the log says so. After a
  warp you stay at the replay's place when it ends (your old spot is in another map).
- **1.5 Equipment.** Every frame records the render equipment assembly: grip (empty, one-handed,
  two-handed left/right), active weapon/arrow/bolt slots, and all 22 equipment pieces (weapons,
  shields, catalysts/seals in their slots, armor, talismans) with their inventory handles. Playback
  writes the frame's full assembly before the pose, so seeking backwards across a swap reverts it,
  and puts your own equipment back afterwards. The log reports whether the game kept the written
  assembly until drawing.
  Not 1:1 yet, and why: weapon buffs/infusion glow are effects (Phase 2.5); sheathing moves weapons by
  animation events we don't replay, so a sheathed weapon may show in hand; Torrent is Phase 2.3.
  Weapons follow the hand bones exactly because the bones are exact; separate per-weapon transforms
  need the weapon model objects, which are researched with props in Phase 2.4.
- **UI sounds** (your sound folder): overlay show/hide -> inventory open/close; hovering a control ->
  menu focus; rail tool -> menu tab; play/load/restart/Space -> ok; pause/stop/unload/delete -> cancel;
  previous/next/page -> prev/next; sort/select/rename -> bracket; recording start/stop -> ok/cancel;
  game messages -> ui_message (max once a second); other clicks -> ok. Scrubbing and speed drags are
  silent. Settings panel: on/off and volume (default on, 60%). Sounds play on their own audio thread.
  Note: your ui_menu_prevnext.wav is only 0.01 s long, so it is nearly silent.
- **Offsets** now live in shared/GameProfile.h (TM_OFF_/TM_VAL_/TM_AOB_) and are exported to Rust.

### Test checklist (Phase 1)
1. Start the game with the launcher in TheaterMode-Current. Press F4: the open sound plays; no ghost box anywhere.
2. Hover and click a few panel buttons: focus and click sounds. Settings: switch sounds off and on, change volume.
3. F5, name it, then run, roll, attack, two-hand your weapon (hold E/Y), switch right-hand weapon, F6.
4. Load it in Replays and play at 1x: your character repeats everything, including the two-hand grip and weapon swap.
5. Drag the speed slider to 0.1x and type 0.05: motion stays smooth, FPS stays normal. Double-click: back to 1x, no jump.
6. Scrub backwards across the weapon swap: the earlier weapon comes back.
7. Walk 50 m away and play again: you are put back on the recorded spot.
8. Ride or warp to another region, load the replay, press Play: the game fast travels to the nearest grace,
   then places you on the recorded spot and plays. (If it can't, the event log says why.)
9. Record across a long run over the overworld (crossing map tiles) and play it: no jump or fall at tile borders.
10. After playback ends (stop the timeline and close the overlay): your own equipment is back, controls work.
Send the event log (Debug > Copy all) and %TEMP%\TheaterModeGame.log if anything looks wrong.

## Phase 2: whole world (in progress; stopped here for testing)

### What changed
- **2.8 File and performance (core).** Recordings now stream to `<replay>.erplay.world`: one-second
  chunks per track with a keyframe at each chunk start (any point seeks by decoding one chunk),
  compact encoding (rotations as "smallest three", 0.01 mm translations, varint deltas, zero runs
  packed, CRC per chunk). The game thread only queues data; a background thread writes, and a stalled
  disk drops frames (counted in the log) instead of stuttering the game. No more 10-minute in-memory
  buffer. The log reports MB per minute when a recording is saved. Older `.bones` replays still play.
- **2.1 World.** Once a second: the in-game clock and every event flag change (full flag copy at the
  start, then changes). Playback holds the recorded time of day and gives your own time back after.
  Settings > Replay world > "Restore doors, fog walls and bosses as recorded" (off by default) also
  sets the flags the recording changed to their value at the replay time and puts your own back
  afterwards. Not 1:1 yet: weather (the weather controller isn't described by the game library we use;
  needs research), and whether fog walls/doors visibly react to a flag change mid-scene is unverified.
- **2.2 Enemies, NPCs, bosses** (approach (a), recommended in the thread): every character within
  100 m gets a stable recording id (entity id, NpcParam, handle; never a pointer) and is recorded with
  root, global position, HP and its full skeleton (bone count read from its own skeleton and only
  trusted when three counts agree), every frame within 30 m and every 3rd frame beyond. On playback
  each recorded character is matched to the live one (same entity id + NpcParam, or handle), held
  with the game's own no-move/no-attack flags (written only after a structural check of the flag
  word), gravity off, and driven with interpolated bones and root like your character. When playback
  ends they get their flags and position back. Limits: characters that no longer exist (a defeated
  boss) can't be shown; characters that appear later or died earlier aren't hidden yet (needs
  spawned puppets, approach (b)). HP is recorded but not yet applied.

### Test checklist (Phase 2 so far)
1. Near a group of enemies, F5, fight for 20-30 s (kill one), F6. The event log / TheaterModeGame.log
   says "WORLD_FILE: saved ... N actors ... MB per minute"; please send that line.
2. Walk away a bit, load the replay and play: your character and the enemies repeat the fight;
   live enemies don't attack you during playback. Scrub back and forth: everyone follows.
3. Stop playback and close the overlay: enemies are back where they were and act normally again.
4. Record across a dusk/dawn change (or wait a minute in-game), play it: the sky follows the
   recording; after playback your own time of day is back.
5. Optional: tick Settings > Replay world > Restore doors, fog walls and bosses, record opening a door
   or a fog wall, then play it back from before that moment. Tell me what you see.
6. FPS while recording and playing near many enemies: tell me if it drops.

## Where work stopped (resume point)
Done: Phase 1 (1.1-1.5), UI sounds, 2.8 core, 2.1, 2.2 (build P2b).
Next, in order: 2.3 mounts and summons, 2.4 props, 2.5 projectiles/VFX/SFX, 2.6 events and params,
2.7 timeline tracks (follow docs/theater-ui-v4 blueprint A.7, load card A.6), then Phase 3.1-3.4
(cameras; CameraTools is a proprietary binary, behaviour reference only).
Open questions in the thread: enemy approach (a) vs (b), .world file format (both proceeding on the
recommended option).

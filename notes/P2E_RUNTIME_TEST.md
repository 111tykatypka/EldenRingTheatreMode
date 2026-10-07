# P2e1 controlled capture test — not a resurrection test

Status: IMPLEMENTED — RUNTIME VALIDATION REQUIRED.
Do not interpret this package as full NPC/boss/VFX replay.

## Package and launch

1. Close Elden Ring and all Theater Mode hosts before replacing a loaded DLL.
2. Use only this matching experimental pair:
   `outputs\P2e1-actor-observations\EldenRingTheaterMode.exe` and `TheaterMode.dll`
   under `EldenRingTheatreMode-P2b-independent`.
3. In the existing offline YAFSML setup, select that package's DLL path. Use the
   existing launcher/settings workflow. Do not copy anything into the game directory.
   Preserve the P2d/Phase5/original Claude packages; they are rollback checkpoints.
4. Launch this EXE, then launch the game through the configured offline YAFSML workflow.
   The target guard is still 2.7.0.0 / patch 1.17 and the previously verified disk SHA.
5. Load a save near one ordinary enemy, in a familiar area. Do not begin with a boss,
   quest NPC, summon or Torrent. No spawn/revive/VFX command is enabled by this checkpoint.

## Capture

- F4 opens the existing overlay. Confirm connection/player before recording.
- F5 starts recording. Observe the recording indicator.
- Keep the enemy alive initially, let it move/attack, then hit and kill it normally.
  Record a few seconds of its death/corpse/disappearance, then F6 stops recording.
- This kill is ordinary gameplay and may persist normally. Theater adds no death,
  quest, HP, reward, event-flag or progression mutation for this test.
- Find the new `.erplay` and companion `.erplay.world` in the configured replay folder.
  Do not use an old file to assess the new observation track.

## Evidence

Send the two replay files and `%TEMP%\TheaterModeGame.log` plus
`%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log` if needed.

New logs: `ACTOR_OBSERVATION` reports id, observed/death/unknown state, availability,
death flag, HP, pose availability, reason and source-clock timestamp. These are
transition logs, not invented animation or corpse events.

Run the read-only inspector from the independent source checkout:

```powershell
python tools/inspect_actor_lifetimes.py 'FULL_PATH_TO_REPLAY.erplay.world' --at 2 --at 7 --at 13 --at 5 --output actor-lifetimes.json
```

Use seconds appropriate to when the actual enemy died. The inspector prints metadata,
not a rendered actor. Check that the death flag transition is consistent with the
visible death, and identify exactly when pose/render availability ends. HP=0 alone
must not produce DeathFlagged. A radius/enumeration gap must report UNKNOWN.

Optional existing-body playback still cannot bring back a removed actor. If the live
enemy is dead but an earlier replay observation was alive, expect
`ACTOR_RECONSTRUCTION_REQUIRED`, not a revived enemy. Normal player replay is retained.
Stop disarms replay and retains existing restoration behavior. Observe this in-game;
automated tests do not establish that it is visually correct.

## Following milestone

Before the requested dead-enemy rewind acceptance test, prove an isolated native
character construction request, exact ownership and removal/cancellation path, save/
reward/script isolation and no AI/TAE gameplay effects. Then implement one ordinary
replay puppet and validate alive -> death -> backward seek -> alive -> removal.
Do not use boss flag clearing or assume a native ghost timeout authorizes another callsite.

# Theater Mode: phase status and decisions

Branch `claude/v3-ui-phase1`. Updated 2026-10-07.

## Phase 1: bug fixes (built, waiting for your in-game test)

Build: `outputs\EldenRingTheaterMode\TheaterMode-Phase1\EldenRingTheaterMode.exe`

| # | Cause found | Fix |
|---|---|---|
| 1 Cursor flicker | Your last session's render log shows the game sends **both** window mouse messages and DirectInput mouse data. My overlay fed ImGui from both on alternate frames, and the Windows cursor was also visible. | Exactly one source per frame: window messages (position and buttons), falling back to DirectInput only if no window mouse message arrives for 1 s. The Windows cursor is hidden while the overlay is open (`WM_SETCURSOR`), ImGui never calls `SetCursor`, and the overlay draws the only arrow. |
| 2 Lost clicks, cursor jumps | Same cause: the press and the release came from different sources and positions, so ImGui saw a click that started on one tab and ended elsewhere. The game can also confine the cursor with `ClipCursor`. | Single source (above). `SetCursorPos` was already blocked while the overlay is open; `ClipCursor` is now widened to the whole game window. |
| 3 Ghost works once | Second F10 in your log: the game's eligibility check refused that moment (`written=2862 size=4133`), and my code treated every failure except one as fatal for the session. | Any create that ends without a ghost and without a buffer overrun can be retried. The game's own periodic serializer skips the same results. Only an overrun still requires a restart. |
| 4 Ghost look | Replay ghosts are drawn with PhantomParam rows 910 and 930 (from `NetworkParam.replay_bonfire_phantom_param_id`). Originals logged: alpha 0.3, diffuse x0.1, white rim, glow 0.3/0.7. | No toggle. Once params load, both rows are set to a normal character look in memory (blend rate 0, alpha 1, no rim/glow/tint). No game files change. |

What to check: the arrow is single and smooth, every tab opens on the first click, F10 can be repeated (walk a few seconds between), and the ghost looks like a normal character.

Logs: `%TEMP%\TheaterModeGame.log` (`GHOST_APPEARANCE`, `NATIVE_GHOST`), `%TEMP%\TheaterModeRender.log` (`CURSOR source=...`).

## Facts that shape Phase 2 and 3

These come from the live logs and the exact-build static research:

1. **The ghost is the game's own replay system.** It plays data from the player's native `ReplayRecorder`: a **rolling window of a few seconds** (39 and 56 frames in your tests). It is not driven by our ERPLAY recording.
2. **Only the player has a native recorder.** NPCs, bosses and objects have none, so the ghost approach cannot record them.
3. **Native replay plays forward only.** No seek API has been found; the manipulator advances a cursor through the data.

## Decisions I need from you

**D1: How the ghost becomes the main replay (item 6).** My recommendation is a hybrid.
- Recording: keep the ERPLAY transform/action tracks as the timeline's ground truth, and add a v3 track that stores the player's native recorder payload, collected continuously by repeating the same serialization the game does.
- Playback: spawn one ghost per stored payload segment, in order.
- A research spike comes first, with one in-game test, to prove that consecutive segments hand over without a visible jump. If they don't, the fallback is to use the ghost only for the look and animation while our transform track drives its position. That fallback is unproven too.

**D2: Scrubbing (item 8).** Because native replay is forward-only, scrubbing would destroy and respawn the ghost at the segment containing the target time, then run it forward to that time. Expect a short catch-up of up to one segment (a few seconds) rather than frame-exact jumps. Is that acceptable?

**D3: NPCs and bosses (item 9).** These need a different mechanism from the ghost: control real NPC instances (risky ownership and AI fights), or spawn copies (unresearched). I'll write the feasibility report first, as you asked, and implement nothing there until you choose.

**D4: Spacebar (item 5).** Proposal: Space toggles the sequencer while the overlay is open (game input is already blocked then). With the overlay hidden, Space toggles only while a replay is loaded, and the DirectInput hook removes just that key from the game, so it doesn't jump. With no replay loaded it stays a normal game key.

Item 5 (keybinds) and item 7 (10-minute cap) need no decision. I'll start them as soon as Phase 1 passes your test. Dolly cams (13) will wait for your reference files.

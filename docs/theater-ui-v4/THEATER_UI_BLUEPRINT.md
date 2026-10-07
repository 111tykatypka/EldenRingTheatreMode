# Elden Ring Theater Mode: UI Implementation Blueprint (v4, bone replay + whole-world, cameras, lights)

Target: Dear ImGui (1.89+, compiled against 1.93 WIP) on the DX12 present-hook overlay.

| File | What it is |
|---|---|
| `TheaterTheme.h` | Colours, spacing, font roles, metrics, `ComputeUiScale(W, H)`, `ApplyStyle()`, `LoadFonts()`. |
| `TheaterLayout.h` | `SolveLayout()`, the resolution-independent layout solver (menu bar included), `ClampToDisplay()` for floating panels, and picture↔back-buffer mapping. Its header comment lists real outputs for 11 resolutions. |
| `TheaterUITypes.h` | View models, `TheaterCommand`, `UIState` (with the F4 `UiVisibility`), the loading state machine, `Speed::` mapping, track categories, camera objects, lights, the project file, entry points. |
| `screens/*.png` | Native-resolution renders of every screen (v4). v3 renders are kept in `screens/v3/`, v1 in `screens/v1/`. |
| `mockups.html` / artifact | Interactive mockups. The resolution picker runs the same solver. |

## What changed in v4

v4 brings the UI in line with what the engine now does (bone replay on the player's own character) and designs the planned phases (whole-world replay, cameras, lights, Look, project file). The v2/v3 rules still hold: minimal on-screen info, left rail, F4 hides everything, any resolution, no empty space.

| Area | v4 change |
|---|---|
| Menu bar | New 28 px bar on top: **File** (New/Open/Save Project, Open Replay, Unload Replay, Export), **Edit** (Undo/Redo, preferences), **Layout** (Reset Layout plus a show/hide toggle for every panel and every track group), **Help**. On the right: project name with saved/unsaved dot, Record F5 (or REC time with F6 stop), F4 Hide UI. This replaces the old "no top bar" rule, at the user's request. |
| Panels | Every panel can be resized from its edge (a grip on hover) and dragged out to float. `ClampToDisplay()` keeps a floating panel on screen. The layout is saved to `%LOCALAPPDATA%\EldenRingTheaterMode\layout.ini`. Layout → Reset Layout restores the solver defaults. |
| Rail | Six tools: **Scene, Camera, Lights, Look, Replays, Export**. Bottom: Debug, Settings. Brand diamond moved to the menu bar. |
| Replay model | Replays are **bone-pose replays on the player's own character** (150 bones, 60 Hz, ERPLAY v4). The ghost system and its top-right HUD are gone. Status text goes to the event log and the Debug panel, never over the game. While a replay drives the character, game input is locked (shown as `Input LOCKED` in Debug only). |
| Transport | Space toggles play/pause. **Eject** button unloads the replay. The speed control is a **log slider 0.01×–4×** with a numeric box, snap marks at 0.1/0.25/0.5/1/2 and double-click to reset to 1×. Speeds below 1× show in amber. A **Speed** track holds speed keys (ramps) once the engine supports them. Hide UI moved from the toolbar to the menu bar. |
| Hotkeys | F5 record, F6 stop, F4 overlay toggle, Space play/pause (also while hidden). |
| Loading | Opening a replay shows a **load card** on the dimmed game picture with six steps: open file, warp to the recorded spot, wait for map and collision, place and verify the player (within 0.5 m, 3 tries), apply equipment, prepare actors/project. Cancel and Retry Placement. The badge reads LOADING. |
| Equipment | The selection block lists the recorded equipment (right/left hand, armour, mount). An **Equipment** track marks each change. |
| Timeline | Tracks are grouped by **category**: Camera Cuts, Speed, Player, Equipment, Enemies & Bosses, Mounts & Summons, World, Props, FX & Projectiles, Events, Lights, Look, Bookmarks. Each group collapses, shows its count, and can be hidden from the Layout menu. Track tree is 250 px. A vertical scrollbar appears when rows overflow. A filter field sits in the track header. |
| Cameras | **Camera objects** (Free Camera, Dolly, Orbit, Bone-attached). Dolly paths are **Catmull-Rom (centripetal)** or **Bezier**, with optional constant speed. **Look At** a target or bone, with a reticle on the picture. **Camera Cuts** track switches between cameras. Per-key interpolation (Linear / Smooth / Bezier / Hold). The v3 key gizmo is unchanged. |
| Lights | New **Lights** tool: spot and point lights with colour or Kelvin, intensity, range, falloff, cone, softness, shadows, volumetric, attach to an actor or bone. Every value is keyframeable. Cones and radii are drawn on the picture with the same gizmo as camera keys. |
| Look | Rebuilt: **Time & Weather** override, **Camera** (exposure, DOF, motion blur, bloom, vignette), **Visibility** (hide HUD, hide player, hide actors, High Quality LODs). Keyable values carry a key diamond. |
| Project | A **project file** (`.thproj` next to the replay) stores cameras, lights, looks, cuts and bookmarks. Opening a replay offers to load its project. The menu bar shows the name and whether it is saved. |
| Debug | Now **bone replay** debug: format, skeleton, sample rate, replay clock, pose error, map tile, warp check, input lock, data rate. |

Trade-off: the menu bar and the taller timeline (300 px minimum, 7 rows) cost the game picture about 6% of its height at 1440p (1112 rows instead of 1180). Dragging the timeline edge down gives it back.

Screens: `01-editor-*` (7 resolutions), `02-hide-ui-f4-*`, `03-camera-*`, `04-lights-*`, `05-look-*`, `06-replays-loading-*`, `07-export-*`, `08-debug-*`, `09-design-system@2x`.

Earlier versions in short: v2 removed the toolbar/status bar, merged the panels into one side panel, added F4 and the solver that scales the game picture into the free area. v3 polished the timeline (navigator, tooltips, in/out handles), added the dolly key gizmo, Look, Replays and Export.

---

## A. Screen specification

### A.1 Editor (UI shown)

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│◆ File Edit Layout Help   💾 Replay_001.thproj · saved        ● Record F5  F4 Hide│ 28
├────┬──────────────┬──────────────────────────────────────────────┬───────────┤
│rail│ SIDE PANEL   │                                              │ SELECTION │ ← only when
│ 64 │ 300–640      │      GAME PICTURE (scaled, own aspect)       │ 280–360   │   width is
│Scene│ fixed tool  │                                              │ (optional)│   spare
│Cam │ controls     │                                              │           │
│Lite│──────────────│                                              │           │
│Look│ event log    │                                              │           │
│Repl│ (fills)      │                                              │           │
│Expt│             ⋮│ ← grip: drag to resize                       │           │
│    ├──────────────┴──────────────────────────────────────────────┴───────────┤
│Dbg │ Replay_001 ⏏ [PAUSED] ⏮◀ ▶ ▶⏭ 00:26.300 / 02:14.500  ⏱━━●━━ 1.00×  ● F5 ◆ K│
│Set │ TRACKS 12  ⌕ | ruler / lanes, grouped by category, v-scroll when overflowing │
└────┴──────────────────────────────────────────────────────────────────────────────┘
```

- **Menu bar** (28 px, full width): see the table above. Menus open as ImGui popups. The bar has no other content, so it reads as chrome, not information.
- **Rail** (left, under the menu bar): Scene, Camera, Lights, Look, Replays, Export; bottom Debug and Settings. Each button is 52×52 with a 22 px icon and a 10.5 px label. Clicking the active tool closes the side panel, and the solver gives that width to the game picture. An amber dot on Debug means the replay backend warned.
- **Side panel**: the tool's fixed controls, then the **event log** filling the rest. Resizable from its right edge (grip), and can float.
  - *Scene*: search, then the actor tree grouped as **Cameras** (Free Camera live, Dolly_01 6 keys, Cam_BossHead bone), **Player** (Your Character, bones), **Enemies & Bosses** (with phase/dead state), **Mounts & Summons**, **Props** (doors, gates, breakables), **Lights**. Then the selected actor: "Your Character · bone replay · 150 bones · 60 Hz", equipment rows, position, **Follow** and **Focus**.
  - Other tools: A.3–A.6.
- **Selection column** (right): only when the solver has ≥280 px it would otherwise waste. Full actor details plus that actor's events; the Scene panel then drops its own selection block.
- **Game picture**: the game frame scaled into the free area at its own aspect. Overlays drawn in picture space: selection brackets, camera paths and keys, look-at reticle, light cones (Lights tool), rule-of-thirds (Camera tool), the load card (A.6) and top-centre toasts. **No replay HUD** is drawn over the game.
- **Sequencer toolbar**: replay name, eject (Unload), state badge, transport (Space), timecode, speed slider, Record F5 (or Stop F6 while recording), Key K.
- **Timeline**: one row per track category (A.7). Groups collapse; the selected actor's rows auto-expand while there is height.

### A.2 Hidden UI (F4)

- The game returns to full screen, presented by the game itself. The overlay skips the frame copy, so it costs nothing.
- A small permanent hint sits at the **bottom-left**: `F4 Show UI`. It stays as long as the UI is hidden.
- While recording, the **recording pill** sits top-left: red dot, REC, elapsed time, frame count, `F6 stop`.
- **Space always goes to the game** while hidden (play/pause of the replay is not bound to Space then; use the UI). F5/F6 still start and stop recording.
- **Shift+F4** hides the hint and the pill too, for clean capture.
- F4 again restores the last layout; the picture scales back into its rect over 120 ms (or instantly with reduced motion).

### A.3 Camera tool

Top of the panel: the **camera list** (Free Camera LIVE, Dolly_01, Orbit_Boss, Cam_BossHead) with add/duplicate/delete. Then the selected camera's settings:

| Field | Values |
|---|---|
| Type | Free · Dolly · Orbit · Bone-attached |
| Path | Catmull-Rom (centripetal, the default: no loops or overshoot) · Bezier (handles on each key) · Linear |
| Constant Speed | On: the camera moves at even speed along the path regardless of key spacing (arc-length reparameterised). Off: key times rule. |
| Look At | None · actor · actor bone (e.g. Boss_001 · Head). Draws a reticle and a dashed line on the picture. |
| Attach To | Bone-attached cameras only: actor + bone + local offset. |
| FOV, Focus, Roll | Keyframeable. |

Then the **Keys** list (time, name, interpolation column: Linear / Smooth / Bezier / Hold) and an Interpolation + Ease row for the selected key. Buttons: **Key from View (K)**, **Look Through (V)**.

The **key gizmo from v3 is unchanged**: Move W, Rotate E, Aim A, Local/World, Ctrl to snap; the key keeps its time and nothing is deleted; Look Through + Set from View (U) writes a flown camera back into a key. Key time is edited only in the sequencer.

**Camera Cuts** (top timeline track) holds coloured blocks, one per active camera. Dropping a camera on it, or pressing C at the playhead, adds a cut. Playback and export view through whichever camera the cut track names; without cuts, the selected camera is used.

### A.4 Lights tool (new)

| Block | Fields |
|---|---|
| List | Key Light, Rim · Boss_001, Fill, Torch_02 with S (solo) and eye per row. Add Spot / Add Point / duplicate / delete. |
| Selected light | Type (Spot / Point segment), Color (swatch + **Kelvin** bar 1900–10000 K), Intensity, Range, Falloff, Cone Inner / Outer (spot only), Softness, Shadows, Volumetric, Attach To (world, actor, bone). Every row has a key diamond. |
| Actions | **Place at Camera** (puts the light at the current view, pointing where you look), **Key K**. |

On the picture, spot lights draw a cone outline and point lights a radius ring, each with a chip (name, Kelvin, intensity). The selected light gets the same ImGuizmo gizmo as camera keys. A **Lights** track group holds one row per light with its keys.

The number of shadow-casting lights the engine allows is not yet known, so the UI does not show a limit. When the engine reports one (`LightCapsViewModel`), the Shadows checkbox greys out with a tooltip once it is reached.

### A.5 Look tool

| Block | Fields |
|---|---|
| Time & Weather | **Override** checkbox (off = the replay's recorded time and weather). Time of Day, Weather, Fog Density, Fog Distance. |
| Camera | Exposure, Depth of Field (aperture, focus from the active camera), Motion Blur, Bloom, Vignette. |
| Visibility | Hide HUD, Hide Player, Hide Actors (pick list), **High Quality LODs** (forces the highest LOD on nearby actors). |
| Actions | **Key Look (K)**, **Compare** (`\`, split Before/After). |
| Presets | The flexible list (20), star to pin. |

Keyable values show a key diamond; their keys appear in the **Look** track group.

### A.6 Replays tool and loading

**Record** block: Radius (100 m + active bosses), Sample Rate (60 Hz), Record F5 / Stop F6. **Library** fills the rest: one row per replay with duration, location and size. The selected row has a **Load project** checkbox when a `.thproj` exists. Footer: Open, Unload, Export.

Opening a replay shows the **load card** centred on the dimmed game picture (440 px):

| Step | Shown as |
|---|---|
| 1 Warp to the recorded spot | place name and map tile, e.g. Leyndell, Royal Capital · m11_00_00_00 |
| 2 Map and collision loaded | time taken |
| 3 Place your character | "check ≤ 0.5 m · try 1 of 3"; after 3 failed tries, Retry Placement |
| 4 Equipment | weapon names |
| 5 Replay actors | entity count |
| 6 Project | cameras, lights, looks (only when Load project is ticked) |

The card's title names the replay with its length and date, and a progress bar sits under the steps. Steps are ✓ done, spinner active, grey pending, red failed (with the reason). The footer says "Your character is frozen until it's placed" and **Esc cancels** (restores gameplay). The badge reads LOADING; the transport is disabled. When done, the card fades and the state is READY (paused at 0).

### A.7 Timeline track categories

| Category | Rows / markers | Default |
|---|---|---|
| Camera Cuts | coloured blocks per camera | shown |
| Speed | speed keys and the curve (ramps) | shown |
| Player | animation, attacks, rolls, hits | shown, open |
| Equipment | equip changes | shown |
| Enemies & Bosses | per actor: attacks, phase, death | shown |
| Mounts & Summons | Torrent on/off, summons | shown |
| World | time, weather, map tile changes | shown |
| Props | doors, gates, breakables | collapsed |
| FX & Projectiles | spells, arrows, VFX | collapsed |
| Events | pickups, messages, bonfires | collapsed |
| Cameras / Lights / Look | keys per object | shown on their tool |
| Bookmarks | M | shown |

Whole-world tracks show only what the engine actually records. A category with no data is hidden, never shown empty.

### A.8 Export tool

Unchanged from v3: exports the In/Out range (or the whole replay) as video (HEVC/H.264/AV1 via NVENC, ProRes on CPU) or an image sequence (PNG 8/16-bit, JPG, EXR), with resolution, frame rate, motion-blur sub-frames, audio, file path, estimate, Export/Queue and a job list. v4 change: export renders **through the Camera Cuts track** and applies Look and Lights keys, because frames are rendered by stepping the replay. See F.4.

### A.9 Debug (bone replay)

Fields: ERPLAY v4, Skeleton (150 bones), Sample Rate, Replay Clock, Drawn Error (pose error vs recording), Map Tile, Warp Check, Input LOCKED, Data Rate (amber when high). Then the event log, which carries every status message the old HUD used to show.

---

## B. Component list (logical px)

| Component | Size | States |
|---|---|---|
| **MenuBar** | h 28, items 12 px padding | normal, open, project saved / unsaved (amber dot), REC |
| **ToolRail** | w 64, buttons 52×52, icon 22, label 10.5 | normal; hover; active + 3 px blue strip; warning dot (Debug) |
| **SidePanel** | w 300–640 (solver or user), header 34 | docked, floating, resizing (grip 6 px) |
| **PropertyRow** | h 28, label 38%, value, 20 px reset slot, 14 px key diamond | hover, modified, keyed (gold diamond), warning, editing, disabled |
| **Field / FillSlider** | h 24, radius 3 | normal, hover, editing, disabled, warning |
| **SpeedSlider** | w 150, log scale, numeric box | normal, < 1× amber, snapping (4 px), double-click reset |
| **KelvinBar** | h 8 gradient 1900–10000 K under the colour swatch | normal, dragging |
| **TreeRow** | h 28, indent 16, icon 16, status dot 7 | hover (eye/lock appear), selected, hidden, locked |
| **TreeGroup** | h 26, caps 11, count | open, closed |
| **CameraList / LightList** | row 30 | selected, live (red), solo (S), muted |
| **TextButton / IconButton** | h 28 / 30 (24 small) | normal, hover, primary, live, disabled |
| **EventLog** | line 20, mono 12 | past, future 45% opacity, auto-scroll |
| **KeyList** | row 30, diamond 10, interpolation column | normal, selected (gold), warning |
| **SequencerToolbar** | h 40, play 36×30 | play/pause swap; Record → Stop F6 |
| **TrackGroupRow** | h 28, chevron, icon, count, eye | open, closed, hidden (via Layout menu) |
| **TrackRow** | h 28, tree w 250 | hover controls, selected, curve row (Speed) |
| **CutBlock** | track height − 6, camera colour | normal, selected, dragging edge |
| **Navigator** | h 16 | as v3 |
| **Camera / light gizmo** | arrows 78, handles r 7 | Move / Rotate / Aim, Local / World |
| **LookAtReticle** | ring 22 + dashed line | shown when a camera has a target |
| **LoadCard** | w 440, step rows 30 | per step: pending, active, done, failed |
| **Toast** | h 34, top-centre | info, warning, busy, error |
| **RecordingPill / F4 hint** | h 34 / h 26 | pill only while recording; hint while hidden; both gone with Shift+F4 |
| **StatusBadge** | h 20 | one per `PlaybackState`; all loading states read LOADING |

---

## C. Design tokens

Colour tokens are unchanged from v1 (`TheaterTheme.h` `Color::`). Additions in v4: camera colours for cut blocks (teal, gold, grey) and the Kelvin gradient.

- **Fonts** (@1440p): Body 14, PanelTitle 12 (+0.10em), Meta 12, RailLabel 10.5 (Inter Medium), Mono 12.5, MonoSmall 11, Timecode 18.
- **Icons**: Material Symbols Rounded (filled), merged into Inter at text size + 4 px with a 3 px downward offset. New in v4: lightbulb (Lights), highlight (spot), point light, pets (mount), door_front, eject, auto_awesome (FX), content_cut (cuts), link (bone attach), save, map, bolt. Exact glyph names are in `IconSet`.
- **Surfaces**: panels `PanelBgSolid`; menu bar `PanelBgSolid` with a 1 px bottom border; `AppBg` is the pillarbox.
- **Radius**: 3 px on controls; 0 on docked panels; 6 px on floating panels and the load card.

---

## D. Layout and resolution support

All of it is in `TheaterLayout.h`:

1. **UiScale** = clamp(min(H/1440, W/2560), 0.8, 2.0) × user (0.75–1.5).
2. The **menu bar** (28 × UiScale) is taken off the top first.
3. **Game picture** = the largest rect at `gameAspect` right of the rail and panel, above a sequencer of at least 300 px (7 track rows).
4. Spare **height** grows the sequencer (capped at 50%).
5. Spare **width** widens the panel to 440, then opens the Selection column (280–360), then widens the panel further up to 640. Only what is left becomes pillarbox.
6. User overrides (dragged panel width, sequencer height, floating panels, hidden panels) come from `PanelLayout` and are applied on top. Hidden rail/timeline/menu bar give their space to the picture.

Verified outputs (compiled and run from the header):

| Display | Picture | UiScale | Game picture | Panel | Selection | Sequencer | Pillar |
|---|---|---|---|---|---|---|---|
| 2560×1440 | 16:9 | 1.00 | 1977×1112 | 519 | – | 300 | 0 |
| 1920×1080 | 16:9 | 0.80 | 1454×818 | 415 | – | 240 | 0 |
| 3840×2160 | 16:9 | 1.50 | 2965×1668 | 779 | – | 450 | 0 |
| 2560×1600 | 16:10 | 1.00 | 2035×1272 | 461 | – | 300 | 0 |
| 2560×1100 | 21:9 | 0.80 | 1950×838 | 512 | – | 240 | 23 |
| 2560×1100 | 16:9 (vanilla) | 0.80 | 1490×838 | 512 | 288 | 240 | 110 |
| 3440×1440 | 21:9 | 1.00 | 2656×1112 | 640 | – | 300 | 40 |
| 3440×1440 | 16:9 (vanilla) | 1.00 | 1977×1112 | 640 | 360 | 300 | 200 |
| 1920×810 | 21:9 | 0.80 | 1299×548 | 512 | – | 240 | 29 |
| 5120×1440 | 16:9 (vanilla) | 1.00 | 1977×1112 | 640 | 360 | 300 | 1040 |
| 1280×720 | 16:9 | 0.80 | 814×458 | 415 | – | 240 | 0 |

Ultrawide pillarbox is now 23–40 px instead of 41–59, because the panel takes up to 640. The 5120×1440 vanilla row still has large bars: that is the game's own 16:9 limit.

Recompute the layout when the swap chain resizes, the game aspect changes, a panel opens, closes, is resized or floated, or the user drags the sequencer edge. Rebuild fonts only when UiScale changes. On load, run `ClampToDisplay()` on every floating panel from `layout.ini`, so a layout saved on a bigger monitor never puts a panel off-screen.

---

## E. Interaction rules

### E.1 F4 (Hide UI)

| Key | Action |
|---|---|
| **F4** | Toggle `UiVisibility::Shown ↔ Hidden`. Ignored while Alt is held (Alt+F4 still closes the game) or while a text field has focus. |
| **Shift+F4** | Toggle `HiddenClean`: also hides the F4 hint and the recording pill. |
| Esc (while hidden) | Show UI. |

While hidden, mouse and keyboard go to the game. **Space goes to the game.** Hotkeys that still work: F4, Shift+F4, F5 record, F6 stop. Toasts are suppressed; errors show as a red pill.

While the UI is shown, game input is blocked and the cursor is visible.

### E.2 Shortcuts

| Key | Action |
|---|---|
| Space | Play / pause (UI shown) |
| F5 / F6 | Start / stop recording |
| ← / → (Shift ×10) | Frame step |
| Home / End | Jump to start / end |
| [ / ] | Speed down / up through the snap marks; double-click the slider for 1× |
| K | Key: camera key in Camera, light key in Lights, look key in Look |
| C | Add a camera cut at the playhead (Camera tool) |
| W / E / A | Gizmo move / rotate / aim (Camera and Lights) |
| V / U | Look through / set key from view |
| I / O | Set in / out |
| M | Bookmark |
| Ctrl+S / Ctrl+Shift+S | Save project / save as |
| Ctrl+O | Open replay |
| Ctrl+Z / Ctrl+Y | Undo / redo |
| Ctrl+E | Export |
| 1–6 | Select rail tool |
| F9 | Debug |
| Esc | Cancel loading or export |

### E.3 Panels and space

- Clicking the active rail tool closes the panel and the picture grows.
- Drag a panel's edge to resize, or its header to float it. A floating panel snaps back when dropped on its dock edge.
- Layout menu: show/hide Menu Bar (when hidden, holding Alt shows it again), Rail, Side Panel, Timeline, Selection Column, and every track category. **Reset Layout** restores the defaults.
- The Selection column appears only when there is spare width.

### E.4 Selection, values and timeline

As v3 (click to seek, drag to scrub, frame snap, box select, drag keys, Ctrl+wheel zoom, marker tooltips, navigator, in/out handles). v4 additions: groups collapse with a click on the chevron; Ctrl+click on a chevron collapses all; the filter field narrows rows by name; every keyable property row has a diamond that adds or removes a key at the playhead.

### E.5 Playback states

`Idle → OpeningReplay → Warping → WaitingForMap → PlacingPlayer → ApplyingEquipment → PreparingActors → LoadingProject → ReadyPaused ⇄ Playing / Paused / Seeking → Exporting → Stopping → RestoringGameplay`, plus `Error` from any step. All loading states show the LOADING badge and the load card. Errors are sticky in the event log and as a toast.

---

## F. Dear ImGui / DX12 implementation mapping

### F.1 Scaling the game into the middle (the one new piece of rendering)

Per frame, in the present hook, only when `visibility == Shown`:

1. `CopyResource(backBuffer → gameCopy)`. `gameCopy` is created once per swap-chain size with the **same format** as the back buffer (R8G8B8A8, R10G10B10A2 or R16G16B16A16_FLOAT for HDR), plus an SRV in the ImGui descriptor heap. Transition barriers: `PRESENT → COPY_SOURCE` and `COMMON → COPY_DEST`, then back.
2. Clear the back buffer RTV to `Color::AppBg`.
3. Draw the UI. `DrawGameView` adds `ImDrawList::AddImage(gameCopySrv, rects.gameMin, rects.gameMax, uvMin, uvMax)` first on the background draw list. `uvMin/uvMax` crop the game's own letterbox when `contentAspect` differs from the back buffer.
4. ImGui renders on top as usual.

What this costs (the trade-off against v1's full-screen-behind-panels):

| Cost | Size |
|---|---|
| GPU copy | One full-resolution copy per frame: 14.7 MB at 1440p, 33 MB at 4K. Typically 0.05–0.2 ms on current GPUs. |
| Memory | One extra back-buffer-sized texture (14.7 MB at 1440p, 33 MB at 4K, ×2 for FP16 HDR). |
| Sharpness | The picture is downscaled to about 84% with bilinear filtering, so it is slightly softer. Use a linear sampler; no mipmaps needed. |
| Game render cost | Unchanged. The game still renders at full resolution. Capture with F4 for full-quality output. |
| Input | Mouse picking has to map picture space back to the back buffer (`GameViewToBackBuffer` in `TheaterLayout.h`). |
| Game HUD | Scaled along with the picture (correct, nothing to do). |
| HDR | The copy keeps the back-buffer format. Draw the ImGui layer in the same colour space as the game (scRGB or PQ), or convert UI colours in the pixel shader. |

When hidden (F4), steps 1–3 are skipped entirely, so F4 is free.

### F.2 Regions

Docked regions are windows positioned from `LayoutRects` each frame (`NoTitleBar | NoMove | NoResize | NoSavedSettings | NoBringToFrontOnFocus`). A floating panel is the same window without `NoMove | NoResize`, with its rect from `PanelLayout` passed through `ClampToDisplay()` every frame.

| Component | Mapping |
|---|---|
| `DrawMenuBar` | `BeginRegion(menu)` + `BeginMenuBar`-style row drawn by hand: `Selectable` per menu opening a `BeginPopup`. Layout menu = `MenuItem(name, nullptr, &visible)` per panel and per `TrackCategory`, then `Separator` and Reset Layout. Right side: project name, Record/REC, F4 hint. |
| `DrawToolRail` | per tool: `InvisibleButton(52×52)` + icon (22) + label; active strip; warning dot. |
| `DrawSidePanel` | header → tool's fixed block → `DrawEventLog(GetContentRegionAvail().y)`. Resize grip: a 6 px `InvisibleButton` on the right edge; while active, `panelWidth += io.MouseDelta.x / uiScale`, clamped 300–640. |
| Property rows | `BeginTable("props", 4)`: label 0.38, value, reset 20, key diamond 14. The diamond pushes `Cmd::AddKey{property}` or removes it. |
| `DrawSpeedSlider` | `InvisibleButton(150×24)`; value ↔ x through `Speed::ToSlider/FromSlider` (log10, 0.01–4). Snap when within `Speed::SnapPx` of a mark. `IsMouseDoubleClicked` → 1×. Numeric box: `InputFloat` with `%.2f×`. Pushes `Cmd::SetSpeed`. |
| Kelvin bar | `AddRectFilledMultiColor` in 6 segments over 1900–10000 K; the handle sets `LightViewModel::kelvin`, and the swatch shows the resulting RGB. |
| `DrawGameView` | Background: `AddImage(gameCopy)`. Then overlays clipped to the game rect: paths, keys, look-at reticle, light cones (`AddPolyline` of the projected cone circle + 4 edge lines), chips, toasts. |
| `DrawLoadCard` | Foreground list over the game rect: `AddRectFilled(game, black 55%)`, then a centred child (440 px) with one row per `ReplayLoadViewModel::steps[i]`: state icon (spinner = rotating arc via `PathArcTo`), label, detail. Cancel / Retry Placement buttons push `Cmd::CancelLoad` / `Cmd::RetryPlacement`. |
| `DrawSequencer` | Toolbar (`IconButton`s, timecode, `DrawSpeedSlider`, Record/Stop, Key). Tree: rows built from `TrackViewModel` sorted by `category`, skipping `!trackVisible[category]` and empty categories; `TreeNodeEx` per group with a count. Lanes: same ImDrawList code as v3; cut blocks are `AddRectFilled` per `CameraCutViewModel`; the Speed row draws its curve with `AddPolyline`. Vertical scroll: `BeginChild` with `ImGuiListClipper` when rows exceed `TrackRowsThatFit`. |
| `DrawEventLog`, `DrawSelectionColumn`, `DrawTimelineNavigator`, `DrawRecordingPill` | As v3. The F4 hint is drawn by `DrawRecordingPill` too, bottom-left, whenever `visibility == Hidden`. |
| `HandleHotkeys` | ImGui keys when shown. While hidden, read F4/F5/F6 from the raw-input hook and **do not consume Space**. |

Layout persistence: `PanelLayout` is written to `%LOCALAPPDATA%\EldenRingTheaterMode\layout.ini` (a small key=value file, not ImGui's own ini, which stays disabled with `io.IniFilename = nullptr`). Write on change, debounced 1 s.

### F.3 Camera key gizmo

Use **ImGuizmo** (MIT, a single .cpp next to ImGui). Inside `DrawCameraKeyGizmo`, only in Camera tool with a dolly key selected:

1. `ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList())` and `SetRect(game.x, game.y, game.w, game.h)`. The gizmo is then drawn and hit-tested on the **scaled** game picture, so it works in the middle of the editor at any resolution.
2. Build the key's matrix from `CameraKeyViewModel::position/rotation`. Call `Manipulate(vm.gameFrame.view, vm.gameFrame.proj, op, mode, matrix, nullptr, snap)`, where `op` = TRANSLATE / ROTATE from `GizmoState::mode` and `mode` = LOCAL / WORLD.
3. While `IsUsing()`, decompose and push `Cmd::SetCameraKeyTransform{key, pos, rot, dragging=true}` each frame. The runtime merges a drag into one undo step and rebuilds the spline. Push the final command with `dragging=false` on release.
4. **Aim** is not part of ImGuizmo. Draw the look-at dot with `AddCircle`. On drag, raycast the mouse into the world (inverse of `viewProj`, mapped with `GameViewToBackBuffer`) to get a point, then set yaw and pitch so the key looks at it.
5. Wedges and the selected frustum are `AddTriangle`/`AddLine` from the key position projected through `viewProj`, then `BackBufferToGameView`.
6. `HandleHotkeys` takes W/E/A/V/U only when the Camera tool is active and `!io.WantTextInput`. Free-camera flying (WASD) works only while the right mouse button is held, as in Unreal, so W/E/A never clash with it.

The runtime needs `view` and `proj` separately (added to `GameFrameViewModel`). It already has `viewProj`, so this is splitting one matrix it already captures from the camera hook.

### F.4 Export pipeline (ffmpeg + NVENC)

Offline render, not screen recording, so the output is smooth at any fps even if the game can't run that fast live:

1. **Step:** for each output frame, seek the replay to `in + i / fps`. With motion blur, render N sub-frames at `i/fps + k/(fps·N)` and average them in a compute pass or on the CPU. The runtime has to support fixed-step seeking; if it can only play in real time, fall back to real-time capture with frame duplication (marked "live capture" in the job).
2. **Capture:** in the present hook, before the UI composite, `CopyResource` the back buffer into a ring of 3 readback buffers (`D3D12_HEAP_TYPE_READBACK`). Wait on a fence, then `Map` the oldest one. This is the same point where the game copy is taken for the scaled view, so the UI never shows up in the file and the editor can stay visible.
3. **Encode:** spawn `ffmpeg.exe` with `CreateProcess` and pipe raw frames to its stdin. Use a worker thread so the game thread never blocks on the pipe.
   - Video: `ffmpeg -f rawvideo -pix_fmt bgra -s 2560x1440 -r 60 -i - [-i audio.wav] -vf scale=W:H:flags=lanczos -c:v hevc_nvenc -preset p6 -rc vbr -cq 18 -b:v 0 -pix_fmt yuv420p -c:a aac out.mp4`. Swap `hevc_nvenc` for `h264_nvenc`, `av1_nvenc`, `libx264` or `prores_ks`.
   - Image sequence: same input, `-c:v png` (or `-pix_fmt rgb48le` for 16-bit, `mjpeg -q:v 2`, `exr`), output `name_%05d.png`.
4. **Resolution:** below native is a Lanczos downscale in ffmpeg. Above native is an upscale, so for real 4K the game must render at 4K (set the game resolution or use DSR; the panel says so when you pick above native).
5. **Audio:** WASAPI loopback of the game process while the range plays in real time once, written to a temp WAV and muxed by ffmpeg. Off when the step is not real time and audio is unchecked.
6. **Progress:** the worker publishes frame, fps, ETA and bytes into `ExportJobViewModel`. ffmpeg's stderr is parsed only for errors.
7. **Caps:** at startup run `ffmpeg -hide_banner -encoders` once and check for `hevc_nvenc` / `av1_nvenc` to fill `EncoderCapsViewModel`. A tiny test encode confirms the GPU actually supports it.

Licensing: NVENC works in an LGPL ffmpeg build, which is fine to ship next to the mod. `libx264` needs a GPL build. Either ship the LGPL build and use NVENC, ProRes or PNG, or let the user point to their own ffmpeg.exe (Settings → Export).

v4 note for F.4: the export step evaluates the Camera Cuts track, camera keys, light keys, look keys and speed keys at each output time, so the file matches what the timeline shows. The speed track changes how fast replay time advances per output frame, not the output frame rate.

### F.5 Camera objects and cuts

- **Path evaluation** (runtime, not UI): centripetal Catmull-Rom (α = 0.5) through the keys, or cubic Bezier with per-key handles. Constant speed: build an arc-length table (e.g. 256 samples per segment) and map time → distance → parameter. Rotation is SLERP between keys, or a look-at target overrides yaw and pitch.
- **Look-at**: the runtime resolves the target's world position each frame (actor root or bone from the bone replay) and builds the rotation. The UI only draws the reticle from `CameraObjectViewModel::lookAtWorld` projected to the picture.
- **Bone-attached**: position = bone world transform × local offset. Bone names come from the skeleton list the bone replay already has (150 player bones; NPC skeletons later).
- **Cuts**: `CameraCutViewModel { cameraId, start, end }`. The UI edits them like clips (drag the edges, drag to move, Delete). Overlaps are not allowed; dropping one cut on another trims the older one.
- Bezier handles are drawn on the picture for the selected key only (two small circles + lines), dragged with the same raycast as Aim.

### F.6 Lights

- Light objects are added by the engine side (a light hook is required; the UI does not assume a mechanism). The UI sends `Cmd::CreateLight`, `Cmd::SetLight`, `Cmd::AddLightKey` and reads `LightViewModel`.
- Gizmo: same ImGuizmo call as camera keys, with the light's matrix. Spot lights also get two handles on the cone rim (inner/outer angle).
- Kelvin → RGB: the Tanner Helland approximation is enough for a UI swatch; the engine side decides what colour actually reaches the light.
- `LightCapsViewModel` reports what the engine supports (max shadowed lights, volumetric yes/no). Unsupported fields are disabled with a tooltip, never hidden, so the panel layout stays stable.

### F.7 Replay loading

The runtime owns the sequence and publishes `ReplayLoadViewModel` (current `PlaybackState`, six steps with state and detail, placement distance and attempt). The UI only draws it. Order: open file → warp → wait until the map tile and collision report loaded → place the player and verify the distance (≤ 0.5 m, up to 3 attempts) → apply equipment → prepare actors → load the project if asked → `ReadyPaused`. Cancel at any step goes to `RestoringGameplay`, which unlocks input and returns control.

### F.8 Project file

`.thproj` (JSON, UTF-8) sits next to the replay with the same base name. It stores: replay path and hash, cameras (type, path, keys, look-at, attach), cuts, lights and keys, look keys, speed keys, bookmarks, in/out, and the export preset. It never stores replay data. Opening a replay whose hash does not match its project shows an amber warning in the load card and opens the project anyway (keys stay where they were in time). `ProjectViewModel` gives the name and dirty flag to the menu bar.

---

## G. C++ architecture

Unchanged in principle: the game thread builds an immutable `TheaterViewModel`; the UI reads it and emits `TheaterCommand`s; the game thread drains and validates them. v4 additions:

- `BoneReplayDebugViewModel` replaces the native replay debug model; `BackendKind::BoneReplay` is the only backend.
- `ReplayLoadViewModel`, `EquipmentViewModel`, `CameraObjectViewModel`, `CameraCutViewModel`, `LightViewModel`, `LightCapsViewModel`, `ProjectViewModel` (all in `TheaterUITypes.h`).
- `TrackViewModel::category` drives grouping; `PanelLayout::trackVisible` drives the Layout menu.
- New commands: TogglePlayPause, OpenReplay{path, loadProject}, CancelLoad, RetryPlacement, SetTrackCategoryVisible, Create/Duplicate/DeleteCamera, SetCameraObject, SetCameraCut, SetKeyInterp, Create/Duplicate/DeleteLight, SetLight, AddLightKey, SaveProject, LoadProject.
- `GameCompositor` (frame copy) and `ExportService` (F.4) as in v3. A `ProjectService` owns load/save of `.thproj` on the game thread.

```cpp
void OnPresent(IDXGISwapChain3* sc)
{
    auto& io = ImGui::GetIO();
    const TheaterViewModel& vm = g_bridge.Latest();
    HandleHotkeys(vm, g_ui, g_cmds);

    g_ui.rects = SolveLayout(io.DisplaySize, vm.gameFrame.contentAspect, g_ui.layout, g_ui.visibility);
    const bool composite = PrepareGameComposite(g_ui);       // copy + clear only when Shown

    ImGui_ImplDX12_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
    if (composite) DrawMain(vm, g_ui, g_cmds);                // menu, rail, panel, selection, game view, load card, sequencer, toasts
    DrawRecordingPill(vm, g_ui);                              // REC pill + F4 hint while hidden
    ImGui::Render();
    RenderDrawData(sc);
    g_bridge.Submit(std::move(g_cmds));
}
```

File layout: `TheaterTheme.h`, `TheaterLayout.h`, `TheaterUITypes.h` (provided), plus `TheaterUI.cpp` (DrawMain, hotkeys, layout.ini), `MenuBar.cpp`, `Rail.cpp`, `SidePanel.cpp` (one function per tool), `LoadCard.cpp`, `EventLog.cpp`, `Sequencer.cpp` (+ `SpeedSlider.cpp`), `GameView.cpp` + `GameCompositor.cpp`, `RecordingPill.cpp`, `TheaterBridge.cpp`.

---

## H. Implementation phases (aligned with the engine phases)

**UI 1, with engine phase 1 (cleanup, playback quality)**
- Menu bar with File and Layout, resizable/floating panels, `ClampToDisplay`, layout.ini.
- Transport: Space, eject, speed slider, F5/F6, badge states. Bone replay Debug panel. Status messages into the event log.
- Done when: a recorded replay plays, pauses, scrubs both ways and changes speed 0.01–4× from the toolbar, at 1440p, 1080p and 2560×1100.

**UI 2, with engine phase 2 (whole-world replay)**
- Load card and the loading states; equipment rows and track; Scene groups; track categories with hide/collapse.
- Categories appear only as the engine records them (Enemies & Bosses first, then Mounts, World, Props, FX, Events).

**UI 3, with engine phase 3 (cameras)**
- Camera list, Dolly/Orbit/Bone types, Catmull-Rom/Bezier, constant speed, look-at reticle, Camera Cuts, per-key interpolation. Key gizmo (F.3).
- Speed keys (ramps) once the engine's clock supports them.

**UI 4, with engine phase 4 (lights)**
- Lights panel, cone/radius overlays, light gizmo, Kelvin bar, light keys, caps handling.

**UI 5, with engine phase 5 (Look)**
- Look panel with override, camera effects, visibility, High Quality LODs, presets, Compare.

**UI 6, with engine phase 6 (ReShade) and the project file**
- `.thproj` save/load, dirty flag, Load project prompt. ReShade preset picker in Look if the engine phase adds it. Export through cuts (F.4).

---

## I. Hard in Dear ImGui / DX12, with simpler equivalents

| Hard thing | Simpler equivalent used |
|---|---|
| Free docking with saved layouts | Not using ImGui docking. Panels dock into solver rects or float; `PanelLayout` + `layout.ini` saves both; `ClampToDisplay` keeps them reachable. |
| A log-scale slider | `InvisibleButton` + `Speed::ToSlider/FromSlider`; ImGui's `SliderFloat` with `ImGuiSliderFlags_Logarithmic` also works but cannot snap to marks. |
| A spinner in the load card | `PathArcTo` with a time-based start angle; no texture. |
| Grouped, collapsible timeline rows with thousands of markers | Group rows by category, clip rows with `ImGuiListClipper`, bin markers per pixel column (as the navigator does). |
| Drawing light cones in the picture | Project 24 points of the cone's end circle through `viewProj`, `AddPolyline` + 4 lines to the apex. No 3D rendering in the overlay. |
| Rendering the game at a smaller viewport size | Not possible from an overlay without hooking the engine's camera and render targets. **Copy and scale the finished frame** instead (F.1). |
| Smoothly animating panels in and out | Panels appear instantly. Only the game picture's rect is lerped over 120 ms, a single `AddImage` with interpolated corners. |
| Panels that size to their content with nothing below | Every panel ends with a flexible event log (`GetContentRegionAvail().y`), so there is never a blank region to manage. |
| Icon fonts with glyphs above U+FFFF | Use the BMP subset of Material Symbols (every icon in `IconSet` is in it), or build with `IMGUI_USE_WCHAR32`. |
| Material Symbols baseline sitting high next to Inter | `ImFontConfig::GlyphOffset.y = 3 px × UiScale` when merging. |
| Letter-spacing on uppercase titles | Bake PanelTitle with `GlyphExtraSpacing.x` (≤1.91) / `GlyphExtraAdvanceX` (1.92+). |
| Dashed guides, blur, shadows | Not used: guides are solid 10% white lines, and panels are solid now. |
| Text ellipsis | `RenderTextEllipsis` (imgui_internal) for names in the tree and log. |
| Hotkeys while ImGui has no focus (UI hidden) | Read them in the existing Win32 WndProc / raw-input hook and forward to `HandleHotkeys`. |
| HDR back buffers | Keep the copy in the back-buffer format; convert UI colours to the game's colour space in the ImGui pixel shader. |
| Exporting at a fixed fps when the game runs slower | Step the replay per frame and capture from the back buffer (F.4). Fall back to real-time capture only if the runtime cannot step. |
| Video encoding inside the DLL | Pipe raw frames to an external `ffmpeg.exe`. No codec code or licensing in the mod itself. |
| A 3D gizmo in an overlay | ImGuizmo, given the game's view/proj and the scaled game rect (F.3). No engine-side gizmo needed. |

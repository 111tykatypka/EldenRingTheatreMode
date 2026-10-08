# C22 — Light viewport handles and compact color wheel

Branch: codex/custom-lights-prototype. Base: 9f664d8 (C21).

## Changes

- RGB hue wheel capped at 170 logical UI pixels, respecting the available panel width and existing DPI scale.
- Point-light sunburst icons and spot-light icons with oriented wire cones, labels and selection highlight.
- Light markers use the existing dolly viewport projection, clipping, resize remapping and world-axis conventions. They work without a replay or any dolly keys.
- Select a light by its icon or the Lights list. XYZ arrows move its definition; colored rotation rings update its quaternion. Middle-click in the F4 game picture switches Move/Rotate, as with camera keys.
- Drag updates are capped at 30 Hz with a final release update. Escape restores the gesture's original transform; unrelated light properties are retained.
- Light and camera selection are distinct. Clicking a dolly marker/timeline key returns to camera editing. Delete inside F4 removes the selected light when light editing owns selection, otherwise the selected camera keys.
- Show light handles is a persisted checkbox, independent of Show camera handles. Light icons/axes remain visible outside F4; mouse editing follows existing dolly behavior and is available inside F4. Clean preview hides both, and lost game focus stops gestures.

## Day/night preserved

The user reports the C21 day/night slider works perfectly. C22 changes neither its UI block nor native request implementation. `adapter/src/lighting_time.rs` remains SHA-256 `75FBB531A065A770FBCB9B23452EC550796BC386A1EE287CFA97077AA48CE3D6`. The user report verifies the tested day/night behavior; it is not evidence for custom light spawning or every region's sunlight behavior.

## Native-light status

Viewport icons are editor handles for saved light definitions, not engine-created lights or illumination. C21 native spawning/shadow limitations remain unchanged. No engine lighting memory writes or new hooks are introduced by this UI change.

## Build / validation

Release AMD64 EXE/native library/DLL compile. No unit tests were requested/run. New icon placement, mouse interaction and wheel layout are IMPLEMENTED — RUNTIME / VISUAL VALIDATION REQUIRED. Source whitespace audit passed; no live Elden Ring test performed here.

## Package and controls

Close Elden Ring and the previous Theater host before replacing a loaded DLL. Start `outputs/Cinematic-C22-light-handles/EldenRingTheaterMode.exe` with the established offline YAFSML workflow; explicit loader paths must use this package's TheaterMode.dll.

F4 -> Lights -> Create Point/Spot. Definitions start at the current camera position, so move the free camera backward/aside and look toward that position to see their markers. Click an icon, drag the red/green/blue axis, middle-click the game picture for rotation, drag a ring, or press Escape to cancel. The Lights panel reflects the edited transform; Save light setup persists it. Close F4 to see handles without the menu, or uncheck Show light handles to hide them.

Logs on failure: `%TEMP%/TheaterModeGame.log`, `%TEMP%/TheaterModeRender.log`; host `%LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log`.

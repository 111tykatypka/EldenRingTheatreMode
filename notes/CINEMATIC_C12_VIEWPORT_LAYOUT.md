# C12a — viewport transforms and compact sequencer

Status: COMPILE_VERIFIED; standalone DX12 smoke PASS. Elden Ring visual alignment,
mouse picking and in-game GPU overhead are not runtime/visually verified.

## Package

Final package: outputs/Cinematic-C12a-viewport-layout. C12 is an intermediate;
C11a and earlier packages are preserved. Host and DLL must come from the same
package. No actor/game timing/signature/replay format changes were made.

## Rotation transforms

Game viewport has Move XYZ / Rotate XYZ selectors. Select a Dolly marker and
choose Rotate XYZ to drag the red X, green Y or blue Z ring. Rotation uses
world-axis quaternion composition; the existing inspector also provides numeric
pitch/yaw/roll editing. Escape cancels a drag. Translation/rotation share selection
with timeline and curve keys. Handles/picking are remapped into the scaled game
picture while F4 is shown and return to full-screen coordinates when hidden.
Passive markers outside F4 and the P visibility shortcut are preserved.

## Compact tracks

Compact mode shows shorter Replay/Camera rows and hides empty Actors/Bookmarks
headings. Actor child rows are collapsed by default. The +/- beside Tracks toggles
compact mode. Right-click sequencer for Compact tracks, Expand actor tracks and
Dolly curve editor visibility. These preferences persist. Scrubbing, transport,
camera diamonds, bookmarks and curve edits remain available; hiding a row does
not remove its underlying data. The sequencer has a lower minimum height.

## Game-view compositing

Only when F4 is shown: copy the current swap-chain back buffer into a same-size
GPU texture before drawing editor UI, sample it through ImGui into Game viewport,
and cover the full-screen background with the editor backdrop. No CPU readback,
video recording, game resolution change or game camera patch is used.

One texture and SRV per swap-chain slot. Textures allocate lazily on first use and
reuse the existing slot fence; no per-frame texture allocation or GPU wait added.
Transitions: back buffer PRESENT -> COPY_SOURCE -> PRESENT; copied texture
PIXEL_SHADER_RESOURCE -> COPY_DEST -> PIXEL_SHADER_RESOURCE. Existing final
RENDER_TARGET -> PRESENT transition is preserved. Reserved descriptors cannot be
allocated by the ImGui texture allocator. Resize/shutdown use existing GPU-fence
resource teardown. Allocation failure disables retries for that slot until resource
recreation and retains the previous full-screen behavior; see TheaterModeRender.log.

The game-view image maintains the actual full back-buffer aspect (including any
game letterboxing/HUD). Normal view returns immediately when hiding F4, with no
frame copy. Copy textures remain allocated until swap-chain resource teardown.
Additional VRAM is approximately width * height * format bytes * buffer count;
for 3840x2160 RGBA8 and three slots, about 95 MiB, excluding allocator alignment.
Real game FPS impact is UNKNOWN and needs measurement.

## Layout and resizing

Game viewport is movable/resizable. Fit sequencer (default on) follows the actual
sequencer top and right boundaries. Resizing the game viewport manually turns Fit
off; enable it again to refill the available space. Either mode clamps the image
and viewport above the sequencer. Timeline movement leaves a small minimum area
for the viewport and prevents moving the timeline over that area.

The picture interpolates toward its aspect-fitted rectangle using an exponential
0.12-second response. Shrinking safety bounds clip/clamp the transition immediately
so animation never crosses into the sequencer. It does not stretch the game.
Window layout uses existing ImGui persistence; Fit/track choices use overlay settings.

## Checks

- Release x64 host/native backend/Rust DLL built.
- 14 CTest suites PASS; Rust 49 PASS, one optional real-file check ignored.
- Added CPU geometry construction using a synthetic texture ID at 1280x720,
  1920x1080 and 3840x2160: picture remains inside display and above moved sequencer.
  This fixture does not assert game pixel rendering.
- Standalone real-device DX12 smoke PASS: F4 shown/hidden, mouse button events,
  swap-chain resize. No Elden Ring process was involved in this test.

## Manual check

1. Close game/old host and start this package's EXE. Use the existing YAFSML launcher
   with this package's DLL, then enter the offline loaded world.
2. F4: game picture appears in Game viewport above the timeline. Hide F4: normal
   full-screen view returns. Repeat and check for stale/black frames and FPS drops.
3. Resize/move the timeline vertically and horizontally. Fit sequencer keeps the
   image above it, maintaining aspect. Resize Game viewport directly (Fit turns off),
   then enable Fit again. Check smooth motion, picking and no timeline overlap.
4. Load a replay/camera path. Select a Dolly marker, Rotate XYZ, drag each colored
   ring. Check position stays fixed and saved quaternion/other keyframes are correct.
   Switch to Move XYZ and check picking follows the scaled image. Escape cancels.
5. Toggle +/- Tracks, expand actor rows from the right-click menu, hide/show curves,
   scrub/play/step/bookmark and close/reopen F4. P still hides/shows Dolly markers.
6. Stop/F6, return Default camera and verify normal controls.

Send %TEMP%/TheaterModeRender.log and TheaterModeGame.log plus package manifest
for rendering/picking failures. New in-game features remain validation-required.

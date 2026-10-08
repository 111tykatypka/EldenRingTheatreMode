# C8 — mouse-look roll correction and replay-origin diagnostics

Status: COMPILE_VERIFIED / automated tests PASS. In-game behavior UNVERIFIED.

## Camera

Normal look previously post-multiplied both yaw and pitch into the camera orientation.
After pitching, yaw therefore rotated about tilted local up and banked the horizon.
`cinematic::mouse_look` now pre-multiplies world-Y yaw and post-multiplies local-X
pitch/local-Z intentional roll. It normalizes and rejects invalid inputs. Dolly and
bone-camera evaluation are unchanged. Existing deliberate roll is preserved rather
than automatically erased. This does not add pitch limits or remove recorded roll.
Regression: 5,000 alternating pitch/yaw updates from a pitched upright orientation,
right-vector vertical component < 1e-10, normalized quaternion; intentional roll and
invalid-input cases checked.

## Old replay blockage — NOT resolved

`begin_arrival` requires equal origin IDs and matching finite chunk XYZ (<0.01).
The message did not report which predicate failed. `Enemy test` research shows origin
-1 and changing chunk metadata in the same block; `Torrent test` showed a constant
chunk field during player movement. Neither proves a cross-origin conversion.
The pinned SDK describes conversion as requiring a block/Havok reference pair.
Current legacy capture does not establish that pair. No offset sign is guessed, no
origin guard is removed, and no demo is modified.

The rejected replay now stays loaded instead of being removed/marked load-failed.
The error latches and releases ownership; the initialization path returns before
setting gravity suppression/input lock. Stop then explicit Play can retry without
reopening the file. Normal restoration still uses the existing callback mechanism.
`ARRIVAL_ORIGIN_MISMATCH` logs path, source timestamp, sample index, both blocks,
origins, chunk fields, physics roots and individual origin/anchor comparison results.
The stale arrival documentation claiming chunk data was global player XYZ is corrected.
These are diagnostics/control-flow fixes, NOT compatibility or automatic-travel success.

## Validation

Release AMD64 host/DLL built. 14 CTest suites passed; 49 Rust tests passed;
one optional real-recording inspection was ignored. No game process was restarted,
no DLL hot-unloaded, and no visual success claimed. Existing packages are preserved.

## Use this isolated package

`outputs/Cinematic-C8-mouse-look-origin-diagnostics` contains the matched host and DLL.
Close Elden Ring normally and close the old host before switching builds.
Launch the C8 EXE, then launch the game from its existing YAFSML launcher button;
the generated launch configuration selects the DLL beside this EXE. If launching
YAFSML manually, select that exact C8 DLL. Do not overwrite the game installation.
Load a playable world and test free-camera mouse look with deliberate roll/shake off.
Open the old demo and press Play once. If still blocked, preserve `%TEMP%/TheaterModeGame.log`
with `ARRIVAL_ORIGIN_MISMATCH`; no new recording is needed for that diagnostic.

## Next investigation

Use the newly logged recorded/live pair to identify an authoritative native origin
conversion or independently validated reference coordinates. Account for origin
changes across recorded frames and actor roots before applying a translation. A
safe conversion must work for player physics, draw/model roots, actors and collision
proxy, not just visually move the player. Cross-map travel remains blocked without
validated location metadata.

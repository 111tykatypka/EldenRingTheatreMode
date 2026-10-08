# C18 — Deterministic cinematic camera foundation

Base: C17 (`7db09df`), branch `codex/cinematic-editor-pass`.

## Audit and integration points

The existing `shared/CinematicCamera.h` was a pure, header-only evaluator with
centripetal Catmull–Rom, SLERP and a 128-subdivision arc cache. One key timestamp
and one eased parameter drove position, rotation and FOV. `.ercam` used ERTCAM v1.
The host owns `replay::Player` and the authoritative timestamp; the overlay
receives `Snapshot.time_ns/master_clock_ns`, then derives render-time position
from that anchor. The version-guarded native camera copy hook applies the result.

The new foundation extends that path. ERPLAY, the replay worker, YAFSML, native
camera/timing addresses, game tasks and actor playback were not redesigned.

## Modules and evaluation

- `CameraMath.h`: the existing vector/quaternion and live input response helpers.
- `CameraCurves.h`: cubic geometry, centripetal Catmull–Rom/Hermite conversion,
  analytical derivatives, arc-length tables, easing and timing-Bezier inversion.
- `CameraQuaternion.h`: quaternion log/exp, shortest-path handling, SQUAD,
  camera-to-world basis and target orientation.
- `CameraTrack.h`: independent position, rotation, FOV, roll, focus and target
  channel indices; cached geometry/rotation-minimizing frames/rotation controls.
- `CameraLiveDamping.h`: tested closed-form critically damped live-rig utility.
  This is prepared for live rigs, not attached to replay Dolly evaluation.
- `CameraProject.h`: ERTCAM v2 serializer and v1 migration.
- `CinematicCamera.h`: compatibility include and the existing frame scheduler.

`Track::evaluate(uint64_t replay_nanoseconds, resolver, debug)` is pure and
allocation-free for a built track. `evaluate_seconds(double)` is the explicit
seconds API; do not pass a floating-point seconds value to the nanosecond API.
Every result depends on track data, requested time and recorded target data.
The evaluator has no accumulated delta, previous-frame pose, game pointers or
private playback clock. Cache rebuilds happen only on edits/settings changes.
Position geometry and parallel-transport caches are reused for scalar-channel
or timing-only edits when geometry and arc resolution are unchanged.

### Channels

A key's membership mask controls which channels it contributes to. Position
keys can be at 0/3/6/10 seconds while FOV keys are at 1/4 seconds. Disjoint
channels may share a timestamp. Duplicate timestamps within one channel and
duplicate IDs are rejected transactionally. Existing pose keys remain editable.
Rotation, scalar curves, position easing and global distance remapping are
separate. Roll is stored independently and composed around camera-local +Z.
K capture separates local roll from the base quaternion and unwraps against
the previous roll key. A vertical-pole capture keeps ambiguous roll in the
base quaternion; it does not invent a bank angle.
Focus is metadata only: no unsupported game focus/aperture writes were added.

### Geometry, timing and rotation

- Default position: centripetal Catmull–Rom, alpha=.5, reflected virtual endpoints.
- Linear, explicit Step, legacy Smooth/Curve, spatial Bezier remain available.
- Bezier tangent UI: Auto, Linear, Free; numeric handle editing is supported.
  Aligned/Broken enum values are reserved and rejected, not implemented modes.
- Arc tables default to 256 subdivisions per segment, adjustable through the
  validated settings API (16–8192 as a cache-resource guard). Both distance-to-u
  and u-to-distance use the cached table. Approximation is finite-resolution.
- Keyframe-time mode honors position timestamps, optionally remapping distance
  inside individual segments. Constant-speed mode traverses total path distance
  across segment boundaries. Speed 0 fits first/last position timestamps; a
  positive value is in **game world units/s**, not asserted to be meters/s.
- Time-remap mode maps normalized path time to normalized global distance using
  Linear, Ease In/Out/In-Out, Smoothstep, Smootherstep or cubic Bezier timing.
  Cubic Bezier solves its X coordinate before evaluating Y.
- Rotation offers SLERP, time-weighted multi-key SQUAD and Step, independently
  of position. Adjacent quaternion signs are aligned; incoming/outgoing SQUAD
  controls account for unequal key intervals.
- Scalar channels use Linear, smooth time-aware cubic or Step, plus independent
  easing. FOV/focus outputs are bounded to meaningful ranges.
- Look Along Path uses analytical derivatives and cached parallel-transport
  up frames. Look At supports world points, target keys and recorded targets.
- Constant/global-remap mode rejects spatial Step cuts rather than smoothing a
  deliberate discontinuity. Existing hard camera cuts remain separate.

The old optional Path filter remains stateless, off by default; it is bypassed
for constant-speed/remapped motion and target/path aim. Smooth spline motion
does not require that filter. Existing real-time shake remains a separate
render effect: disable shake when checking identical paused/export poses.

## Recorded player targets and IPC

Overlay protocol v12 adds one requested-timestamp command and a bounded window
of up to 64 **original ERPLAY player position samples**. The host reads them
through its existing reader only when requested. No second replay parser is
introduced in the DLL. The camera interpolates a bracketing pair at the exact
same timestamp used for its path. Target offset is applied afterward.

Missing/out-of-window target data uses the authored rotation and reports an
unresolved target; it never substitutes live player position or a zero vector.
Seeks can briefly wait for the next IPC window. Actor-ID targeting has a public
resolver API but no actor-target transport yet. Host and DLL must both be C18;
mixed overlay protocol versions are rejected.

## Editor workflow

1. Load a replay, choose Dolly, enable camera control, leave Preview off.
2. Pause/seek, position camera, press K; repeat at other timestamps.
3. Under Camera → Dolly motion, choose Keyframe time / Constant world speed /
   Time remap and an aim mode. Use **Add channel key** for independent keys.
4. Under Keyframe details edit rotation/scalar interpolation, roll, independent
   easing and Bezier tangents. Duplicate at cursor rejects channel-time clashes.
5. Use Play path / J to explicitly enable preview. Scrubbing evaluates directly.
6. Curves show XYZ, FOV, Roll or Focus. Existing selection, Delete, bulk
   interpolation, Alt+Z/Alt+Shift+Z and viewport gizmos remain available.
7. Save path writes v2 `.ercam`; Load migrates v1 without automatically arming.

## Validation

Final Release AMD64 host/native library build: **COMPILE_VERIFIED**.
Rust DLL relink with locked offline dependencies: **COMPILE_VERIFIED** (existing
crate-name warning only). Selected suites: **5/5 passed** on 2026-10-08:
`cinematic-channels-tests`, `cinematic-camera-tests`, `native-camera-tests`,
`replay-player-tests`, and `ingame-editor-tests`. The IPC fixture ran outside
the sandbox with explicit user authorization; it used a test pipe and generated
TEMP replay data. No Elden Ring process was launched by these tests.
Automated results and binary hashes are also recorded in the package manifest.
Tests include random evaluation order, endpoints/boundaries, unequal-time SQUAD
velocity, global constant-speed intervals, LUT round trips, timing inversion,
independent FOV/roll, targets at requested time, vertical parallel transport,
duplicate/zero/tiny/180-degree cases, v1/v2, runtime undo/settings/duplication,
IPC target samples and replay-player regressions.

**All new in-game/editor behavior is RUNTIME VALIDATION REQUIRED.** Unit tests
use generated fixtures, not evidence of successful game playback.

## Remaining limitations / next steps

- Full graph editing of timing-Bezier handles and viewport spatial-Bezier
  tangent gizmos are not implemented; controls are numeric.
- Recorded actor targets, named multi-track management, cut/blend shot editor,
  banking and native focus/aperture control remain future work.
- Reverse timestamp evaluation is tested; this does not add reverse transport
  to the host, which currently advances at positive timescale only.
- Exact requested velocity can finish a path before other channel keys. Use
  speed 0 to fit the authored interval. Arbitrary uneven position times can
  create speed changes; global arc traversal addresses that use case.
- Very sharp/degenerate target/up configurations and extreme SQUAD transitions
  need visual testing. Fixed-up Look At is not a promise of pole-free banking.
- Target IPC sampling overhead, high-timescale window coverage and actual
  rendered-camera synchronization still need measurement in game.
- Recorded-player targets currently assume the native camera and ERPLAY
  positions share the same coordinate origin. No cross-origin conversion was
  invented; validate in the recorded area first.

## In-game check (after closing previous host/game)

Launch the C18 EXE with its adjacent DLL through the existing YAFSML workflow.
Load a replay in the appropriate area. Author four keys at 0/3/6/10 seconds.
Check Play path, pause, backward/forward seeks, J, FOV keys at separate times,
SQUAD rotation, constant speed, saved-path reload and Stop/native restoration.
Then test Look At Recorded Player with offset Y≈1, including seeks and timescale
changes. Send `%TEMP%/TheaterModeGame.log` and
`%LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log` for failures.

## Mathematical references

Independent implementation from mathematical descriptions; no third-party
implementation code was copied.

- [Yuksel, Schaefer, Keyser: Catmull–Rom parameterization](https://www.cemyuksel.com/research/catmullrom_param/)
- [Microsoft: quaternion SQUAD and XYZW conventions](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmquaternionsquad)
- [Dam, Koch, Lillholm: Quaternions, Interpolation and Animation](https://web.mit.edu/2.998/www/QuaternionReport1.pdf)

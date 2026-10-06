# Phase8 — first real expanded player capture

## Evidence

User reports approximately two minutes fighting a boss, resting at a grace and using items. Automatically discovered actual file: `%LOCALAPPDATA%/EldenRingTheaterMode/replays/replay_2026-10-06_075252.erplay`.

SHA256: `6af296f656729f5463ac898d90d31f516fa149e8117c5864d9955874327acf54`.

ERPLAY03, producer `0.7.0-fidelity1`, game `2.7.0.0`. Duration **126.4597572 seconds**, **7555** player samples, **59.734418 Hz**, **13** transform chunks. File **21,030,717 bytes** (20.056 MiB), observed total file rate **166,303.63 bytes/s**. This rate includes legacy tracks, not only new player fields.

Read-only Python inspector checked header/footer, CRC, sample order, track lengths, availability masks and per-track ordering. The C++ Reader/ReplayPlayer independently opened this real file and sought to start, midpoint and end with all nine tracks present (`replay-player-tests --capture-real <file>`). This command only reads the file and never applies game transforms.

## New tracks observed

Every track has 7555 records and every declared field is available in every record. No raw float NaN/Inf found. All 242 fields were readable; **112** changed during this session. Readability is not proof of correct field interpretation.

| Track | Named fields | Changed records |
|---|---:|---:|
| PhysicsTrack | 62 | 7542 |
| AnimationTrack | 42 | 7542 |
| ActionTrackRaw | 39 | 3559 |
| LocomotionBehaviorTrack | 23 | 6981 |
| EquipmentTrack | 8 | 4 |
| AppearanceTrack | 3 | 0 |
| GameplayStateTrack | 25 | 3133 |
| EffectSignalsTrack | 4 | 4 |
| CombatStateTrack | 36 | 5295 |

Animation queue IDs and phase/time vary in all 10 public entries; each slot contains 50–58 distinct IDs. Queue indices cycle 0–9. Behavior root motion changes 6712 times. Current native action-request bits change 249 times. These are native observations, not an established attack/roll semantic mapping.

Item-use timer changes near **22.54s** and **106.45s**. Item SFX fields change near **22.54s**, **106.61s**, **109.51s**. These support item-use correlation; exact item identity and effect lifetime remain unverified. HP changes at **69.96s, 104.65s, 110.37s, 121.31s, 124.05s**, range **0–522**. FP range **38–78**; stamina raw signed value range **-16–97**. Negative stamina is retained honestly; its interpretation needs validation, not clamping. Lock flag changes near **36.89s** and **124.10s**. Arm style changes four times; equipment IDs and slot indices remain constant. Appearance stays constant, consistent with an unchanged character, but its full semantic layout is not verified.

No exact grace-rest interval or boss identity is proven by raw changes alone. User action timestamps/video observation are needed before labeling those events. Locomotion orientation matrix values range outside unit rotation bounds: do not treat this raw SDK field as a validated pure rotation matrix.

## Timing / losses / performance

Player FIFO drop counter: **0**; recorded player sequence gaps: **0**. Maximum source sample interval **70.0702ms**; therefore 60 Hz is an average, not a guarantee of every frame. All nine tracks share source timestamps/sequences.

Separate pre-existing character capture reports cumulative **313 character drops** in host logs. This is a different queue and is not claimed lossless; its per-session loss delta is not established here. Do not present zero player drops as zero losses across the whole replay.

Game log reports capture reads around **116–124 microseconds per callback** during ordinary player availability. This measures the field-read portion, not complete recording overhead or game FPS impact. Game later reports PLAYER_LOST then PLAYER_FOUND; this is not demonstrated to fall inside the recorded interval, whose samples have no sequence gaps. No claim of captured transition lifecycle completeness.

## Reader compatibility hotfix

Host logs after finalization contain `EDITOR_ERROR=incompatible replay mod version`. Exact cause: ReplayPlayer allowed only `0.2.0`, `0.3.0`, `0.6.0`, while the new recorder writes `0.7.0-fidelity1`. Added that exact producer version to the whitelist. Unknown producers remain rejected. Added synthetic accepted/rejected producer tests plus the actual-file read-only validation CLI. No format, game guard, DLL, playback writes or animation logic changed.

New host package: `Phase8_PlayerCapture_Fidelity1_Hotfix1`. Close the old host before launching the new EXE. DLL behavior is unchanged; no game restart is needed solely for this host opening fix. Open the existing file, inspect Player capture tracks and scrub. This verifies data viewing; replaying animations remains NOT IMPLEMENTED. Do not use this capture validation as authorization for uncontrolled gameplay writes.

## Remaining scope

Raw field availability and genuine persisted multi-track capture are established for this session. Full skeletal pose, behavior blend weights, full HKS VM, dynamic action queue and complete SpEffect inventory remain unresolved. Most semantic labels remain REFERENCE pending controlled correlation. This capture does not yet reproduce the boss fight, damage, items, grace state or animation in-game.

Detailed evidence: PHASE8_REAL_CAPTURE_INSPECTION.json (state near 50s) and PHASE8_REAL_CAPTURE_AUDIT.json (all-field change/availability/range summary). Original replay and original logs are left unchanged.

# Enemy test — actual capture and playback diagnostics

Inspection date: 2026-10-07. Read-only analysis of the user's recording and existing logs. No game writes, deployment, or changes to the recording.

## Files and validation

Directory: `C:\Users\user\AppData\Local\EldenRingTheaterMode\replays`.

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| Enemy test.erplay | 13,271,187 | C2F9542B0DD09CE8F924E1AE8DDF7B50B8F19A221D9BC534C1D993D65B60944B |
| Enemy test.erplay.world | 135,348,026 | 7C34B2777B0636B96EF5689BA9716D6A7BED48906454D0711A2BC8A02AC6C733 |

[CONFIRMED] Combined size: 141.73 MiB. Main ERPLAY format is **v3**; companion ERWORLD format is **v2**.

[CONFIRMED] The actual C++ ERPLAY reader/validator passed on this file: 4,728 samples, duration 80.6242341 seconds, measured 58.63 Hz, nine chunks, 139 character descriptors and 22,834 character samples. Source drop count is zero. These character samples are a separate stream from full skeleton poses in the world file.

[CONFIRMED] The Rust saved-world validator passed chunk integrity, decoding, ordering and identity references. World file has 4,116 chunks; capture log reports 4,728 player pose frames, 51 actors with 124,137 actor pose frames, and six event-flag changes. Sixty-seven candidate character identities were seen during capture; this does not mean 67 complete pose tracks were saved.

[CONFIRMED] World capture reported zero disk-busy frame drops and zero dropped actor samples. The host character transport's cumulative drop counter was 343 before this recording and remained 343: these are not 343 newly dropped samples.

The logged world-file data rate is approximately 96.07 MiB/minute. This is storage throughput, not a measured game FPS or total performance overhead.

## HP and lifetime observations

[CONFIRMED] There are 3,455 lifetime observations covering 51 actors. Current classifier labels: 2,265 `ObservedAlive`, 1,190 `Unknown`, zero `DeathFlagged`. **ObservedAlive is only the current classifier label, not proof that an actor is alive.**

Five actors transition from positive recorded HP to zero:

| Replay actor ID | Model | NPC parameter | Initial positive HP | First zero HP, seconds |
| --- | ---: | ---: | ---: | ---: |
| 17 | 4070 | 40700010 | 88 | 40.993 |
| 24 | 4311 | 43112110 | 219 | 38.701 |
| 26 | 4311 | 43110010 | 219 | 16.155 |
| 27 | 4311 | 43111210 | 219 | 11.107 |
| 35 | 4351 | 43511010 | 657 | 77.408 |

Times are relative to the first world player frame. Main-file sampling starts approximately 44 ms earlier.

[CONFIRMED] All five retain a clear captured SDK death flag and an enabled render flag, and have pose records after HP reaches zero. Their entity IDs are zero; replay-local IDs must not be treated as persistent cross-session entity identifiers.

[UNKNOWN] HP depletion alone does not prove the engine's dying/corpse/destruction phase or a confirmed kill. Fifteen actors contain zero HP at some point, but ten already had zero HP when first observed. Do not count those as fifteen kills. An earlier interim commentary said six HP depletions; the completed inspection establishes **five**.

[CONFIRMED] Some actors become pose-unavailable/unobserved. These observations do not establish death or despawn. No accepted companion identities were recorded; diagnostics for model 8002 had no validated pose arrays. This recording does not validate Torrent skeleton capture.

## Playback blocker

[CONFIRMED] Existing game log reports:

```text
ARRIVAL_ERROR: different coordinate origin; automatic travel disabled until root conversion is verified
REPLAY_ERROR_LATCHED: invalid root interpolation or coordinate origin; stop and explicitly play again to retry
```

The user also supplied the current UI message:

```text
20:30:23 REPLAY BLOCKED: different coordinate origin. Automatic travel requires corrected location metadata.
```

[CONFIRMED] `adapter/src/bone_replay.rs` calls `same_root_space` before arrival. In `adapter/src/replay_interpolation.rs`, that check requires equal origin IDs **and** matching finite anchor XYZ values within 0.01 units. The generic error therefore does not establish whether the origin ID, anchor, or both differ. Initial rejection removes the loaded replay; the subsequent selection failure releases player ownership. Logged accuracy covers **zero settled frames** and is not a successful playback measurement.

[CONFIRMED] Recorded player block is constant at 1009394944, while origin is -1 throughout. The recorded anchor changes:

| World-relative time | Anchor XYZ |
| ---: | --- |
| 0.000 s | -8, -104, -96 |
| 4.891 s | -40, -104, -96 |
| 15.454 s | -72, -104, -80 |
| 34.115 s | -104, -104, -56 |

[CONFIRMED] Host log shows the first Play attempt near 16.873 seconds, with subsequent attempts near 21.934 and 35.057 seconds. The required recorded anchor depends on the selected timeline position.

[UNKNOWN] Exact live origin/anchor at rejection: the failing arrival branch does not log them. Do not infer a different region from this error, and do not simply remove the guard or reinterpret the anchor as a global teleport destination. Correct coordinate conversion still needs proof.

## Recommended next development work

1. Log live and requested block, signed origin, anchor, local physics position, timeline timestamp, and each failed comparison at arrival rejection.
2. Establish the actual SDK coordinate basis and root/proxy conversion across these recorded anchor transitions; then implement safe placement and interpolation. Preserve normal-control restoration on failure.
3. Expand read-only death/lifecycle investigation: the captured death flag alone missed all five observed HP-depletion transitions. Do not fake full death reconstruction by writing HP or progression flags.
4. Continue safe existing-body isolation and reconstruction research separately. This recording does not prove native spawn, corpse revival, VFX replay, or visual NPC rewind.

No new recording is needed to investigate these file-level issues. Preserve this fixture and both companion files.

## Local diagnostic evidence

- `.research/enemy-test-erplay-validation.log`: actual C++ ERPLAY reader result.
- `.research/enemy-test-world-validation.log`: actual Rust world validator/root inspector result.
- `.research/enemy-test-observations.json`: complete observation analysis.
- `.research/enemy-test-root-metadata.json`: decoded player root transitions and actor catalog.
- Game log: `%TEMP%\TheaterModeGame.log`.
- Host log: `%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log`.

Result: **capture files validated; in-game replay blocked before transform application; full enemy death/rewind reconstruction remains unverified.**

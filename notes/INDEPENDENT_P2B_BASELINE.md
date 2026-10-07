# Independent P2b continuation

Created 2026-10-07 at the user's request.

- Source copied via local Git clone with `--no-hardlinks` from `EldenRingTheatreMode-claude`.
- Baseline commit: `3a4f97fe6499bf4f8fbe97cf413645ff231d8b1a`.
- Independent branch: `codex/p2b-continuation`; no push remote configured.
- Original Claude checkout and `TheaterMode-Current` build are preserved.
- New project: `C:\Users\user\Documents\ChatGPT\elden ring theater mode\EldenRingTheatreMode-P2b-independent`.
- Preserved package: `outputs/P2b-baseline`, copied from the original `TheaterMode-Current`; every copied file's SHA-256 was compared with its source. `COPY_VERIFICATION.json` records those hashes.
- This is the user's existing P2b package, **not a newly built or newly runtime-validated feature release**. Old package manifests and tests retain their original dates and meaning.

## Resume point

The latest source implements Phase 1 and Phase 2.1, 2.2 and 2.8 core as recorded in `docs/PHASES_SUMMARY.md`. P2b additions still require user in-game validation. Earlier own-player bone playback was user-verified; that does not verify new actor, arrival or world-state features.

Next missing phase: 2.3 mounts and summons. Keep the current master replay clock, skeletal pose reconstruction, version guard, launcher and YAFSML integration. No global time slowdown for replay speed.

New Hexinton research is documented in `notes/HEXINTON_REFERENCE.md`. Its table targets 2.7.1.0, not our 2.7.0.0. Mount fields need runtime read-only instrumentation and corroboration before new ownership writes. Mount/summon spawning and full mount lifecycle replay are not implemented by this research checkpoint.

Do not run the preserved baseline as though it were isolated storage: launcher/settings and replay storage behavior are inherited from the original. Configure and audit those paths before running two hosts or producing a new experimental runtime build. Never overwrite `TheaterMode-Current` or Phase5; always pass an explicit output path to the existing build script.

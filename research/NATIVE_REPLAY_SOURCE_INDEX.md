# Native replay local source index

References read without modifications. New clones are snapshot references (no automatic pulls). SDK dependency remains pinned at3c8c1d7. Commit SHA proves source revision, not binary compatibility.

## https://github.com/vswarte/fromsoftware-rs

- Local: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\fromsoftware-rs-vswarte`
- SHA: `59fbd3b3b7daaf14aca47c9f73530493dba6bc79`
- Matching files: 18
- Files: `tools\debug-darksouls3\src\display\world_chr_man.rs`, `tools\debug-eldenring\src\display\world_chr_man.rs`, `crates\eldenring\src\cs\chr_ins.rs`, `crates\darksouls3\src\sprj\world_chr_man.rs`, `tools\debug-eldenring\src\display\game_data_man.rs`, `crates\darksouls3\src\sprj\game_data_man.rs`, `crates\darksouls3\src\sprj\chr_ins.rs`, `crates\eldenring\src\cs\world_chr_man.rs`, `crates\eldenring\src\cs\game_data_man.rs`, `crates\darksouls3\mapper-profile.toml`, `crates\eldenring\src\cs\field_ins.rs`, `tools\param-generator\params\sekiro\NetworkAreaParam.xml`

## https://github.com/KamiyamaShiki0704/fromsoftware-rs

- Local: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\fromsoftware-rs-kamiyama`
- SHA: `7d4fdd66eda1a35284c170feb4b07b058c005f8f`
- Matching files: 16
- Files: `tools\debug-darksouls3\src\display\world_chr_man.rs`, `tools\debug-eldenring\src\display\world_chr_man.rs`, `tools\param-generator\params\darksouls3\NetworkAreaParam.xml`, `crates\eldenring\src\cs\world_chr_man.rs`, `tools\param-generator\params\darksouls3\PlayRegionParam.xml`, `crates\eldenring\src\cs\chr_ins.rs`, `crates\eldenring\src\cs\field_ins.rs`, `tools\param-generator\params\eldenring\NetworkAreaParam.xml`, `tools\param-generator\params\sekiro\NetworkAreaParam.xml`, `tools\param-generator\params\eldenring\PlayRegionParam.xml`, `tools\param-generator\params\sekiro\PlayRegionParam.xml`, `crates\darksouls3\mapper-profile.toml`

## https://github.com/Dasaav-dsv/libER

- Local: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\libER`
- SHA: `d8ae5c92719adca4344d8d902b63286b807f562b`
- Matching files: 3
- Files: `include\param\paramdef\NETWORK_AREA_PARAM_ST.hpp`, `include\param\paramdef\NETWORK_PARAM_ST.hpp`, `include\param\paramdef\PLAY_REGION_PARAM_ST.hpp`

## https://github.com/NightFyre/EldenRing-SDK

- Local: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\EldenRing-SDK`
- SHA: `37652c02bf91c845aac14c0a4e1ee0f9a965df9c`
- Matching files: 0
- Files: No direct Replay/Bloodstain source hits; architecture/loader reference only

## https://github.com/FriXeee/ELDENRING-INTERNAL

- Local: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\ELDENRING-INTERNAL`
- SHA: `ab62d3b64495fec46c3ff0e0c1eca1fc52dcbc10`
- Matching files: 0
- Files: No direct Replay/Bloodstain source hits; architecture/loader reference only

## https://github.com/soulsmods/EldenRingHKS

- Local: `C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\EldenRingHKS`
- SHA: `d88d6441f5fccfdd6a5fd10d493309b680181897`
- Matching files: 0
- Files: No direct Replay/Bloodstain source hits; architecture/loader reference only

## https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA

- Local: `C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\Elden-Ring-CT-TGA`
- SHA: `7926205c5a2ed236dd31278c4f5579c964ceec35`
- Matching files: 1
- Files: `CheatTable\CheatEntries\Open - The Grand Archives - Elden Ring\Coordinates & Teleport\Teleport to Map-Relative Coordinates\ Teleport to Bloodstain.xml`

## https://github.com/Logersnamed/FreecamMod

- Local: `C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\FreecamMod`
- SHA: `a4628aaf50d88feeda56f79573cf2842eddec54a`
- Matching files: 0
- Files: No direct Replay/Bloodstain source hits; architecture/loader reference only

## https://github.com/micthiesen/unseamless-coop

- Local: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\unseamless-coop`
- SHA: `8faed9e1efb1e76e36d253e296daa051f4581253`
- Matching files: 0
- Files: No direct Replay/Bloodstain source hits; architecture/loader reference only

## https://github.com/LukeYui/EldenRingSeamlessCoopRelease

- Local: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\EldenRingSeamlessCoopRelease`
- SHA: `27ea9d7e5f50b35097d53f0ed8c5d28cba121bab`
- Matching files: 0
- Files: No direct Replay/Bloodstain source hits; architecture/loader reference only

## Interpretation

vswarte current SDK has BloodstainData/GameDataMan fields absent from the pinned fork; those newer offsets are REFERENCE only. libER/older SDK/internal sources are cross-checks, not offset authorities. HKS provides action/behavior context. CT tables provide candidate labels/scripts, never auto-executed. FreecamMod is an integration/input reference. unseamless-coop is a local clean-room session architecture reference; this snapshot has no direct bloodstain replay matches. Seamless release repository is a release reference, not proprietary source access. No binaries or game pseudocode redistributed.

Official docs: https://ersc-docs.github.io/how-to-install-and-update/ (read2026-10-06); installation/session documentation does not prove native replay codec.

Exact current working checkout is Documents/ChatGPT/elden ring theater mode/EldenRingTheatreMode-phase4 at parent6432477. Spec historical primary Documents/Codex/.../EldenRingTheaterMode is backup commit a81efe4; not overwritten.

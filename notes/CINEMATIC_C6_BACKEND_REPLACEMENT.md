# C6 вЂ” replace native timing and camera backends

Independent branch codex/cinematic-editor-pass. User requested removal of old systems.

Removed: GameTimingAdapter.cpp/.h (replaced by EldenRingTimingAdapter), old 0x681970 camera-copy hook/signature, render-camera +0x20 lookup, world opt-in checkbox.

Installed: CameraTools-equivalent CSFlipperImp +0x2CC backend (validated root458DB58), automatic world timing on Play; CameraTools-site3BB458 native camera copy suppression with independent ABI-preserving MASM bridge and full53-byte profile guard. Restore path: original native copy on inactive/invalid state. Existing Free/Dolly/Bone editor, host clock, spline evaluation and quaternion SLERP remain; those are not competing native hooks.

No proprietary DLL/code/assets were embedded or redistributed. Original reference and stable packages preserved. Source mechanism STATIC_VERIFIED; compilation/tests reported in package manifest. Runtime and visual behavior UNVERIFIED. Gamepad handling/native world pause and other earlier feature gaps remain.

New package outputs/Cinematic-C6-reference-backends. Close game/old host before loading matching EXE/DLL through existing YAFSML. Do not run CameraTools concurrently. Logs: TEMP/TheaterModeGame.log and LOCALAPPDATA/EldenRingTheaterMode/logs/TheaterModeRecorder.log. First check native camera restoration, then Free/FOV, then1/.5/.25/.1/.05 world rates. F6 stops; test extreme values only after normal rates pass.

Release AMD64 built successfully.14/14 CTest passed,49 Rust passed,1 optional ignored. New offline fixture exercises actual MASM bridge pass-through/stack return using valid synthetic camera objects, without game hooks or writes. Active suppression branch still requires live validation.

#pragma once
#include <cstdint>
#include <windows.h>
#define TM_PROFILE_NAME "EldenRing_1_17"
#define TM_EXPECTED_EXE_PATH L"C:\\Users\\user\\Downloads\\ELDEN RING\\Game\\eldenring.exe"
#define TM_EXPECTED_FILE_VERSION "2.7.0.0"
#define TM_EXPECTED_PRODUCT_VERSION "2.7.0.0"
#define TM_EXPECTED_VERSION_MAJOR 2
#define TM_EXPECTED_VERSION_MINOR 7
#define TM_EXPECTED_VERSION_PATCH 0
#define TM_EXPECTED_VERSION_BUILD 0
#define TM_EXPECTED_PE_MACHINE 0x8664
#define TM_EXPECTED_SHA256 "D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134"
// Weather request mailbox and active weather ID: exact-target constructor/consumer evidence C19.
#define TM_VAL_WEATHER_ROOT_RVA 0x3D6D3F0
#define TM_VAL_WEATHER_ROOT_SITE 0x582990
#define TM_VAL_WEATHER_CONSTRUCTOR_SITE 0x646798
#define TM_VAL_WEATHER_CONSUMER_SITE 0x64B1FD
#define TM_OFF_WEATHER_REQUEST 0x02
#define TM_OFF_WEATHER_CURRENT 0x2A
#define TM_VAL_TIMING_SITE_A 0xDEB30F
#define TM_VAL_TIMING_SITE_B 0xDEBE2F
#define TM_VAL_TIMING_ROOT_RVA 0x458DB58
#define TM_OFF_TIMING_SCALE 0x2CC
#define TM_VAL_CAMERA_INTERCEPT_RVA 0x3BB458
#define TM_VAL_CAMERA_CONTINUE_RVA 0x3BB48D
#define TM_CAMERA_INTERCEPT_BYTES {0x89,0x42,0x50,0x8b,0x41,0x54,0x89,0x42,0x54,0x8b,0x41,0x58,0x89,0x42,0x58,0x8b,0x41,0x5c,0x89,0x42,0x5c,0x0f,0x28,0x41,0x10,0x0f,0x29,0x42,0x10,0x0f,0x28,0x49,0x20,0x0f,0x29,0x4a,0x20,0x0f,0x28,0x41,0x30,0x0f,0x29,0x42,0x30,0x0f,0x28,0x49,0x40,0x0f,0x29,0x4a,0x40}
#define TM_VAL_HUD_OPACITY_RVA 0x11624E0
#define TM_HUD_OPACITY_BYTES {0x41,0x0f,0x28,0x00,0x48,0x8b,0xc2,0x41,0x0f,0x28,0x48,0x10,0x0f,0x29,0x02,0x0f,0x29,0x4a,0x10,0xc3}
#define TM_OFF_CAMERA_MATRIX 0x10
#define TM_OFF_CAMERA_FOV 0x50
#define TM_OFF_CAMERA_NEAR_PLANE 0x58
#define TM_CHECK_PATH 0x0001
#define TM_CHECK_FILE_VERSION 0x0002
#define TM_CHECK_PRODUCT_VERSION 0x0004
#define TM_CHECK_ARCH 0x0008
#define TM_CHECK_SHA256 0x0010
#define TM_CHECK_IMAGE_BASE 0x0020
#define TM_ERR_PATH_OR_RESOURCE 0x101
#define TM_ERR_PATH_MISMATCH 0x102
#define TM_ERR_FILE_VERSION 0x103
#define TM_ERR_PRODUCT_VERSION 0x104
#define TM_ERR_ARCH 0x105
#define TM_ERR_IMAGE_BASE 0x106
#define TM_ERR_SHA256 0x107
#define TM_ERR_PE_READ 0x108
struct TmValidationReport {
    uint32_t size;
    uint32_t status;
    uint32_t checked;
    uint32_t passed;
    uint16_t file_version[4];
    uint16_t product_version[4];
    uint16_t machine;
    uint16_t reserved;
    uintptr_t image_base;
    wchar_t runtime_path[32768];
    char sha256[65];
};
extern "C" __declspec(dllexport) uint32_t __cdecl tm_validate_profile(const wchar_t* executable_path, uintptr_t image_base, TmValidationReport* report);
// 0: known anti-cheat process absent, 1: present, -1: enumeration unavailable.
extern "C" __declspec(dllexport) int __cdecl tm_anti_cheat_state();

// ---------------------------------------------------------------------------------------------
// Game memory layout used by Theater Mode, for this exact executable (2.7.0.0, SHA above).
// After a game patch, update this block only. TM_OFF_* are byte offsets, TM_VAL_* plain numbers,
// TM_AOB_* byte patterns for pelite (' marks the address the code uses). adapter/build.rs exports
// every TM_OFF_/TM_VAL_/TM_AOB_ macro to Rust as a constant of the same name without the TM_ prefix.
// ---------------------------------------------------------------------------------------------
// Bone pose (skeleton probe K1, write tests K2-K4, 2026-10-07)
#define TM_OFF_CHRINS_POSE_IMPORTER 0x398      /* ChrIns -> CSFD4LocationHkaPoseImporter */
#define TM_OFF_POSE_IMPORTER_LOCAL 0x50        /* -> hkQsTransform[bones], parent space */
#define TM_OFF_POSE_IMPORTER_MODEL 0x60        /* -> hkQsTransform[bones], model space */
#define TM_VAL_PLAYER_BONES 150                /* c0000 skeleton */
// ChrIns debug flags: constructor RVA 0x3E7409 initializes +0x538 (the pinned SDK says +0x530,
// which is a callback pointer; writing it crashed the game). Bit 0x8 ignore damage (from the SDK
// bit list, applied at the corrected offset), 0x10 no move, 0x20 no attack.
#define TM_OFF_CHRINS_DEBUG_FLAGS 0x538
#define TM_VAL_DEBUG_FLAG_NO_DAMAGE 0x8
// Grace warp, the game's own fast travel: fn(CSLuaEventScriptImitation*, CSLuaEventProxy*,
// grace entity id - 1000). Same call the Hexinton all-in-one table's "Fast Travel and Warp" uses.
#define TM_AOB_LUA_WARP "C3 ? ' ? ? ? ? ? 57 48 83 EC ? 48 8B FA 44"
#define TM_VAL_GRACE_ID_BIAS 1000
#define TM_OFF_LUA_EVENT_MAN_PROXY 0x8         /* CSLuaEventManImp -> CSLuaEventProxy (is_load_wait) */
#define TM_OFF_LUA_EVENT_MAN_IMITATION 0x18    /* CSLuaEventManImp -> CSLuaEventScriptImitation */
// Skeleton (hkaSkeleton via the pose importer). Bone count is read three ways (parent indices, bones,
// reference pose) and only trusted when all three agree, which also checks these offsets at runtime.
#define TM_OFF_POSE_IMPORTER_SKELETON 0x48     /* importer -> hkaSkeleton */
#define TM_OFF_HKA_SKELETON_PARENTS 0x20       /* -> int16 parent index per bone */
#define TM_OFF_HKA_SKELETON_PARENT_COUNT 0x28
#define TM_OFF_HKA_SKELETON_BONE_COUNT 0x38
#define TM_OFF_HKA_SKELETON_REFPOSE_COUNT 0x48
// Structural guard for the debug flags: +0x530 must hold this callback (RVA) before +0x538 is written.
#define TM_OFF_CHRINS_DEBUG_CALLBACK 0x530
#define TM_VAL_CHRINS_DEBUG_CALLBACK_RVA 0x3F8FD0
#define TM_VAL_DEBUG_FLAG_NO_MOVE 0x10
#define TM_VAL_DEBUG_FLAG_NO_ATTACK 0x20
// Recording radius for enemies/NPCs/bosses (metres) and the "near" distance for full-rate sampling.
#define TM_VAL_ACTOR_RADIUS 100
#define TM_VAL_ACTOR_NEAR 30
// Corruption guard for the SDK's companion ChrSet capacity, not a recording actor limit.
#define TM_VAL_COMPANION_SCAN_GUARD 4096
// Update-LOD ("omission") override (STEP A). These come from the pinned SDK structs and are asserted
// against them by a unit test (omission.rs game_profile_offsets_match_the_sdk_layout). Values are
// plausibility-checked at runtime before any write: omission mode in {-2,0,1,5,20,30}, override in -1..2.
#define TM_OFF_CHRINS_OMISSION_MODE 0xB4
#define TM_OFF_CHRINS_FLAGS_1C4 0x1C4
#define TM_OFF_WCMDBG_OMISSION_OVERRIDE 0x20
#define TM_OFF_WCMDBG_OMISSION_NEAR 0x24
#define TM_OFF_WCMDBG_OMISSION_FAR 0x30

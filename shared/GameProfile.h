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

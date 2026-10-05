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

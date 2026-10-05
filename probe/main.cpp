#include <windows.h>
#include "GameProfile.h"
#include <iostream>
#include <sstream>
#include <string>
#pragma comment(lib,"GameProfile.lib")
static std::wstring ver(const uint16_t v[4]){std::wstringstream s;s<<v[0]<<L'.'<<v[1]<<L'.'<<v[2]<<L'.'<<v[3];return s.str();}
static const wchar_t* arch(uint16_t m){return m==IMAGE_FILE_MACHINE_AMD64?L"AMD64":m==IMAGE_FILE_MACHINE_I386?L"x86":L"OTHER/UNKNOWN";}
static void check(const wchar_t* name,uint32_t flag,const TmValidationReport&r){const bool ok=(r.checked&flag)&&(r.passed&flag);std::wcout<<name<<L": "<<((r.checked&flag)?(ok?L"PASS":L"FAIL"):L"NOT AVAILABLE")<<L"\n";}
int wmain(int argc,wchar_t**argv){
 std::wstring path;for(int i=1;i+1<argc;i++)if(std::wstring(argv[i])==L"--game-exe")path=argv[++i];
 if(path.empty()){std::wcerr<<L"Usage: EldenRingCompatibilityProbe.exe --game-exe <eldenring.exe>\n";return 2;}
 TmValidationReport r{};const auto status=tm_validate_profile(path.c_str(),0,&r);
 std::wcout<<L"ELDEN RING COMPATIBILITY PROBE\n\nExecutable: "<<r.runtime_path<<L"\nDefault executable location: "<<TM_EXPECTED_EXE_PATH<<L"\nPath policy: filename eldenring.exe; installation directory may differ\nFile version: "<<ver(r.file_version)<<L"\nProduct version: "<<ver(r.product_version)<<L"\nPE architecture: "<<arch(r.machine)<<L"\nSHA-256 (file on disk): "<<r.sha256<<L"\nExpected profile: "<<TM_PROFILE_NAME<<L" / WW_2.7.0.0\nSelected profile: "<<(status==0?L"EldenRing_1_17":L"NONE")<<L"\n";
 check(L"Path check",TM_CHECK_PATH,r);check(L"File-version check",TM_CHECK_FILE_VERSION,r);check(L"Product-version check",TM_CHECK_PRODUCT_VERSION,r);check(L"AMD64 check",TM_CHECK_ARCH,r);check(L"SHA-256 check",TM_CHECK_SHA256,r);
 std::wcout<<L"Image-base check: NOT APPLICABLE (static probe)\nProfile validation: "<<(status==0?L"PASS":L"FAIL")<<L" (code 0x"<<std::hex<<status<<std::dec<<L")\nRuntime CSTaskImp/WorldChrMan/player access: RUNTIME REQUIRED\n";
 return status==0?0:1;
}

#include "GameProfile.h"
#include <winver.h>
#include <bcrypt.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#pragma comment(lib,"Version.lib")
#pragma comment(lib,"Bcrypt.lib")

static bool hash_file(const wchar_t* path,unsigned char digest[32]) {
    std::ifstream file(std::filesystem::path(path),std::ios::binary);if(!file)return false;
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};DWORD object_length{},digest_length{},returned{};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    bool ok=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,(PUCHAR)&object_length,sizeof(object_length),&returned,0)>=0&&BCryptGetProperty(algorithm,BCRYPT_HASH_LENGTH,(PUCHAR)&digest_length,sizeof(digest_length),&returned,0)>=0&&digest_length==32;
    std::vector<UCHAR> object(object_length);if(ok)ok=BCryptCreateHash(algorithm,&hash,object.data(),object_length,nullptr,0,0)>=0;
    std::array<char,65536> buffer{};while(ok&&file){file.read(buffer.data(),buffer.size());const auto count=(ULONG)file.gcount();if(count)ok=BCryptHashData(hash,(PUCHAR)buffer.data(),count,0)>=0;}
    if(ok)ok=BCryptFinishHash(hash,digest,32,0)>=0;if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);return ok;
}
static void split_version(DWORD ms,DWORD ls,uint16_t out[4]){out[0]=HIWORD(ms);out[1]=LOWORD(ms);out[2]=HIWORD(ls);out[3]=LOWORD(ls);}
static bool version_eq(const uint16_t v[4]){return v[0]==TM_EXPECTED_VERSION_MAJOR&&v[1]==TM_EXPECTED_VERSION_MINOR&&v[2]==TM_EXPECTED_VERSION_PATCH&&v[3]==TM_EXPECTED_VERSION_BUILD;}
static bool read_machine(const wchar_t* path,uint16_t& machine){std::ifstream f(std::filesystem::path(path),std::ios::binary);IMAGE_DOS_HEADER dos{};f.read((char*)&dos,sizeof(dos));if(!f||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<=0)return false;f.seekg(dos.e_lfanew);DWORD sig{};IMAGE_FILE_HEADER pe{};f.read((char*)&sig,4);f.read((char*)&pe,sizeof(pe));if(!f||sig!=IMAGE_NT_SIGNATURE)return false;machine=pe.Machine;return true;}
extern "C" uint32_t __cdecl tm_validate_profile(const wchar_t* input,uintptr_t image_base,TmValidationReport* r){
    if(!r||!input)return TM_ERR_PATH_OR_RESOURCE;ZeroMemory(r,sizeof(*r));r->size=sizeof(*r);r->image_base=image_base;
    wchar_t path[32768]{};DWORD plen=GetFullPathNameW(input,(DWORD)_countof(path),path,nullptr);if(!plen||plen>=_countof(path))return r->status=TM_ERR_PATH_OR_RESOURCE;lstrcpynW(r->runtime_path,path,(int)_countof(r->runtime_path));
    // A tester may install the exact supported binary on a different drive.
    // Require its executable name; version, AMD64 and the full on-disk SHA-256
    // below remain mandatory. The default installation directory is not identity.
    const auto filename=std::filesystem::path(path).filename().wstring();
    r->checked|=TM_CHECK_PATH;if(CompareStringOrdinal(filename.c_str(),-1,L"eldenring.exe",-1,TRUE)==CSTR_EQUAL)r->passed|=TM_CHECK_PATH;
    DWORD ignored{},size=GetFileVersionInfoSizeW(path,&ignored);if(size){std::vector<BYTE> data(size);VS_FIXEDFILEINFO* info{};UINT bytes{};if(GetFileVersionInfoW(path,0,size,data.data())&&VerQueryValueW(data.data(),L"\\",(LPVOID*)&info,&bytes)&&bytes>=sizeof(*info)&&info->dwSignature==0xFEEF04BD){split_version(info->dwFileVersionMS,info->dwFileVersionLS,r->file_version);split_version(info->dwProductVersionMS,info->dwProductVersionLS,r->product_version);r->checked|=TM_CHECK_FILE_VERSION|TM_CHECK_PRODUCT_VERSION;if(version_eq(r->file_version))r->passed|=TM_CHECK_FILE_VERSION;if(version_eq(r->product_version))r->passed|=TM_CHECK_PRODUCT_VERSION;}}
    if(read_machine(path,r->machine)){r->checked|=TM_CHECK_ARCH;if(r->machine==TM_EXPECTED_PE_MACHINE)r->passed|=TM_CHECK_ARCH;}
    if(image_base){r->checked|=TM_CHECK_IMAGE_BASE;if(r->image_base)r->passed|=TM_CHECK_IMAGE_BASE;}
    unsigned char digest[32]{};if(hash_file(path,digest)){static const char hex[]="0123456789ABCDEF";for(int i=0;i<32;i++){r->sha256[i*2]=hex[digest[i]>>4];r->sha256[i*2+1]=hex[digest[i]&15];}r->sha256[64]=0;r->checked|=TM_CHECK_SHA256;bool hash_matches=true;for(int i=0;i<64;i++){if(r->sha256[i]!=TM_EXPECTED_SHA256[i]){hash_matches=false;break;}}if(hash_matches)r->passed|=TM_CHECK_SHA256;}
    if(!(r->passed&TM_CHECK_PATH))return r->status=TM_ERR_PATH_MISMATCH;
    if(!(r->checked&TM_CHECK_FILE_VERSION))return r->status=TM_ERR_PATH_OR_RESOURCE;
    if(!(r->passed&TM_CHECK_FILE_VERSION))return r->status=TM_ERR_FILE_VERSION;
    if(!(r->passed&TM_CHECK_PRODUCT_VERSION))return r->status=TM_ERR_PRODUCT_VERSION;
    if(!(r->checked&TM_CHECK_ARCH))return r->status=TM_ERR_PE_READ;
    if(!(r->passed&TM_CHECK_ARCH))return r->status=TM_ERR_ARCH;
    if(image_base&&!(r->passed&TM_CHECK_IMAGE_BASE))return r->status=TM_ERR_IMAGE_BASE;
    if(!(r->checked&TM_CHECK_SHA256))return r->status=TM_ERR_PE_READ;
    if(!(r->passed&TM_CHECK_SHA256))return r->status=TM_ERR_SHA256;
    return r->status=0;
}


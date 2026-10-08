#include "EldenRingHudAdapter.h"
#include "GameProfile.h"
#include <MinHook.h>
#include <atomic>
#include <cstring>
namespace game_hud {
namespace {
std::atomic_bool available=false,hidden=false,active=false,focus=false,fault=false;
std::atomic<ULONGLONG> heartbeat=0;
using Copy=void*(*)(void*,void*,const void*);Copy original=nullptr;
// Guarded game leaf: output's first vector is the HUD opacity/color value.
// Preserve the other vector and return value. No global settings or saves changed.
void* copy(void*owner,void*out,const void*source){
 void* result=original(owner,out,source);
 if(hidden&&active&&focus&&!fault&&GetTickCount64()-heartbeat.load()<500){
  __try{auto*v=static_cast<float*>(out);for(int i=0;i<4;++i)v[i]=0;}
  __except(EXCEPTION_EXECUTE_HANDLER){fault=true;hidden=false;}
 }
 return result;
}
}
bool ready(){return available&&!fault;}
bool requested(){return hidden;}
void request(bool hide){hidden=hide&&ready();}
void focused(bool value){focus=value;}
}
extern "C" int tm_hud_initialize(){
 using namespace game_hud;auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
 const unsigned char expected[]=TM_HUD_OPACITY_BYTES;unsigned char bytes[sizeof(expected)];SIZE_T got=0;
 if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(base+TM_VAL_HUD_OPACITY_RVA),bytes,sizeof(bytes),&got)||got!=sizeof(bytes)||memcmp(bytes,expected,sizeof(bytes)))return 0;
 auto initialized=MH_Initialize();if(initialized!=MH_OK&&initialized!=MH_ERROR_ALREADY_INITIALIZED)return 0;
 auto site=reinterpret_cast<void*>(base+TM_VAL_HUD_OPACITY_RVA);
 if(MH_CreateHook(site,reinterpret_cast<void*>(copy),reinterpret_cast<void**>(&original))!=MH_OK)return 0;
 if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);original=nullptr;return 0;}available=true;return 1;
}
extern "C" void tm_hud_game_context(int value){game_hud::active=value!=0;game_hud::heartbeat=GetTickCount64();}

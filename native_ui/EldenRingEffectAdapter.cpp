// Visual-effect (FFX) spawn capture and replay for one exact game build (Elden Ring 2.7.0.0).
//
// The game creates a one-shot effect at a world position through one function (found with the project's Ghidra export, RVA
// 0xDA93E0): rcx = effect manager (global pointer at RVA 0x3D87D48), edx = effect id, r8 = unused tag, r9 = float[3] position,
// 5th argument = out pointer for the created instance. The function reads the camera distance, creates the effect instance named
// "Sfx_s%09d" at that position with an identity rotation and registers it with the manager.
//   CAPTURE: while recording, every call (id, position, time) is queued and drained by the Rust side into the replay file.
//   REPLAY:  the Rust side calls the same function again at the recorded time with the recorded position (rebased into today's
//            physics frame). A replay-made call is flagged so it is never captured again.
// Nothing is changed in the game's data or saves. The hook is installed only if the first bytes of the function match the build.
#include "GameProfile.h"
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>
#include <realtimeapiset.h>
namespace game_effects {
namespace {
struct Event{std::uint64_t time_ns;std::uint32_t id;float pos[3];};
using Spawn=void(__fastcall*)(void*,std::uint32_t,std::uint64_t,const float*,void*);
Spawn original=nullptr;std::uintptr_t base=0;
std::mutex lock;std::vector<Event> pending;
std::atomic_bool capture=false;std::atomic<std::uint64_t> seen=0,dropped=0,replayed=0;
thread_local bool replaying=false;
bool read_position(const float*pos,float*out){
 __try{out[0]=pos[0];out[1]=pos[1];out[2]=pos[2];return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void queue_event(std::uint32_t id,const float*p){
 ULONGLONG ticks=0;QueryInterruptTimePrecise(&ticks);
 Event e{ticks*100,id,{p[0],p[1],p[2]}};
 std::lock_guard guard(lock);
 if(pending.size()>=8192){++dropped;return;}
 pending.push_back(e);++seen;
}
void __fastcall detour(void*manager,std::uint32_t id,std::uint64_t tag,const float*pos,void*out){
 if(capture.load(std::memory_order_relaxed)&&!replaying&&pos){float p[3];if(read_position(pos,p))queue_event(id,p);}
 original(manager,id,tag,pos,out);
}
bool call_original(void*manager,std::uint32_t id,const float*pos){
 __try{void*out=nullptr;original(manager,id,0,pos,&out);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool read_manager(void**out){
 void*value=nullptr;SIZE_T got=0;
 if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(base+TM_VAL_EFFECT_MANAGER_PTR_RVA),&value,sizeof(value),&got)||got!=sizeof(value))return false;
 *out=value;return true;
}
}
}
extern "C" int tm_effect_initialize(){
 using namespace game_effects;base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
 const unsigned char expected[]=TM_EFFECT_SPAWN_BYTES;unsigned char bytes[sizeof(expected)];SIZE_T got=0;
 auto site=reinterpret_cast<void*>(base+TM_VAL_EFFECT_SPAWN_RVA);
 if(!ReadProcessMemory(GetCurrentProcess(),site,bytes,sizeof(bytes),&got)||got!=sizeof(bytes)||memcmp(bytes,expected,sizeof(bytes)))return 0;
 auto initialized=MH_Initialize();if(initialized!=MH_OK&&initialized!=MH_ERROR_ALREADY_INITIALIZED)return 0;
 if(MH_CreateHook(site,reinterpret_cast<void*>(detour),reinterpret_cast<void**>(&original))!=MH_OK)return 0;
 if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);original=nullptr;return 0;}
 return 1;
}
extern "C" void tm_effect_capture(int on){game_effects::capture=on!=0;}
extern "C" std::uint32_t tm_effect_drain(void*out,std::uint32_t max){
 using namespace game_effects;std::lock_guard guard(lock);
 const auto n=static_cast<std::uint32_t>(std::min<std::size_t>(pending.size(),max));
 if(n){memcpy(out,pending.data(),n*sizeof(Event));pending.erase(pending.begin(),pending.begin()+n);}
 return n;
}
extern "C" void tm_effect_stats(std::uint64_t*seen_out,std::uint64_t*dropped_out,std::uint64_t*replayed_out){
 *seen_out=game_effects::seen.load();*dropped_out=game_effects::dropped.load();*replayed_out=game_effects::replayed.load();
}
// Creates one recorded effect at `pos` (physics frame). Must run on a game task. 1 = called, 0 = refused / unavailable.
extern "C" int tm_effect_spawn(std::uint32_t id,const float*pos){
 using namespace game_effects;if(!original||!pos)return 0;
 void*manager=nullptr;if(!read_manager(&manager)||!manager)return 0;
 replaying=true;const bool ok=call_original(manager,id,pos);replaying=false;
 if(ok)++replayed;return ok?1:0;
}

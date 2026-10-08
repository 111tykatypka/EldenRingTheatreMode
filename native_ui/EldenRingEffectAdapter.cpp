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
bool scene_guard(std::uintptr_t b);
using SceneCreate=void*(__fastcall*)(void*,void*,std::uint32_t,void*,const float*,int,int);
SceneCreate scene_original=nullptr;
// Every effect the game creates (hits, blood, spells, attached or free) passes through this one scene-controller function; capture
// the id and the translation of the 4x4 matrix (row 3). Replay-made calls are flagged and never captured.
void*__fastcall scene_detour(void*scene,void*handle,std::uint32_t id,void*params,const float*matrix,int a,int b){
 if(capture.load(std::memory_order_relaxed)&&!replaying&&matrix){float m[16];__try{for(int i=0;i<16;++i)m[i]=matrix[i];queue_event(id,m+12);}__except(EXCEPTION_EXECUTE_HANDLER){}}
 return scene_original(scene,handle,id,params,matrix,a,b);
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
 if(scene_guard(base)){
  auto initialized=MH_Initialize();if(initialized!=MH_OK&&initialized!=MH_ERROR_ALREADY_INITIALIZED)return 0;
  auto site=reinterpret_cast<void*>(base+TM_VAL_VFX_SCENE_CREATE_RVA);
  if(MH_CreateHook(site,reinterpret_cast<void*>(scene_detour),reinterpret_cast<void**>(&scene_original))!=MH_OK)return 0;
  if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);scene_original=nullptr;return 0;}
  return 1;}
 return 0;
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
 replaying=true;const bool ok=false&&call_original(manager,id,pos);replaying=false;
 if(ok)++replayed;return ok?1:0;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Replay through the scene-controller creation path (found by the particle research, RVA 0x1CA0CB0): CSSfxImp -> scene_ctrl ->
// create(scene, handle, fxr id, null parameters, 4x4 matrix, 0, 0). Only effects whose FXR is resident in the game's list are
// created (the debug-position function above silently ignores anything else). Every created effect owns one fixed handle slot and is
// stopped and released after a fixed real-time lifetime or when the replay ends.
namespace game_effects { namespace {
struct Slot{alignas(16) unsigned char handle[TM_VAL_VFX_HANDLE_SIZE];bool used=false;ULONGLONG expires=0;};
Slot slots[32];
std::atomic<std::uint64_t> created{0},not_resident{0},failed{0},no_slot{0};
bool read_mem(std::uintptr_t p,void*out,std::size_t n){SIZE_T got=0;return p>=0x10000&&p<=0x00007FFFFFFFFFFFULL-n&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),out,n,&got)&&got==n;}
bool scene_guard(std::uintptr_t b){
 struct Site{std::uintptr_t rva;unsigned char bytes[10];};
 const Site sites[]={{TM_VAL_VFX_SCENE_CREATE_RVA,{0x40,0x53,0x48,0x81,0xec,0x80,0,0,0,0x48}},{TM_VAL_VFX_HANDLE_STOP_RVA,{0x48,0x89,0x4c,0x24,0x08,0x57,0x48,0x83,0xec,0x20}},{TM_VAL_VFX_HANDLE_RELEASE_RVA,{0x48,0x89,0x4c,0x24,0x08,0x57,0x48,0x83,0xec,0x30}}};
 for(const auto&s:sites){unsigned char a[10]{};if(!read_mem(b+s.rva,a,10)||memcmp(a,s.bytes,10))return false;}return true;}
bool resident(std::uintptr_t scene,std::uint32_t wanted){
 std::uintptr_t graphics=0,container=0,head=0,node=0;
 if(!read_mem(scene+TM_OFF_FFX_GRAPHICS_MANAGER,&graphics,8)||!read_mem(graphics+TM_OFF_FFX_RESOURCE_CONTAINER,&container,8)||!read_mem(container+TM_OFF_FXR_LIST_HEAD,&head,8)||!read_mem(head,&node,8))return false;
 const auto deadline=GetTickCount64()+4;
 while(node&&node!=head&&GetTickCount64()<deadline){
  std::uint32_t id=0;std::uintptr_t next=0,wrapper=0,fxr=0;
  if(!read_mem(node,&next,8)||next==node||!read_mem(node+TM_OFF_FXR_NODE_ID,&id,4))return false;
  if(id==wanted)return read_mem(node+TM_OFF_FXR_NODE_WRAPPER,&wrapper,8)&&read_mem(wrapper,&fxr,8)&&fxr;
  node=next;}
 return false;}
using CreateFn=void*(__fastcall*)(void*,void*,std::uint32_t,void*,const float*,int,int);
using HandleFn=void(__fastcall*)(void*);
bool seh_create(std::uintptr_t b,std::uintptr_t scene,Slot&s,std::uint32_t id,const float*m){
 __try{reinterpret_cast<CreateFn>(b+TM_VAL_VFX_SCENE_CREATE_RVA)(reinterpret_cast<void*>(scene),s.handle,id,nullptr,m,0,0);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool seh_release(std::uintptr_t b,Slot&s){
 __try{reinterpret_cast<HandleFn>(b+TM_VAL_VFX_HANDLE_STOP_RVA)(s.handle);reinterpret_cast<HandleFn>(b+TM_VAL_VFX_HANDLE_RELEASE_RVA)(s.handle);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
std::atomic_bool scene_faulted=false;
void release_slot(Slot&s){if(!s.used)return;if(!seh_release(base,s))scene_faulted=true;memset(s.handle,0,sizeof(s.handle));s.used=false;}
}}
// manager = CSSfxImp address (from the Rust side, game callback thread only). Returns 1 created, 0 refused, -1 not resident, -2 no free
// slot, -3 faulted. `life_ms` is the real-time lifetime before the effect is stopped and released.
extern "C" int tm_effect_scene_spawn(std::uintptr_t manager,std::uint32_t id,const float*pos,std::uint32_t life_ms){
 using namespace game_effects;if(!manager||!pos||!id)return 0;if(scene_faulted)return -3;
 if(!base)base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
 static bool guarded=false,guard_ok=false;if(!guarded){guard_ok=scene_guard(base);guarded=true;}if(!guard_ok)return 0;
 std::uintptr_t scene=0;if(!read_mem(manager+TM_OFF_SFX_SCENE_CTRL,&scene,8)||!scene)return 0;
 if(!resident(scene,id)){++not_resident;return -1;}
 Slot*slot=nullptr;for(auto&s:slots)if(!s.used){slot=&s;break;}if(!slot){++no_slot;return -2;}
 alignas(16) float m[16]={1,0,0,0, 0,1,0,0, 0,0,1,0, pos[0],pos[1],pos[2],1};
 memset(slot->handle,0,sizeof(slot->handle));
 replaying=true;const bool made=seh_create(base,scene,*slot,id,m);replaying=false;
 if(!made){scene_faulted=true;++failed;return -3;}
 std::uintptr_t object=0;memcpy(&object,slot->handle+TM_OFF_VFX_HANDLE_OBJECT,sizeof(object));
 slot->used=true;slot->expires=GetTickCount64()+life_ms;
 if(!object){release_slot(*slot);++failed;return 0;}
 ++created;return 1;}
extern "C" void tm_effect_scene_tick(){
 using namespace game_effects;const auto now=GetTickCount64();for(auto&s:slots)if(s.used&&now>=s.expires)release_slot(s);}
extern "C" void tm_effect_scene_release_all(){using namespace game_effects;for(auto&s:slots)release_slot(s);}
extern "C" void tm_effect_scene_stats(std::uint64_t*c,std::uint64_t*nr,std::uint64_t*f,std::uint64_t*ns){using namespace game_effects;*c=created;*nr=not_resident;*f=failed;*ns=no_slot;}

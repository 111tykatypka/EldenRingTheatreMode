// Exact ER 2.7.0.0 ONLY. Developer experiment; never called from PostPhysics/IPC.
// Byte fingerprints and ABI callsites: tools/native_replay/build_ghost_prototype.ps1.
#include <windows.h>
#include <MinHook.h>
#include <intrin.h>
#include <atomic>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <thread>
#include <mutex>
#include "NativeGhostFingerprints.h"
extern "C" void tm_render_native_status(const char*);

namespace {
using U=uintptr_t;
U base{}, layout[7]{};
void(*logger)(const char*){};
void log(const char* fmt,...) { char text[2048];va_list a;va_start(a,fmt);vsnprintf(text,sizeof(text),fmt,a);va_end(a);logger(text);if(strncmp(text,"NATIVE_GHOST",12)==0)tm_render_native_status(text); }
bool read(U address,void* out,size_t bytes) { SIZE_T n{};return address>=0x10000 && address<=UINTPTR_MAX-bytes && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out,bytes,&n)&&n==bytes; }
template<class T> T get(U a) { T v{};read(a,&v,sizeof(v));return v; }
template<class F> F fn(U rva) {return reinterpret_cast<F>(base+rva);}
U world() {return get<U>(base+0x3d69ff8);}
struct NativeGhostRuntimeState {bool active{};uint64_t epoch{1},handle{UINT64_MAX};U actor{},entry{},data{},world{},manipulator{};uint32_t slot{};};
std::mutex stateMutex;
NativeGhostRuntimeState state;
// Tombstones are numeric event identities only. NEVER dereferenced or resolved.
std::atomic<U> retiredActor{},retiredManipulator{},retiredData{};
std::atomic<uint64_t> retiredEpoch{};
std::atomic<unsigned> command{}; // 0=OFF, 1=create, 2=remove
std::atomic<bool> used{}, contextSeen{}, readyLogged{};
std::atomic<ULONGLONG> deadline{};
std::atomic<uint64_t> stepCount{},buildCount{},requestStepCount{};
std::atomic<ULONGLONG> lastStepTick{};
std::atomic<uint32_t> timerBits{};
thread_local bool localStep{}, creating{};
thread_local U factoryActor{};
using LocalStep=void(*)(U,U,float,U); LocalStep stepOriginal{};
using Build=void(*)(U,U); Build buildOriginal{};
using Spawn=U(*)(U,uint32_t,U); Spawn spawnOriginal{};
using Drain=void(*)(U,U); Drain drainOriginal{};
using Remove=void(*)(U,U); Remove removeOriginal{};
using ActorCall=U(*)(U); ActorCall activateOriginal{},disableOriginal{},ghostDestroyOriginal{},manipDestroyOriginal{};
using Pair=void(*)(U,U); Pair enqueueOriginal{},deleterOriginal{};
using Release=int32_t(*)(U); Release releaseOriginal{};

NativeGhostRuntimeState snapshot() {std::lock_guard g(stateMutex);return state;}
bool retired(U a,const std::atomic<U>& identity) {return a && a==identity.load(std::memory_order_acquire);}
bool hasGhost(U w) {
 const auto cap=get<uint32_t>(w+0x10f48);const auto entries=get<U>(w+0x10f50);
 if(!entries||cap>0x100000)return true; // reject corrupt layout; handle index is 20 bits
 for(uint32_t i=0;i<cap;i++){U actor{};if(!read(entries+size_t(i)*16,&actor,8)||actor)return true;}
 return false;
}
bool ready(U w,U& player,U& recorder) {
 if(!w||get<uint32_t>(w+0x1e524)!=0)return false;
 player=get<U>(w+0x1e508);if(!player)return false;
 recorder=get<U>(player+layout[0]);
 if(!recorder||get<U>(recorder)!=base+0x2a4aa40||get<U>(recorder+0x10)!=player)return false;
 if(get<uint32_t>(recorder+0x40)==0||!get<U>(recorder+0x20))return false;
 // Same native block as recorder's oldest position: no cross-map experiment.
 if(get<uint32_t>(player+layout[6])!=get<uint32_t>(recorder+0x5c))return false;
 U modules=get<U>(player+layout[1]),physics=get<U>(modules+layout[2]);float p[3],q[4];
 if(!physics||!read(physics+layout[3],p,sizeof(p))||!read(physics+layout[4],q,sizeof(q)))return false;
 for(float x:p)if(!std::isfinite(x))return false;for(float x:q)if(!std::isfinite(x))return false;
 return true;
}
int kindLiteral(U manip) {
 unsigned char code[6]{};U getter=get<U>(get<U>(manip)+0x10);
 if(!read(getter,code,sizeof(code))||code[0]!=0xb8||code[5]!=0xc3)return -1;
 uint32_t kind{};memcpy(&kind,code+1,4);return kind<=7?static_cast<int>(kind):-1;
}
void observe(U actor,const char* prefix) {
 // Only at factory return, activation callback, or before native removal.
 U entry=get<U>(actor+0x10), ctrl=get<U>(actor+0x58), manip=get<U>(actor+0x588), data=get<U>(actor+0x740);
 U primary=get<U>(ctrl+0x18),modules=get<U>(actor+layout[1]),physics=get<U>(modules+layout[2]);
 float p[3]{},q[4]{};bool transforms=physics&&read(physics+layout[3],p,sizeof(p))&&read(physics+layout[4],q,sizeof(q));
 uint64_t handle=get<uint64_t>(actor+8);
 log("%s: literal_manipulator_type=%d (read getter instructions, not incompatible SDK reference-return ABI) ctrl_owner_matches=%u manip_owner_matches=%u",prefix,kindLiteral(manip),get<U>(ctrl+0x10)==actor,get<U>(manip+0xa8)==actor);
 log("%s: actor=0x%llX entry=0x%llX entry_actor_matches=%u handle=0x%llX ChrType=%u ChrCtrl=0x%llX primary=0x%llX ReplayManipulator=0x%llX vtable=0x%llX expected_vtable=%u ManipulatorType=%s gate132=%u ReplayData=0x%llX refcount=%d primary_count=%u primary_cursor=%u secondary_count=%u secondary_cursor=%u BlockId=0x%X slot=%u transform_read=%u position=(%.3f,%.3f,%.3f) rotation=(%.5f,%.5f,%.5f,%.5f)",
 prefix,actor,entry,entry&&get<U>(entry)==actor,handle,get<uint32_t>(actor+layout[5]),ctrl,primary,manip,get<U>(manip),get<U>(manip)==base+0x2a2eda0,get<U>(manip)==base+0x2a2eda0?"Replay(3)-exact-vtable":"UNKNOWN",get<uint8_t>(manip+0x132),data,get<int32_t>(data+8),get<uint32_t>(data+0xd0),get<uint32_t>(data+0x220),get<uint32_t>(data+0x224),get<uint32_t>(data+0x230),get<uint32_t>(actor+layout[6]),uint32_t(handle)&0xfffff,transforms,p[0],p[1],p[2],q[0],q[1],q[2],q[3]);
}
void retire(U actor,const char* reason) {
 std::lock_guard g(stateMutex);if(!state.active||state.actor!=actor)return;
 // No dereference: also safe if the world disappeared before a remove command.
 retiredManipulator.store(state.manipulator);retiredData.store(state.data);
 retiredActor.store(actor);retiredEpoch.store(state.epoch);
 state.active=false;state.epoch++;state.handle=UINT64_MAX;state.actor=state.entry=state.data=state.world=state.manipulator=0;
 command.store(0);
 log("NATIVE_GHOST_REMOVE: REQUESTED reason=%s retired_epoch=%llu next_epoch=%llu; runtime pointers invalidated; no retired-handle resolution",reason,retiredEpoch.load(),state.epoch);
}
void freeNative(U allocation) {
 if(!allocation)return;
 auto allocator=fn<U(*)(U)>(0xe1c010)(allocation);
 auto free=reinterpret_cast<void(*)(U,U)>(get<U>(get<U>(allocator)+0x68));
 free(allocator,allocation);
}
void releaseTemporary(U data) {
 if(!data)return;
 const auto old=releaseOriginal(data+8);
 log("NATIVE_REPLAY_DATA: creator reference release old=%d new=%d",old,old-1);
 // Exactly the native smart-reference release contract (NOT ghost teardown).
 if(old==1)reinterpret_cast<void(*)(U)>(get<U>(get<U>(data)))(data);
 else if(old<1)log("NATIVE_GHOST_ERROR: invalid native creator refcount; no additional cleanup attempted");
}
void create(U nativeContext) {
 U w=world(),player{},recorder{};
 if(snapshot().active||!ready(w,player,recorder)||hasGhost(w)){log("NATIVE_GHOST_ERROR: create precondition failed (world/player/recorder/block/occupied ghost set)");return;}
 U gameData=get<U>(base+0x3d61f98), assembly=get<U>(gameData+8), flags=get<U>(base+0x3d5eec0);
 U bufferAllocator=get<U>(base+0x3d8b390),objectAllocator=get<U>(base+0x3d8b3d8);
 if(!assembly||!flags||!bufferAllocator||!objectAllocator){log("NATIVE_GHOST_ERROR: native metadata/allocator unavailable");return;}
 // Native metadata ctor/fill. 0xc0-byte size is confirmed by serializer copies
 // and original 1407048e0 stack layout; none of the metadata is fabricated.
 alignas(16) unsigned char metadata[0xc0];
 fn<U(*)(void*)>(0x6514f0)(metadata);
 if(!fn<uint8_t(*)(U,void*)>(0x25f810)(assembly,metadata)){fn<void(*)(void*)>(0x3c12a0)(metadata);log("NATIVE_GHOST_ERROR: metadata fill failed");return;}
 U alternate=fn<U(*)(U)>(0x25f7e0)(assembly);
 // Live tests (2026-10-06, owner on foot, not mounted) returned a non-null alternate
 // metadata pointer, so its presence alone does not mean a mounted/paired recording.
 // Pass it exactly as the original call does; the one-actor guarantee is enforced below
 // on the decoded data (secondary count must be 0 or the factory is never called).
 log("NATIVE_GHOST: alternate metadata=0x%llX passed as the native call does; one-actor check uses decoded secondary count",alternate);
 // The original call is `test rax,rax; setne dl; call 1406f1ec0` (1407049bc): the size
 // includes the alternate stream when one exists. Passing 0 here under-sized the buffer
 // (live: written=4391 size=4262).
 int size=fn<int(*)(U,uint8_t)>(0x6f1ec0)(recorder,alternate!=0);
 if(size<=0){fn<void(*)(void*)>(0x3c12a0)(metadata);log("NATIVE_GHOST_ERROR: native serialized size invalid=%d",size);return;}
 U buffer=reinterpret_cast<U(*)(U,size_t,size_t)>(get<U>(get<U>(bufferAllocator)+0x50))(bufferAllocator,size,1);
 if(!buffer){fn<void(*)(void*)>(0x3c12a0)(metadata);log("NATIVE_GHOST_ERROR: native buffer allocation failed");return;}
 // IMPORTANT: 1406f2410 has FIVE arguments; original call stores alternate
 // metadata at [rsp+20]. Do not use the four-argument decompiler shortcut.
 int written=fn<int(*)(U,int,U,void*,U)>(0x6f2410)(buffer,size,recorder,metadata,alternate);
 U data{};
 if(written>0&&written<=size&&fn<uint8_t(*)(U)>(0x4e5440)(recorder)) {
  U allocation=fn<U(*)(size_t,size_t,U)>(0x1ebbcd0)(0x238,8,objectAllocator);
  if(allocation) {
   data=fn<U(*)(U,uint8_t)>(0x6f1bb0)(allocation,get<uint8_t>(flags+0xf3));
   fn<int(*)(U)>(0x1ebbfc0)(data+8);
   auto decoded=fn<uint8_t(*)(U,U,int)>(0x6f1f20)(data,buffer,written);
   log("NATIVE_REPLAY_DATA: native decode=%u object=0x%llX bytes=%d primary=%u secondary=%u refcount=%d",decoded,data,written,get<uint32_t>(data+0xd0),get<uint32_t>(data+0x224),get<int32_t>(data+8));
   if(decoded&&get<uint32_t>(data+0xd0)>0&&get<uint32_t>(data+0x224)==0) {
    uint64_t handle=UINT64_MAX;factoryActor=0;creating=true;
    fn<void(*)(uint64_t*,U,U,uint8_t)>(0x6f27f0)(&handle,nativeContext,data,0);
    creating=false;
    if(factoryActor) {
     auto actor=factoryActor;U entry=get<U>(actor+0x10);
     {std::lock_guard g(stateMutex);state.active=true;state.actor=actor;state.entry=entry;state.data=data;state.handle=handle;state.world=w;state.slot=uint32_t(handle)&0xfffff;state.manipulator=get<U>(actor+0x588);}
     observe(actor,"NATIVE_GHOST_CREATE");
     log("NATIVE_GHOST: OWNED epoch=%llu; F11 REMOVE ONCE; activation is native, gate132 never written by Theater",snapshot().epoch);
    } else log("NATIVE_GHOST_ERROR: native factory returned no actor handle=0x%llX",handle);
   } else log("NATIVE_GHOST_ERROR: decoded data invalid/secondary present; factory not called");
  } else log("NATIVE_GHOST_ERROR: native replay-data allocation failed");
 } else log("NATIVE_GHOST_ERROR: native serialization/eligibility failed written=%d size=%d",written,size);
 releaseTemporary(data);freeNative(buffer);fn<void(*)(void*)>(0x3c12a0)(metadata);
}
void onStep(U child,U context,float dt,U auxiliary) {
 const bool nativeCaller=reinterpret_cast<U>(_ReturnAddress())==base+0xb08422;
 bool old=localStep;localStep=nativeCaller;
 stepOriginal(child,context,dt,auxiliary);
 if(nativeCaller){stepCount.fetch_add(1);lastStepTick.store(GetTickCount64());timerBits.store(get<uint32_t>(child+0x38));}
 unsigned expected=1;
 // Use the SAME native TestNetStep dynamic context, after the original manager
 // update has completed. Do not wait for its periodic serialize/upload timer,
 // mutate that timer, cache context pointers, or invent a CSTask callback.
 // Require prior observation of the native 703f37 builder call in this process.
 if(nativeCaller&&contextSeen.load()&&std::isfinite(dt)&&dt>=0&&command.compare_exchange_strong(expected,0)) {
  log("NATIVE_GHOST_CREATE: one-shot consumed after original TestNetStep update; caller=140b0841d thread=%lu native_steps=%llu periodic_build_calls=%llu",GetCurrentThreadId(),stepCount.load(),buildCount.load());
  create(context);
 }
 localStep=old;
}
void onBuild(U child,U context) {
 const bool valid=localStep && reinterpret_cast<U>(_ReturnAddress())==base+0x703f3c;
 if(valid)buildCount.fetch_add(1);
 if(valid&&!contextSeen.exchange(true))log("NATIVE_GHOST: native local replay context observed at 140703f37; thread=%lu",GetCurrentThreadId());
 unsigned expected=1;
 if(valid&&command.compare_exchange_strong(expected,0)) {log("NATIVE_GHOST_CREATE: one-shot command consumed in original local native callsite");create(context);return;}
 buildOriginal(child,context);
}
U onSpawn(U w,uint32_t id,U data) {
 U actor=spawnOriginal(w,id,data);
 if(creating){factoryActor=actor;log("NATIVE_GHOST_CREATE: native world factory result=0x%llX",actor);}
 return actor;
}
U onActivate(U actor) {
 U result=activateOriginal(actor);
 if(creating||(snapshot().active&&snapshot().actor==actor))observe(actor,"NATIVE_GHOST_ACTIVATE");
 return result;
}
void onRemove(U w,U actor) {
 auto before=snapshot();bool ours=before.active&&before.actor==actor&&before.world==w;
 if(ours){observe(actor,"NATIVE_GHOST_REMOVE");retire(actor,"native-world-removal");}
 removeOriginal(w,actor);
 if(ours&&world()==w) {
  // World-owned entry array only. NO actor/refcount/retired handle access.
  auto cap=get<uint32_t>(w+0x10f48);auto entries=get<U>(w+0x10f50);
  U value{};bool readable=entries&&before.slot<cap&&read(entries+size_t(before.slot)*16,&value,8);
  log("NATIVE_GHOST_REMOVE: slot=%u world_entry_read=%u entry_actor_null=%u epoch=%llu",before.slot,readable,readable&&value==0,retiredEpoch.load());
 }
}
void onDrain(U w,U taskData) {
 unsigned expected=2;
 if(command.compare_exchange_strong(expected,0)) {
  auto s=snapshot();auto entries=get<U>(w+0x10f50);auto cap=get<uint32_t>(w+0x10f48);
  U entry=entries+size_t(s.slot)*16;
  if(s.active&&s.world==w&&world()==w&&entries&&s.slot<cap&&entry==s.entry&&get<U>(entry)==s.actor&&get<uint64_t>(s.actor+8)==s.handle) {
   // Actual native removal drain; same execution context as its own
   // 14050b340 calls. No removal from arbitrary ChrIns callbacks.
   onRemove(w,s.actor);
  } else {log("NATIVE_GHOST_ERROR: remove ownership/world/entry mismatch; no raw destruction attempted");if(s.active)retire(s.actor,"ownership-mismatch");}
 }
 drainOriginal(w,taskData);
}
U onDisable(U manip) {U result=disableOriginal(manip);if(retired(manip,retiredManipulator))log("NATIVE_GHOST_REMOVE: ReplayManipulator native disable returned epoch=%llu",retiredEpoch.load());return result;}
void onEnqueue(U manager,U actor) {bool ours=retired(actor,retiredActor);enqueueOriginal(manager,actor);if(ours)log("NATIVE_GHOST_DELAYDELETE: native enqueue returned epoch=%llu",retiredEpoch.load());}
U onGhostDestroy(U actor) {auto live=snapshot();if(live.active&&live.actor==actor){log("NATIVE_GHOST_ERROR: destructor reached before observed world removal; retiring epoch without cleanup fallback");retire(actor,"unexpected-direct-destruction");}bool ours=retired(actor,retiredActor);if(ours)log("NATIVE_GHOST_DESTROY: ReplayGhost destructor entered epoch=%llu",retiredEpoch.load());U r=ghostDestroyOriginal(actor);if(ours){log("NATIVE_GHOST_DESTROY: ReplayGhost destructor returned (no post-destroy dereference)");retiredActor.store(0);}return r;}
U onManipDestroy(U manip) {auto live=snapshot();if(live.active&&live.manipulator==manip){log("NATIVE_GHOST_ERROR: manipulator destroyed while runtime record active; retiring epoch");retire(live.actor,"unexpected-manipulator-destruction");}bool ours=retired(manip,retiredManipulator);if(ours)log("NATIVE_GHOST_DESTROY: ReplayManipulator destructor entered epoch=%llu",retiredEpoch.load());U r=manipDestroyOriginal(manip);if(ours){log("NATIVE_GHOST_DESTROY: ReplayManipulator destructor returned (no post-destroy dereference)");retiredManipulator.store(0);}return r;}
int32_t onRelease(U counter) {U identity=retiredData.load();bool ours=identity!=0&&counter==identity+8;int32_t old=releaseOriginal(counter);if(ours){log("NATIVE_REPLAY_DATA: native release old=%d new=%d final_release=%u epoch=%llu",old,old-1,old==1,retiredEpoch.load());if(old==1)retiredData.compare_exchange_strong(identity,0);}return old;}
void onDeleter(U deleter,U payload) {
 U actor=get<U>(payload);bool ours=retired(actor,retiredActor);
 if(ours)log("NATIVE_GHOST_DELAYDELETE: native actor deleter entered epoch=%llu",retiredEpoch.load());
 deleterOriginal(deleter,payload);
 if(ours)log("NATIVE_GHOST_DESTROY: native deleter returned; synchronous actor destructor+allocator-free path completed; no freed-pointer reads");
}
void keys() {
 bool lastCreate=false,lastRemove=false;
 for(;;) {
  DWORD pid{};GetWindowThreadProcessId(GetForegroundWindow(),&pid);bool foreground=pid==GetCurrentProcessId();
  bool c=foreground&&(GetAsyncKeyState(VK_F10)&0x8000),r=foreground&&(GetAsyncKeyState(VK_F11)&0x8000);
  auto live=snapshot();if(live.active&&world()!=live.world)retire(live.actor,"world-changed; observation only, no destruction request");
  if(c&&!lastCreate) {
   U player{},recorder{};auto s=snapshot();
   if(s.active||command.load())log("NATIVE_GHOST_ERROR: create rejected: active/pending command");
   else if(used.exchange(true))log("NATIVE_GHOST_ERROR: one create attempt per process; restart before another test");
   else if(!ready(world(),player,recorder))log("NATIVE_GHOST_ERROR: CREATE refused: player/recorder/world/block not ready; restart required before retry");
   else {requestStepCount.store(stepCount.load());deadline.store(GetTickCount64()+60000);command.store(1);log("NATIVE_GHOST_CREATE: REQUESTED; waiting for native TestNetStep (maximum 60s); context_seen=%u steps=%llu periodic_build_calls=%llu",contextSeen.load(),stepCount.load(),buildCount.load());}
  }
  if(r&&!lastRemove) {
   unsigned pending=1;
   if(command.compare_exchange_strong(pending,0))log("NATIVE_GHOST_REMOVE: pending create cancelled; no actor owned");
   else if(snapshot().active) {deadline.store(GetTickCount64()+60000);command.store(2);log("NATIVE_GHOST_REMOVE: command queued for native world removal drain");}
   else log("NATIVE_GHOST_ERROR: REMOVE refused: no active Theater-owned ghost");
  }
  if(command.load()&&GetTickCount64()>deadline.load()) {auto kind=command.exchange(0);uint32_t raw=timerBits.load();float timer;memcpy(&timer,&raw,4);if(kind)log("NATIVE_GHOST_ERROR: command=%u TIMEOUT; steps_since_request=%llu periodic_build_calls=%llu last_step_age_ms=%llu native_timer=%.3f context_seen=%u; no arbitrary callback mutation",kind,stepCount.load()-requestStepCount.load(),buildCount.load(),lastStepTick.load()?GetTickCount64()-lastStepTick.load():UINT64_MAX,timer,contextSeen.load());}
  U p{},rec{};if(!readyLogged.load()&&ready(world(),p,rec)&&!readyLogged.exchange(true))log("NATIVE_GHOST: PLAYER_RECORDER_READY; F10 CREATE ONCE, F11 REMOVE ONCE (game focus); existing payload/legacy write controls disabled in this feature");
  lastCreate=c;lastRemove=r;Sleep(25);
 }
}
}

extern "C" int tm_native_ghost_start(void(*logCallback)(const char*),const size_t* sdkLayout) {
 logger=logCallback;base=reinterpret_cast<U>(GetModuleHandleW(nullptr));memcpy(layout,sdkLayout,sizeof(layout));
 // Caller already passed the shared file/version/AMD64/SHA guard. Also reject
 // patched entrypoints before installing hooks; never scan another profile.
 for(const auto& p:nativeGhostFingerprints) {unsigned char bytes[16];if(!read(base+p.rva,bytes,sizeof(bytes))||memcmp(bytes,p.bytes,sizeof(bytes))){log("NATIVE_GHOST_ERROR: native entrypoint fingerprint mismatch RVA=0x%llX; feature OFF",p.rva);return 0;}}
 auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return 0;
 struct Hook{U rva;void* detour;void** original;};
 Hook hooks[]{
 {0x703e30,reinterpret_cast<void*>(onStep),reinterpret_cast<void**>(&stepOriginal)},
 {0x7048e0,reinterpret_cast<void*>(onBuild),reinterpret_cast<void**>(&buildOriginal)},
 {0x507e60,reinterpret_cast<void*>(onSpawn),reinterpret_cast<void**>(&spawnOriginal)},
 {0x50efa0,reinterpret_cast<void*>(onDrain),reinterpret_cast<void**>(&drainOriginal)},
 {0x50b340,reinterpret_cast<void*>(onRemove),reinterpret_cast<void**>(&removeOriginal)},
 {0x4f1c10,reinterpret_cast<void*>(onActivate),reinterpret_cast<void**>(&activateOriginal)},
 {0x3deec0,reinterpret_cast<void*>(onDisable),reinterpret_cast<void**>(&disableOriginal)},
 {0xe78ca0,reinterpret_cast<void*>(onEnqueue),reinterpret_cast<void**>(&enqueueOriginal)},
 {0x4f1ab0,reinterpret_cast<void*>(onGhostDestroy),reinterpret_cast<void**>(&ghostDestroyOriginal)},
 {0x3dec70,reinterpret_cast<void*>(onManipDestroy),reinterpret_cast<void**>(&manipDestroyOriginal)},
 {0x1ebc000,reinterpret_cast<void*>(onRelease),reinterpret_cast<void**>(&releaseOriginal)},
 {0xe775e0,reinterpret_cast<void*>(onDeleter),reinterpret_cast<void**>(&deleterOriginal)}};
 size_t created=0;for(auto& h:hooks){auto status=MH_CreateHook(reinterpret_cast<void*>(base+h.rva),h.detour,h.original);if(status!=MH_OK){log("NATIVE_GHOST_ERROR: hook create RVA=0x%llX status=%d",h.rva,status);break;}created++;}
 if(created!=std::size(hooks)){for(size_t i=0;i<created;i++)MH_RemoveHook(reinterpret_cast<void*>(base+hooks[i].rva));return 0;}
 for(auto& h:hooks)MH_QueueEnableHook(reinterpret_cast<void*>(base+h.rva));
 auto enabled=MH_ApplyQueued();if(enabled!=MH_OK){for(auto& h:hooks){MH_DisableHook(reinterpret_cast<void*>(base+h.rva));MH_RemoveHook(reinterpret_cast<void*>(base+h.rva));}log("NATIVE_GHOST_ERROR: hook enable failed=%d; feature OFF",enabled);return 0;}
 log("NATIVE_GHOST: experimental hooks installed; default OFF; exact SHA guard passed; one attempt per process; runtime validation REQUIRED");
 std::thread(keys).detach();return 1;
}

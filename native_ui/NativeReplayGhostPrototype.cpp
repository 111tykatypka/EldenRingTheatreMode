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
#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include "NativeGhostFingerprints.h"
#include "TheaterHotkeys.h"
extern "C" void tm_render_native_status(const char*);

namespace {
using U=uintptr_t;
U base{}, layout[12]{}; // [7] ChrIns::chr_model_ins, [8] modules.behavior, [9] modules.time_act, [10] modules.event, [11] CSChrEventModule::request_animation_id (SDK offsets)
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
// A new create is allowed only after the previous ghost finished the full native teardown
// (deleter returned and the replay data's final reference released). Any other ending keeps
// the one-attempt rule until restart.
std::atomic<bool> deleterDone{}, dataReleased{};
// Create gate. One ghost at a time; a new one only after the previous ghost finished the full
// native teardown. A create that produced no ghost is retried automatically (the game's own
// eligibility check often refuses until the recorder holds enough frames). Only a buffer
// overrun locks creation until restart.
std::atomic<bool> ghostCreated{}, overrunLock{};
std::atomic<ULONGLONG> retryAt{};
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
std::atomic<bool> overflowed{};
void createImpl(U nativeContext) {
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
     {std::lock_guard g(stateMutex);state.active=true;state.actor=actor;state.entry=entry;state.data=data;state.handle=handle;state.world=w;state.slot=uint32_t(handle)&0xfffff;state.manipulator=get<U>(actor+0x588);}ghostCreated.store(true);deleterDone.store(false);dataReleased.store(false);
     observe(actor,"NATIVE_GHOST_CREATE");
     log("NATIVE_GHOST: OWNED epoch=%llu; F11 REMOVE ONCE; activation is native, gate132 never written by Theater",snapshot().epoch);
    } else log("NATIVE_GHOST_ERROR: native factory returned no actor handle=0x%llX",handle);
   } else log("NATIVE_GHOST_ERROR: decoded data invalid/secondary present; factory not called");
  } else log("NATIVE_GHOST_ERROR: native replay-data allocation failed");
 } else {if(written>size)overflowed.store(true);log("NATIVE_GHOST_ERROR: native serialization/eligibility failed written=%d size=%d recorder_count40=%u recorder_count44=%u%s",written,size,get<uint32_t>(recorder+0x40),get<uint32_t>(recorder+0x44),written==0?"; nothing was written":"");}
 releaseTemporary(data);freeNative(buffer);fn<void(*)(void*)>(0x3c12a0)(metadata);
}
// A create that ended without a ghost and without writing past the native buffer left the game
// exactly where its own periodic serializer leaves it (it also skips on these results), so another
// F10 is allowed. Only a buffer overrun keeps the session locked.
void create(U nativeContext) {
 overflowed.store(false);createImpl(nativeContext);
 if(snapshot().active)return;
 if(overflowed.load()){overrunLock.store(true);log("NATIVE_GHOST_ERROR: buffer overrun detected; restart the game before another create");return;}
 if(GetTickCount64()+1000<deadline.load()){retryAt.store(GetTickCount64()+1000);log("NATIVE_GHOST: the game is not ready to make a ghost yet; retrying automatically every second (keep moving)");}
 else log("NATIVE_GHOST_ERROR: the game did not accept a ghost within 60 seconds; press F10 to try again");
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
int32_t onRelease(U counter) {U identity=retiredData.load();bool ours=identity!=0&&counter==identity+8;int32_t old=releaseOriginal(counter);if(ours){log("NATIVE_REPLAY_DATA: native release old=%d new=%d final_release=%u epoch=%llu",old,old-1,old==1,retiredEpoch.load());if(old==1){retiredData.compare_exchange_strong(identity,0);dataReleased.store(true);}}return old;}
void onDeleter(U deleter,U payload) {
 U actor=get<U>(payload);bool ours=retired(actor,retiredActor);
 if(ours)log("NATIVE_GHOST_DELAYDELETE: native actor deleter entered epoch=%llu",retiredEpoch.load());
 deleterOriginal(deleter,payload);
 if(ours){log("NATIVE_GHOST_DESTROY: native deleter returned; synchronous actor destructor+allocator-free path completed; no freed-pointer reads");deleterDone.store(true);}
}
// Read-only render diagnostic (shadow investigation): compares the owned ghost with the main
// player, word by word, in ChrIns and in the model item / display entity behind chr_model_ins.
// Words that look like pointers on either side are skipped. Logged once per ghost.
void logBlockDiff(const char* name,U ghost,U player,size_t bytes) {
 if(!ghost||!player){log("GHOST_RENDER_DIFF %s: missing pointer ghost=0x%llX player=0x%llX",name,ghost,player);return;}
 static unsigned char a[0x1000],b[0x1000];bytes=std::min(bytes,sizeof(a));
 if(!read(ghost,a,bytes)||!read(player,b,bytes)){log("GHOST_RENDER_DIFF %s: unreadable",name);return;}
 char line[1800];int n=snprintf(line,sizeof(line),"GHOST_RENDER_DIFF %s:",name);unsigned shown=0;
 auto pointerish=[](uint64_t v){return v>=0x10000000000ull&&v<0x800000000000ull;};
 for(size_t o=0;o+8<=bytes;o+=4){uint32_t ga,pa;memcpy(&ga,a+o,4);memcpy(&pa,b+o,4);if(ga==pa)continue;
  uint64_t g8,p8;memcpy(&g8,a+(o&~size_t(7)),8);memcpy(&p8,b+(o&~size_t(7)),8);if(pointerish(g8)||pointerish(p8))continue;
  if(n<int(sizeof(line))-40){n+=snprintf(line+n,sizeof(line)-n," +%zX:%08X/%08X",o,ga,pa);}
  if(++shown%60==0){log("%s",line);n=snprintf(line,sizeof(line),"GHOST_RENDER_DIFF %s (cont):",name);}}
 log("%s (ghost/player, %u words)",line,shown);}
// Shadow flags. The live diff (2026-10-06) showed three byte flags in CSFD4ModelItem that are 1
// on the player and 0 on the replay ghost, on every ghost: +0x6AC, +0x6AD, +0x6B6. The ghost's
// model item is given the player's values while the ghost is alive; the game may reset them, so
// this runs every 100 ms. Only bytes that are 0 on the ghost and 1 on the player are written.
bool writeByte(U address,uint8_t value){SIZE_T n{};return address>=0x10000&&WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,1,&n)&&n==1;}
void matchPlayerShadowFlags(U actor) {
 static unsigned long long loggedEpoch=~0ull;
 U w=world(),player{},recorder{};if(!ready(w,player,recorder))return;
 U gm=get<U>(actor+layout[7]),pm=get<U>(player+layout[7]);if(!gm||!pm)return;
 U gi=get<U>(gm+0x10),pi=get<U>(pm+0x10);if(!gi||!pi)return;
 static constexpr U flags[]{0x6AC,0x6AD,0x6B6};unsigned written=0;
 for(U o:flags){uint8_t g{},p{};if(read(gi+o,&g,1)&&read(pi+o,&p,1)&&g==0&&p==1&&writeByte(gi+o,1))++written;}
 // Draw-pass words of the model display entity. Live diff: +0x20 is 0x01 on the ghost and 0xBF on
 // the player (looks like a pass mask: the ghost draws in one pass only, no shadow pass), and
 // +0x94/+0x280 are 8 on the ghost and 6 on the player (looks like the translucent vs opaque
 // draw type). M0e copied only the model item flags and the ghost still had no shadow.
 U gd=get<U>(gm+0x18),pd=get<U>(pm+0x18);unsigned passWords=0;
 if(gd&&pd){static constexpr U words[]{0x20,0x94,0x280};
  for(U o:words){uint32_t g{},p{};if(read(gd+o,&g,4)&&read(pd+o,&p,4)&&g!=p){SIZE_T n{};if(WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(gd+o),&p,4,&n)&&n==4)++passWords;}}}
 const auto epoch=snapshot().epoch;
 if((written||passWords)&&loggedEpoch!=epoch){loggedEpoch=epoch;log("GHOST_RENDER: copied %u model item flag(s) and %u draw-pass word(s) from the player to the ghost (ModelDispEntity +0x20/+0x94/+0x280)",written,passWords);}
}
void renderDiff(U actor) {
 U w=world(),player{},recorder{};if(!ready(w,player,recorder)){log("GHOST_RENDER_DIFF: player not ready");return;}
 U gm=get<U>(actor+layout[7]),pm=get<U>(player+layout[7]);
 logBlockDiff("ChrIns",actor,player,0x540);
 logBlockDiff("CSChrModelIns",gm,pm,0x28);
 logBlockDiff("CSFD4ModelItem",gm?get<U>(gm+0x10):0,pm?get<U>(pm+0x10):0,0x6d0);
 logBlockDiff("ModelDispEntity",gm?get<U>(gm+0x18):0,pm?get<U>(pm+0x18):0,0x400);}
// ---------------------------------------------------------------------------
// Animation probe (replay system Step 1, spike 1). READ-ONLY.
// Maps the player's Havok animation objects by walking pointers from the behavior and TimeAct
// modules and naming each object by its MSVC RTTI, then samples their first words for 1.5 s so
// the local-time / weight / speed fields can be identified. Nothing is written.
// ---------------------------------------------------------------------------
std::string rttiName(U object) {
 U vtable{};if(!read(object,&vtable,8)||vtable<0x10000)return {};
 U col{};if(!read(vtable-8,&col,8)||col<0x10000)return {};
 uint32_t signature{},typeRva{},selfRva{};
 if(!read(col,&signature,4)||signature!=1||!read(col+0xC,&typeRva,4)||!read(col+0x14,&selfRva,4))return {};
 const U image=col-selfRva;char name[96]{};
 if(!read(image+typeRva+0x10,name,sizeof(name)-1)||strncmp(name,".?AV",4))return {};
 std::string n(name+4);const auto at=n.find("@@");return at==std::string::npos?n:n.substr(0,at);
}
bool pointerish(U v){return v>=0x10000000000ull&&v<0x800000000000ull&&(v&7)==0;}
struct ProbeNode{U address;std::string name;std::string path;};
// Probe 4: collect every hkbClipGenerator reachable from the character's behavior graph
// (breadth first through Havok objects and hkArray storage), sample them for 1.5 s, and report
// the ones whose values change (the active clips) with their animation name and changing fields.
std::string asciiAt(U address){char t[48]{};if(!read(address,t,sizeof(t)-1))return {};for(char&c:t){if(!c)break;if(c<32||c>126)return {};}return t;}
std::string clipName(U clip){for(U o=0x30;o<0x100;o+=8){U p=get<U>(clip+o);if(!pointerish(p))continue;auto t=asciiAt(p);if(t.size()>=4&&t[0]=='a'&&isdigit((unsigned char)t[1])){char b[80];snprintf(b,sizeof(b),"+%llX:%s",o,t.c_str());return b;}}return {};}
void animProbe(U player,U modulesOffset,U behaviorOffset,U timeActOffset) {
 tm_render_native_status("ANIM PROBE (F9): running, keep doing what you're doing for 2 seconds..."); // temporary, removed after the spike
 U modules=get<U>(player+modulesOffset);
 U behavior=get<U>(modules+behaviorOffset),timeAct=get<U>(modules+timeActOffset);
 U holder=get<U>(behavior+0x10),character=holder?get<U>(holder+0x30):0,graph=character?get<U>(character+0x98):0;
 log("ANIM_PROBE: start player=0x%llX hkbCharacter=0x%llX graph=0x%llX(%s)",player,character,graph,rttiName(graph).c_str());
 std::vector<U> queue{graph},seen{graph},clips;
 for(size_t qi=0;qi<queue.size()&&queue.size()<6000;++qi){
  const U object=queue[qi];unsigned char b[0x300];if(!read(object,b,sizeof(b)))continue;
  for(size_t o=8;o+8<=sizeof(b);o+=8){U v;memcpy(&v,b+o,8);if(!pointerish(v))continue;
   auto consider=[&](U q){if(std::find(seen.begin(),seen.end(),q)!=seen.end())return;const auto n=rttiName(q);if(n.rfind("hk",0)!=0&&n.find("Custom")==std::string::npos)return;
    seen.push_back(q);if(n=="hkbClipGenerator")clips.push_back(q);if(n.rfind("hkb",0)==0||n.find("Custom")!=std::string::npos||n.find("@hkbStateMachine")!=std::string::npos)queue.push_back(q);};
   const auto n=rttiName(v);
   if(!n.empty()){consider(v);continue;}
   // hkArray storage of object pointers
   U items[16];if(!read(v,items,sizeof(items)))continue;for(U it:items)if(pointerish(it))consider(it);
  }}
 log("ANIM_PROBE: %zu Havok objects visited, %zu hkbClipGenerator found",seen.size(),clips.size());
 constexpr int samples=30;constexpr size_t words=0x400/4; // whole clip generator incl. internal state
 std::vector<std::vector<float>> series(clips.size(),std::vector<float>(samples*words));
 std::vector<std::array<float,4>> tae(samples);
 for(int i=0;i<samples;++i){
  for(size_t c=0;c<clips.size();++c)read(clips[c],series[c].data()+i*words,words*4);
  const uint32_t idx=get<uint32_t>(timeAct+0x20+10*16+4);const U slot=timeAct+0x20+(idx%10)*16;
  tae[i]={float(get<int32_t>(slot)),get<float>(slot+4),get<float>(slot+8),get<float>(slot+12)};Sleep(50);}
 char line[1900];
 {int n=snprintf(line,sizeof(line),"ANIM_PROBE: time_act[read] id/play/len:");
  for(int i=0;i<samples&&n<int(sizeof(line))-40;++i)n+=snprintf(line+n,sizeof(line)-n," %.0f/%.3f/%.3f",tae[i][0],tae[i][1],tae[i][3]);log("%s",line);}
 unsigned active=0;
 for(size_t c=0;c<clips.size()&&active<16;++c){
  bool any=false;for(size_t w=0;w<words&&!any;++w)for(int i=1;i<samples;++i){float a=series[c][w],v=series[c][i*words+w];if(std::isfinite(a)&&std::isfinite(v)&&v!=a&&std::fabs(v)<1e6f){any=true;break;}}
  if(!any)continue;++active;
  log("ANIM_PROBE: ACTIVE clip @0x%llX name %s",clips[c],clipName(clips[c]).c_str());
  for(size_t w=0;w<words;++w){float first=series[c][w];bool changes=false,ok=true;
   for(int i=0;i<samples;++i){float v=series[c][i*words+w];if(!std::isfinite(v)||std::fabs(v)>1e6f){ok=false;break;}if(v!=first)changes=true;}
   if(!ok||!changes)continue;
   int up=0;float lo=1e9f,hi=-1e9f;for(int i=0;i<samples;++i){float v=series[c][i*words+w];lo=std::min(lo,v);hi=std::max(hi,v);if(i&&v>series[c][(i-1)*words+w])++up;}
   const bool timeLike=lo>=-0.001f&&hi<=30.f&&up>=samples/3;
   if(!timeLike)continue;int n=snprintf(line,sizeof(line),"ANIM_PROBE:   TIME? +%zX:",w*4);
   for(int i=0;i<samples&&n<int(sizeof(line))-16;++i)n+=snprintf(line+n,sizeof(line)-n," %.3f",series[c][i*words+w]);log("%s",line);}
  // Static words (first sample) for field identification: hex, 0x30..0x140
  int n=snprintf(line,sizeof(line),"ANIM_PROBE:   static:");for(size_t w=0x140/4;w<0x240/4&&n<int(sizeof(line))-24;++w){uint32_t u;memcpy(&u,&series[c][w],4);n+=snprintf(line+n,sizeof(line)-n," %zX=%08X",w*4,u);}log("%s",line);
 }
 log("ANIM_PROBE: done; %u active clips",active);
 {char hud[160];snprintf(hud,sizeof(hud),"ANIM PROBE (F9): done, %zu clips found, %u active. You can press F9 again.",clips.size(),active);tm_render_native_status(hud);}
 (void)behaviorOffset;
}
// Spike 2 (first WRITE test): freeze the player's active animation clips for 2 s by writing
// each active hkbClipGenerator's local time (+0x140, identified by probe 5) back to the value it
// had when F9 was pressed. If the pose freezes while the game keeps running, writing the clip
// time controls the pose, which is what the replay puppet driver needs. Only float times are
// written, only to clips that were active, and writing stops after 2 s.
void animFreezeTest(U player,U modulesOffset,U behaviorOffset) {
 tm_render_native_status("ANIM TEST (F9): freezing your pose for 2 seconds (keep moving to see it)...");
 U modules=get<U>(player+modulesOffset),behavior=get<U>(modules+behaviorOffset);
 U holder=get<U>(behavior+0x10),character=holder?get<U>(holder+0x30):0,graph=character?get<U>(character+0x98):0;
 if(!graph||rttiName(graph)!="hkbBehaviorGraph"){log("ANIM_TEST: behavior graph not found");tm_render_native_status("ANIM TEST (F9): behavior graph not found");return;}
 std::vector<U> queue{graph},seen{graph},clips;
 for(size_t qi=0;qi<queue.size()&&queue.size()<6000;++qi){
  const U object=queue[qi];unsigned char b[0x300];if(!read(object,b,sizeof(b)))continue;
  for(size_t o=8;o+8<=sizeof(b);o+=8){U v;memcpy(&v,b+o,8);if(!pointerish(v))continue;
   auto consider=[&](U q){if(std::find(seen.begin(),seen.end(),q)!=seen.end())return;const auto n=rttiName(q);if(n.rfind("hk",0)!=0&&n.find("Custom")==std::string::npos)return;
    seen.push_back(q);if(n=="hkbClipGenerator")clips.push_back(q);if(n.rfind("hkb",0)==0||n.find("Custom")!=std::string::npos||n.find("@hkbStateMachine")!=std::string::npos)queue.push_back(q);};
   if(!rttiName(v).empty()){consider(v);continue;}
   U items[16];if(!read(v,items,sizeof(items)))continue;for(U it:items)if(pointerish(it))consider(it);}}
 // Active = local time advances over 100 ms.
 std::vector<float> before(clips.size());for(size_t i=0;i<clips.size();++i)before[i]=get<float>(clips[i]+0x140);
 Sleep(100);
 std::vector<std::pair<U,float>> active;
 for(size_t i=0;i<clips.size();++i){const float now=get<float>(clips[i]+0x140);if(std::isfinite(now)&&now!=before[i]&&now>=0&&now<100)active.push_back({clips[i],now});}
 log("ANIM_TEST: %zu clips, %zu active; freezing their local time for 2 s",clips.size(),active.size());
 const ULONGLONG until=GetTickCount64()+2000;unsigned long long writes=0,drift=0;
 while(GetTickCount64()<until){
  for(const auto& [clip,t]:active){float cur=get<float>(clip+0x140);if(cur!=t){++drift;SIZE_T n{};WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(clip+0x140),&t,4,&n);++writes;}}
  Sleep(0);}
 log("ANIM_TEST: done; %llu corrections written (the game advanced the time %llu times)",writes,drift);
 char hud[200];snprintf(hud,sizeof(hud),"ANIM TEST (F9): done. %zu active clips held for 2 s. Did your pose freeze?",active.size());tm_render_native_status(hud);
}
// Spike 3 (WRITE test): ask the player's own character to play animation 22100 (the roll the
// TimeAct module reported in spike 1) through CSChrEventModule.request_animation_id, then log
// what TimeAct plays for 1.5 s. If the character rolls in place, TAE IDs can start animations,
// which together with spike 2 (clip time) is the puppet driver.
void animRequestTest(U player) {
 tm_render_native_status("ANIM TEST (F9): requesting animation 60100 (prayer)...");
 U modules=get<U>(player+layout[1]),event=get<U>(modules+layout[10]),timeAct=get<U>(modules+layout[9]);
 if(!event||!timeAct){log("ANIM_TEST: event/time_act module missing");return;}
 const int32_t before=get<int32_t>(event+layout[11]);const int32_t id=60100; /* 22100 (roll) was consumed but ignored; 60100 is an event animation community tools force this way */SIZE_T n{};
 WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(event+layout[11]),&id,4,&n);
 log("ANIM_TEST: request_animation_id %d -> %d (written=%zu)",before,id,size_t(n));
 char line[1800];int len=snprintf(line,sizeof(line),"ANIM_TEST: time_act after request id/time:");
 for(int i=0;i<30;++i){const uint32_t idx=get<uint32_t>(timeAct+0x20+10*16+4);const U slot=timeAct+0x20+(idx%10)*16;
  len+=snprintf(line+len,sizeof(line)-len," %d/%.3f",get<int32_t>(slot),get<float>(slot+4));
  if(i==2)len+=snprintf(line+len,sizeof(line)-len," [req now %d]",get<int32_t>(event+layout[11]));Sleep(50);}
 log("%s",line);
 tm_render_native_status("ANIM TEST (F9): done. Did your character kneel and pray?");
}
// Probe 6 (spike 3 confirmed the override plays event animations such as 60100): request 60100,
// then compare the active event clip with the roll clip (a000_022100) and two more clips, to find
// which field selects the animation asset (binding index). Read-only apart from the same 60100
// request already tested.
std::vector<U> collectClips(U graph){
 std::vector<U> queue{graph},seen{graph},clips;
 for(size_t qi=0;qi<queue.size()&&queue.size()<6000;++qi){
  const U object=queue[qi];unsigned char b[0x300];if(!read(object,b,sizeof(b)))continue;
  for(size_t o=8;o+8<=sizeof(b);o+=8){U v;memcpy(&v,b+o,8);if(!pointerish(v))continue;
   auto consider=[&](U q){if(std::find(seen.begin(),seen.end(),q)!=seen.end())return;const auto n=rttiName(q);if(n.rfind("hk",0)!=0&&n.find("Custom")==std::string::npos)return;
    seen.push_back(q);if(n=="hkbClipGenerator")clips.push_back(q);if(n.rfind("hkb",0)==0||n.find("Custom")!=std::string::npos||n.find("@hkbStateMachine")!=std::string::npos)queue.push_back(q);};
   if(!rttiName(v).empty()){consider(v);continue;}
   U items[16];if(!read(v,items,sizeof(items)))continue;for(U it:items)if(pointerish(it))consider(it);}}
 return clips;}
std::string clipTitle(U clip){U p=get<U>(clip+0x48);return pointerish(p)?asciiAt(p):std::string();}
void dumpClip(const char* label,U clip){
 unsigned char b[0x200]{};read(clip,b,sizeof(b));char line[1900];
 int n=snprintf(line,sizeof(line),"ANIM_PROBE6: %s @0x%llX name=%s ints:",label,clip,clipTitle(clip).c_str());
 for(size_t o=0x30;o<sizeof(b)&&n<int(sizeof(line))-24;o+=4){uint32_t u;memcpy(&u,b+o,4);n+=snprintf(line+n,sizeof(line)-n," %zX=%X",o,u);}
 log("%s",line);}
void animProbe6(U player){
 tm_render_native_status("ANIM PROBE 6 (F9): praying, then comparing clips...");
 U modules=get<U>(player+layout[1]),behavior=get<U>(modules+layout[8]),event=get<U>(modules+layout[10]);
 U holder=get<U>(behavior+0x10),character=holder?get<U>(holder+0x30):0,graph=character?get<U>(character+0x98):0;
 if(!graph||!event){log("ANIM_PROBE6: graph/event missing");return;}
 // The prayer clip did not exist before the request in the first run, so collect AFTER it starts.
 const int32_t id=60100;SIZE_T w{};WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(event+layout[11]),&id,4,&w);
 Sleep(500);
 auto clips=collectClips(graph);
 std::vector<float> t0(clips.size());for(size_t i=0;i<clips.size();++i)t0[i]=get<float>(clips[i]+0x140);
 Sleep(150);
 unsigned shown=0;
 for(size_t i=0;i<clips.size()&&shown<12;++i){const float t=get<float>(clips[i]+0x140);if(t!=t0[i]&&std::isfinite(t)){dumpClip("ACTIVE during prayer",clips[i]);++shown;}}
 log("ANIM_PROBE6: %u active clips during the prayer",shown);
 shown=0;for(U c:clips){const auto name=clipTitle(c);if(name.find("022100")!=std::string::npos&&shown<3){dumpClip("ROLL",c);++shown;}}
 shown=0;for(U c:clips){const auto name=clipTitle(c);if(name.find("060000")!=std::string::npos&&shown<2){dumpClip("DOOR",c);++shown;}}
 // the binding set: hkbCharacter+0x90 setup -> +0x40 hkbAnimationBindingSet -> +0x18 bindings array
 U setup=get<U>(character+0x90),bindingSet=setup?get<U>(setup+0x40):0;
 log("ANIM_PROBE6: %zu clips; setup=0x%llX bindingSet=0x%llX(%s) arr=0x%llX size=%d",clips.size(),setup,bindingSet,rttiName(bindingSet).c_str(),get<U>(bindingSet+0x18),get<int32_t>(bindingSet+0x20));
 tm_render_native_status("ANIM PROBE 6 (F9): done.");
}
// Spike 4: probe 6b found the active prayer clip "a000_060100" with +0x50 = 0x04021B88 while other
// clips show 0x0402xxxx; the low 16 bits look like the animation binding index. Verify that with
// the character's animation name table, then try to make the prayer clip play the roll instead
// (write the roll's index, hold its local time near 0.3 s for 1.5 s). If the pose turns into a roll,
// one event clip can play any of the character's animations, which is the general puppet driver.
// Find the hkArray<hkStringPtr> of animation names: the largest string array in hkbCharacterStringData.
struct NameTable{U data{};int32_t size{};};
NameTable findAnimationNames(U character){
 U setup=get<U>(character+0x90),charData=setup?get<U>(setup+0x48):0,strings=charData?get<U>(charData+0x98):0;
 log("SPIKE4: setup=0x%llX(%s) charData=0x%llX(%s) strings=0x%llX(%s)",setup,rttiName(setup).c_str(),charData,rttiName(charData).c_str(),strings,rttiName(strings).c_str());
 NameTable best;if(!strings){log("SPIKE4: hkbCharacterStringData missing");return best;}
 for(U o=0x10;o<0x100;o+=0x10){U data=get<U>(strings+o);int32_t size=get<int32_t>(strings+o+8);
  log("SPIKE4: strings+%llX data=0x%llX size=%d",o,data,size);
  if(!pointerish(data)||size<=0||size>100000)continue;U first=get<U>(data)&~U(1);const auto t=pointerish(first)?asciiAt(first):std::string();
  log("SPIKE4: string array +%llX size=%d first=%s",o,size,t.c_str());if(size>best.size&&!t.empty())best={data,size};}
 return best;}
int32_t findName(const NameTable& names,const char* needle){
 for(int32_t i=0;i<names.size;++i){U p=get<U>(names.data+size_t(i)*8)&~U(1);if(!pointerish(p))continue;if(asciiAt(p).find(needle)!=std::string::npos)return i;}return -1;}
void animSpike4(U player){
 tm_render_native_status("ANIM TEST 4 (F9): praying, then switching the prayer clip to the walk animation...");
 U modules=get<U>(player+layout[1]),behavior=get<U>(modules+layout[8]),event=get<U>(modules+layout[10]);
 U holder=get<U>(behavior+0x10),character=holder?get<U>(holder+0x30):0,graph=character?get<U>(character+0x98):0;
 if(!graph||!event){log("SPIKE4: graph/event missing");return;}
 const auto names=findAnimationNames(character);
 const int32_t prayIndex=findName(names,"060100"),rollIndex=findName(names,"022100");
 log("SPIKE4: name table size=%d; a000_060100 index=%d (0x%X); a000_022100 index=%d (0x%X)",names.size,prayIndex,prayIndex,rollIndex,rollIndex);
 const int32_t id=60100;SIZE_T w{};WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(event+layout[11]),&id,4,&w);
 Sleep(400);
 U prayer=0;for(U c:collectClips(graph))if(clipTitle(c).find("060100")!=std::string::npos){prayer=c;break;}
 if(!prayer){log("SPIKE4: prayer clip not found");tm_render_native_status("ANIM TEST 4 (F9): prayer clip not found");return;}
 const uint16_t was=get<uint16_t>(prayer+0x50);
 log("SPIKE4: prayer clip @0x%llX +50 low16=0x%X (matches name table: %s)",prayer,was,was==uint16_t(prayIndex)?"YES":"no");
 // The name table is empty in this build (FromSoftware strips hkbCharacterStringData), so swap to an
 // index observed directly in probe 6b instead: 0x0045 belonged to the active walk clip
 // a000_002000. If the prayer pose turns into a walk pose, the index field selects the animation.
 (void)rollIndex;const uint16_t roll=0x0045;SIZE_T n{};
 WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(prayer+0x50),&roll,2,&n);
 log("SPIKE4: wrote roll index 0x%X into the prayer clip; holding local time 0.3 s for 1.5 s",roll);
 const float t=0.3f;const ULONGLONG until=GetTickCount64()+1500;
 while(GetTickCount64()<until){WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(prayer+0x140),&t,4,&n);Sleep(1);}
 WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(prayer+0x50),&was,2,&n); // restore
 log("SPIKE4: restored the prayer index");
 tm_render_native_status("ANIM TEST 4 (F9): done. Did the prayer turn into a frozen walking pose?");
}
// Spike 5: start a move through the behavior graph's own event entry point, the method public
// tools use ("PlayAnimation": hkbCharacter + event name such as W_BackStep). The function is found
// by the same byte pattern those tools use (74 ?? 48 85 D2 74 ?? 48 8D 4C 24 50, entry = match-0xD)
// and is only called if exactly one match exists in the game image.
U findBehaviorEventFunction(){
 static U cached=~U(0);if(cached!=~U(0))return cached;cached=0;
 auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
 auto section=IMAGE_FIRST_SECTION(nt);
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i,++section){
  if(memcmp(section->Name,".text",5))continue;
  const unsigned char* begin=reinterpret_cast<const unsigned char*>(base+section->VirtualAddress);const size_t size=section->Misc.VirtualSize;
  unsigned matches=0;U hit=0;
  for(size_t k=0;k+12<=size;++k){const unsigned char*q=begin+k;
   if(q[0]==0x74&&q[2]==0x48&&q[3]==0x85&&q[4]==0xD2&&q[5]==0x74&&q[7]==0x48&&q[8]==0x8D&&q[9]==0x4C&&q[10]==0x24&&q[11]==0x50){++matches;hit=reinterpret_cast<U>(q)-0xD;}}
  log("SPIKE5: event function pattern matches=%u entry=0x%llX (rva 0x%llX)",matches,hit,hit?hit-base:0);
  if(matches==1)cached=hit;}
 return cached;}
void animSpike5(U player){
 tm_render_native_status("ANIM TEST 5 (F9): asking the animation graph for a back step (W_BackStep)...");
 U modules=get<U>(player+layout[1]),behavior=get<U>(modules+layout[8]),timeAct=get<U>(modules+layout[9]);
 U holder=get<U>(behavior+0x10),character=holder?get<U>(holder+0x30):0;
 if(!character||rttiName(character)!="hkbCharacter"){log("SPIKE5: hkbCharacter missing");return;}
 const U fn=findBehaviorEventFunction();
 if(!fn){tm_render_native_status("ANIM TEST 5 (F9): event function not found uniquely, nothing called");return;}
 // The public tool writes the name as UTF-16 (writeString(..., true)); the narrow string returned -1.
 static const wchar_t name[]=L"W_BackStep";
 const auto result=reinterpret_cast<uint32_t(*)(U,const wchar_t*)>(fn)(character,name);
 log("SPIKE5: W_BackStep returned 0x%X",result);
 char line[900];int len=snprintf(line,sizeof(line),"SPIKE5: time_act after event id/time:");
 for(int i=0;i<20;++i){const uint32_t idx=get<uint32_t>(timeAct+0x20+10*16+4);const U slot=timeAct+0x20+(idx%10)*16;
  len+=snprintf(line+len,sizeof(line)-len," %d/%.3f",get<int32_t>(slot),get<float>(slot+4));Sleep(50);}
 log("%s",line);
 tm_render_native_status(result==0xFFFFFFFFu?"ANIM TEST 5 (F9): the game rejected W_BackStep":"ANIM TEST 5 (F9): done. Did your character back step by itself?");
}
std::atomic<bool> probeRunning{};
void keys() {
 bool lastCreate=false,lastRemove=false;
 for(;;) {
  DWORD pid{};GetWindowThreadProcessId(GetForegroundWindow(),&pid);bool foreground=pid==GetCurrentProcessId();
  using theater_hotkeys::Action;const int createKey=int(theater_hotkeys::Key(Action::GhostCreateTest)),removeKey=int(theater_hotkeys::Key(Action::GhostRemoveTest));
  bool c=foreground&&(GetAsyncKeyState(createKey)&0x8000),r=foreground&&(GetAsyncKeyState(removeKey)&0x8000);
  auto live=snapshot();if(live.active&&world()!=live.world)retire(live.actor,"world-changed; observation only, no destruction request");
  {static unsigned long long diffEpoch=~0ull;static ULONGLONG ownedAt=0;
   if(live.active&&live.epoch!=diffEpoch){if(!ownedAt)ownedAt=GetTickCount64();else if(GetTickCount64()-ownedAt>1000){diffEpoch=live.epoch;ownedAt=0;renderDiff(live.actor);}}
   else if(!live.active)ownedAt=0;}
  {static ULONGLONG lastShadow=0;if(live.active&&GetTickCount64()-lastShadow>=100){lastShadow=GetTickCount64();matchPlayerShadowFlags(live.actor);}}
  if(c&&!lastCreate) {
   U player{},recorder{};auto s=snapshot();
   if(s.active||command.load())log("NATIVE_GHOST_ERROR: create rejected: active/pending command");
   else if(overrunLock.load())log("NATIVE_GHOST_ERROR: a buffer overrun happened earlier; restart the game before another create");
   else if(ghostCreated.load()&&!(deleterDone.load()&&dataReleased.load()))log("NATIVE_GHOST_ERROR: previous ghost has not finished its native cleanup yet; wait for it to disappear");
   else if(!ready(world(),player,recorder))log("NATIVE_GHOST_ERROR: CREATE refused: player/recorder/world/block not ready; restart required before retry");
   else {used.store(true);retryAt.store(0);requestStepCount.store(stepCount.load());deadline.store(GetTickCount64()+60000);command.store(1);log("NATIVE_GHOST_CREATE: REQUESTED; waiting for native TestNetStep (maximum 60s); context_seen=%u steps=%llu periodic_build_calls=%llu",contextSeen.load(),stepCount.load(),buildCount.load());}
  }
  {static bool lastProbe=false;const bool probe=foreground&&(GetAsyncKeyState(int(theater_hotkeys::Key(theater_hotkeys::Action::AnimProbe)))&0x8000);
   if(probe&&!lastProbe&&!probeRunning.exchange(true)){
    std::thread([]{U w=world(),player{},recorder{};
     (void)w;(void)player;(void)recorder;log("ANIM_PROBE: F9 test retired (spike 5b crashed the game)");tm_render_native_status("F9 test is switched off");
     probeRunning=false;}).detach();}
   lastProbe=probe;}
  if(r&&!lastRemove) {
   unsigned pending=1;
   if(command.compare_exchange_strong(pending,0)||retryAt.exchange(0)){retryAt.store(0);log("NATIVE_GHOST_REMOVE: pending create cancelled; no actor owned");}
   else if(snapshot().active) {deadline.store(GetTickCount64()+60000);command.store(2);log("NATIVE_GHOST_REMOVE: command queued for native world removal drain");}
   else log("NATIVE_GHOST_ERROR: REMOVE refused: no active Theater-owned ghost");
  }
  {const ULONGLONG at=retryAt.load();if(at&&GetTickCount64()>=at&&!command.load()&&!snapshot().active&&[&]{ULONGLONG expected=at;return retryAt.compare_exchange_strong(expected,0);}()){
    if(GetTickCount64()<deadline.load())command.store(1);}}
  if(command.load()&&GetTickCount64()>deadline.load()) {retryAt.store(0);auto kind=command.exchange(0);uint32_t raw=timerBits.load();float timer;memcpy(&timer,&raw,4);if(kind)log("NATIVE_GHOST_ERROR: command=%u TIMEOUT; steps_since_request=%llu periodic_build_calls=%llu last_step_age_ms=%llu native_timer=%.3f context_seen=%u; no arbitrary callback mutation",kind,stepCount.load()-requestStepCount.load(),buildCount.load(),lastStepTick.load()?GetTickCount64()-lastStepTick.load():UINT64_MAX,timer,contextSeen.load());}
  U p{},rec{};if(!readyLogged.load()&&ready(world(),p,rec)&&!readyLogged.exchange(true))log("NATIVE_GHOST: PLAYER_RECORDER_READY; F10 create, F11 remove; F10 again after the previous ghost is gone");
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
 log("NATIVE_GHOST: experimental hooks installed; default OFF; exact SHA guard passed; one ghost at a time");
 std::thread(keys).detach();return 1;
}

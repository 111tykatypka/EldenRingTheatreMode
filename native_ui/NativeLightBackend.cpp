// Independent binding to exact-target native light APIs. Draw_Pre game task only.
#include "NativeLightBackend.h"
#include "LightEditor.h"
#include "GameProfile.h"
#include "CinematicCameraRuntime.h"
#include <windows.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
namespace native_lights { namespace {
std::atomic_bool armed=false,linked=false;
std::atomic_bool shadow_requested=false;
bool shadow_verified=false;
std::atomic<ULONGLONG> heartbeat=0;
std::mutex telemetry;
View state;
struct Owned {std::uint64_t editor_id;void* body;std::uintptr_t manager;light_editor::Type type;};
std::vector<Owned> owned; // Strong native refs; accessed only on Draw_Pre.
std::uintptr_t base=0;
bool checked=false,verified=false;
std::atomic_bool faulted=false;
ULONGLONG last_update=0;
std::uint64_t created=0,removed=0,lock_skips=0;
using PointFactory=void*(*)(void*,const float*,bool,float);
using SpotFactory=void*(*)(void*,bool,float);
using Retain=int(*)(void*);
using Release=void(*)(void*,void*);
using TryLock=int(*)(void*);
using Unlock=int(*)(void*);
using PointSet=void(*)(void*,const float*);
using SpotMatrix=void(*)(void*,const float*);
using SpotShape=void(*)(void*,float,float,float,float);
using Update=void(*)(void*,float);
std::atomic_flag in_tick=ATOMIC_FLAG_INIT;
struct TickGuard {~TickGuard(){in_tick.clear(std::memory_order_release);}};
template<class Fn> Fn api(std::uintptr_t rva){return reinterpret_cast<Fn>(base+rva);}
bool read(std::uintptr_t p,void* dst,std::size_t n){SIZE_T got=0;return p>=0x10000&&p<=0x7fffffffffffULL-n&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),dst,n,&got)&&got==n;}
template<class T>bool read(std::uintptr_t p,T& value){return read(p,&value,sizeof(value));}
template<std::size_t N>bool bytes(std::uintptr_t rva,const unsigned char (&expected)[N]){unsigned char actual[N]{};return read(base+rva,actual,N)&&!memcmp(actual,expected,N);}
void report(const char* message){
 std::unique_lock lock(telemetry,std::try_to_lock);if(!lock.owns_lock())return;
 state.available=verified;state.faulted=faulted;state.rendered=unsigned(owned.size());state.created=created;state.removed=removed;state.lock_skips=lock_skips;
 state.shadow_available=shadow_verified;state.submitted_ids.clear();for(const auto& o:owned)state.submitted_ids.push_back(o.editor_id);
 if(state.status==message)return;state.status=message;
 wchar_t directory[MAX_PATH]{};if(GetTempPathW(MAX_PATH,directory)){std::ofstream f(std::filesystem::path(directory)/L"TheaterModeGame.log",std::ios::app);f<<"NATIVE_LIGHTS "<<message<<" live="<<owned.size()<<" created="<<created<<" removed="<<removed<<'\n';}
}
bool guard(){
 if(checked)return verified;checked=true;base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
 constexpr unsigned char pf[]=TM_LIGHT_GUARD_POINT_FACTORY,sf[]=TM_LIGHT_GUARD_SPOT_FACTORY,ps[]=TM_LIGHT_GUARD_POINT_SET,sm[]=TM_LIGHT_GUARD_SPOT_MATRIX,ss[]=TM_LIGHT_GUARD_SPOT_SHAPE,retain[]=TM_LIGHT_GUARD_RETAIN,release[]=TM_LIGHT_GUARD_REMOVE_RELEASE,tl[]=TM_LIGHT_GUARD_TRYLOCK,ul[]=TM_LIGHT_GUARD_UNLOCK,pu[]=TM_LIGHT_GUARD_POINT_LIGHT_UPDATE,su[]=TM_LIGHT_GUARD_SPOT_LIGHT_UPDATE;
 verified=base&&bytes(TM_VAL_POINT_LIGHT_FACTORY,pf)&&bytes(TM_VAL_SPOT_LIGHT_FACTORY,sf)&&bytes(TM_VAL_POINT_LIGHT_SET_POSITION,ps)&&bytes(TM_VAL_SPOT_LIGHT_SET_MATRIX,sm)&&bytes(TM_VAL_SPOT_LIGHT_SET_SHAPE,ss)&&bytes(TM_VAL_LIGHT_RETAIN,retain)&&bytes(TM_VAL_LIGHT_REMOVE_RELEASE,release)&&bytes(TM_VAL_LIGHT_TRY_LOCK,tl)&&bytes(TM_VAL_LIGHT_UNLOCK,ul)&&bytes(TM_VAL_POINT_LIGHT_UPDATE,pu)&&bytes(TM_VAL_SPOT_LIGHT_UPDATE,su);
 constexpr unsigned char pointShadow[]=TM_LIGHT_GUARD_POINT_SHADOW_CHECK,spotShadow[]=TM_LIGHT_GUARD_SPOT_SHADOW_CHECK;
 shadow_verified=verified&&bytes(TM_VAL_POINT_SHADOW_CHECK,pointShadow)&&bytes(TM_VAL_SPOT_SHADOW_CHECK,spotShadow);
 if(!verified){faulted=true;armed=false;report("FAILED exact-target native API guard; no allocations");}
 return verified;
}
bool manager(std::uintptr_t& graphics,std::uintptr_t& value){
 std::uintptr_t vt=0;
 return read(base+TM_VAL_LIGHT_ROOT_RVA,graphics)&&graphics&&read(graphics+TM_OFF_GRAPHICS_LIGHT_MANAGER,value)&&value&&read(value,vt)&&vt==base+TM_VAL_LIGHT_MANAGER_VTABLE;
}
bool matches(const Owned& o){std::uintptr_t vt=0;return read(reinterpret_cast<std::uintptr_t>(o.body),vt)&&vt==base+(o.type==light_editor::Type::Point?TM_VAL_POINT_LIGHT_VTABLE:TM_VAL_SPOT_LIGHT_VTABLE);}
std::vector<std::uintptr_t> membership;
bool registry_snapshot(std::uintptr_t manager){
 // Bulk-copy once per tick; do not issue one memory-read syscall per native
 // light per editor light. Capacity is reused and no fixed light cap is imposed.
 membership.clear();
 for(auto offset:{TM_OFF_LIGHT_COLLECTION_A,TM_OFF_LIGHT_COLLECTION_B}){
  std::uintptr_t entries[3]{};
  if(!read(manager+offset,entries,sizeof(entries))||entries[0]>entries[1]||entries[1]>entries[2]||entries[2]>0x7fffffffffffULL||(entries[0]|entries[1]|entries[2])%8||(entries[0]<0x10000&&entries[1]!=0))return false;
  const auto count=(entries[1]-entries[0])/8,old=membership.size();
  if(count>membership.max_size()-old)return false;
  membership.resize(old+count);
  if(count&&!read(entries[0],membership.data()+old,count*sizeof(std::uintptr_t)))return false;
 }
 return true;
}
struct Lock {
 void* lock;
 bool held;
 explicit Lock(std::uintptr_t m):lock(reinterpret_cast<void*>(m+TM_OFF_LIGHT_LOCK)),held(api<TryLock>(TM_VAL_LIGHT_TRY_LOCK)(lock)==0){if(!held)++lock_skips;}
 ~Lock(){if(held)api<Unlock>(TM_VAL_LIGHT_UNLOCK)(lock);}
};
// These primitives contain no STL objects. SEH faults fail closed; not a substitute
// for ABI/lifetime validation and not a guarantee of rollback inside native code.
void* allocate(std::uintptr_t m,const light_editor::Light& l){
 __try {
  void* result=nullptr;
  // Caller already holds the manager lock. The factory's own locking flag MUST
  // be false or its registration helper would reenter a nonrecursive spin lock.
  if(l.type==light_editor::Type::Point){alignas(16) float p[4]={float(l.transform.position[0]),float(l.transform.position[1]),float(l.transform.position[2]),l.radius};result=api<PointFactory>(TM_VAL_POINT_LIGHT_FACTORY)(reinterpret_cast<void*>(m),p,false,0.f);}
  else result=api<SpotFactory>(TM_VAL_SPOT_LIGHT_FACTORY)(reinterpret_cast<void*>(m),false,0.f);
  if(result)api<Retain>(TM_VAL_LIGHT_RETAIN)(static_cast<unsigned char*>(result)+TM_OFF_LIGHT_REFCOUNT);
  return result;
 }__except(EXCEPTION_EXECUTE_HANDLER){faulted=true;armed=false;return nullptr;}
}
bool apply(const Owned& o,const light_editor::Light& l){
 __try {
  auto* p=static_cast<unsigned char*>(o.body);
  // Linear RGB scaled by relative intensity. No claim of calibrated lumen units.
  float diffuse[4]={l.rgb[0]*l.intensity,l.rgb[1]*l.intensity,l.rgb[2]*l.intensity,1};
  float specular[4]={l.specular_rgb[0]*l.intensity,l.specular_rgb[1]*l.intensity,l.specular_rgb[2]*l.intensity,1};
  memcpy(p+TM_OFF_LIGHT_DIFFUSE,diffuse,sizeof(diffuse));memcpy(p+TM_OFF_LIGHT_SPECULAR,specular,sizeof(specular));
  memcpy(p+TM_OFF_LIGHT_SOURCE_RADIUS,&l.source_radius,sizeof(float));
  // Native render packet generation consumes these documented live properties.
  // The engine owns shadow resource allocation; Theater never allocates a shadow map.
  const bool shadow=shadow_requested.load()&&shadow_verified&&l.shadows;
  p[TM_OFF_LIGHT_SHADOW]=shadow?1:0;p[TM_OFF_LIGHT_ENABLED]=l.enabled&&l.intensity>0?1:0;
  if(shadow){memcpy(p+TM_OFF_LIGHT_SHADOW_INTENSITY,&l.shadow_strength,4);memcpy(p+TM_OFF_LIGHT_SHADOW_SPEC_LEVEL,&l.shadow_level,4);memcpy(p+TM_OFF_LIGHT_SHADOW_BIAS,&l.shadow_bias,4);}
  if(l.type==light_editor::Type::Point){alignas(16) float pos[4]={float(l.transform.position[0]),float(l.transform.position[1]),float(l.transform.position[2]),l.radius};api<PointSet>(TM_VAL_POINT_LIGHT_SET_POSITION)(o.body,pos);}
  else {
   alignas(16) float matrix[16];camera_runtime::encode_pose(l.transform,matrix);
   api<SpotMatrix>(TM_VAL_SPOT_LIGHT_SET_MATRIX)(o.body,matrix);
   const float angle=l.cone_degrees*3.14159265358979323846f/180.f;
   // Source projection math uses near/far and vertical/horizontal full angles.
   api<SpotShape>(TM_VAL_SPOT_LIGHT_SET_SHAPE)(o.body,std::max(.001f,l.radius*.001f),l.radius,angle,angle);
  }
  // Refresh native derived world bounds / transforms after setters. Zero dt
  // avoids advancing fade/modifier time twice if the engine also updates it.
  api<Update>(l.type==light_editor::Type::Point?TM_VAL_POINT_LIGHT_UPDATE:TM_VAL_SPOT_LIGHT_UPDATE)(o.body,0.f);
  return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){faulted=true;armed=false;return false;}
}
bool release(Owned& o){
 __try {api<Release>(TM_VAL_LIGHT_REMOVE_RELEASE)(nullptr,o.body);o.body=nullptr;++removed;return true;}
 __except(EXCEPTION_EXECUTE_HANDLER){faulted=true;armed=false;return false;}
}
bool valid(const light_editor::Light& l){
 return l.shadow_level>=1&&l.shadow_level<=5&&std::isfinite(l.shadow_strength)&&l.shadow_strength>=0&&l.shadow_strength<=1&&l.shadow_bias>=-7&&l.shadow_bias<=7&&unsigned(l.type)<=1&&cinematic::valid(l.transform)&&std::all_of(l.transform.position.begin(),l.transform.position.end(),[](double x){return std::isfinite(static_cast<float>(x));})&&l.radius>0&&l.radius<=500&&std::isfinite(l.radius)&&l.intensity>=0&&l.intensity<=100&&std::isfinite(l.intensity)&&l.cone_degrees>=1&&l.cone_degrees<=179&&std::isfinite(l.cone_degrees)&&l.source_radius>=0&&l.source_radius<=500&&std::isfinite(l.source_radius)&&std::all_of(std::begin(l.rgb),std::end(l.rgb),[](float x){return std::isfinite(x)&&x>=0&&x<=1;})&&std::all_of(std::begin(l.specular_rgb),std::end(l.specular_rgb),[](float x){return std::isfinite(x)&&x>=0&&x<=1;});
}
}
View view(){std::lock_guard lock(telemetry);auto v=state;v.enabled=armed;v.shadows=shadow_requested;return v;}
void enable(bool value){armed=value&&!faulted.load();}
void shadows(bool value){shadow_requested=value&&!faulted.load();}
void host_connected(bool value){linked=value;if(value)heartbeat=GetTickCount64();else armed=false;}
}
extern "C" void tm_native_lights_disable(){native_lights::enable(false);}
extern "C" void tm_native_lights_tick(int active){using namespace native_lights;
 if(in_tick.test_and_set(std::memory_order_acquire))return;TickGuard tick_guard;
 const auto now=GetTickCount64();
 if(faulted){report("FAILED native backend quarantined; restart game before retry");return;}
 if(!armed&&owned.empty())return;
 if(!active){armed=false;report("Loading or player unavailable; cleanup deferred to loaded renderer context");return;}
 if(!guard())return;
 if(!linked||now-heartbeat.load()>2000)armed=false;
 if(now-last_update<33)return;last_update=now;
 DWORD foreground_pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground_pid);
 const bool render=active&&linked&&armed&&!faulted&&now-heartbeat.load()<=2000&&foreground_pid==GetCurrentProcessId();
 light_editor::View desired;
 if(render&&!light_editor::snapshot(desired))return; // Never wait for the UI mutex.
 std::uintptr_t graphics=0,m=0;
 if(!manager(graphics,m)){report("Waiting for native light manager; owned references retained for cleanup");return;}
 {
  Lock lock(m);if(!lock.held){report("Waiting for native light lock");return;}
  if(!registry_snapshot(m)){armed=false;report("Invalid native collection; cleanup deferred until a valid context");return;}
 }
 // Removal wrapper locks internally. Never call it while our lock is held.
 // API releases our strong reference after requesting native deferred removal.
 for(auto it=owned.begin();it!=owned.end();){
  const bool keep=render&&it->manager==m&&std::any_of(desired.lights.begin(),desired.lights.end(),[&](const auto& l){return l.id==it->editor_id&&l.enabled&&l.type==it->type&&valid(l);});
  const bool member=std::find(membership.begin(),membership.end(),reinterpret_cast<std::uintptr_t>(it->body))!=membership.end();
  if(keep&&member){++it;continue;}
  if(!matches(*it)){faulted=true;armed=false;report("FAILED owned-light type guard; unknown body not dereferenced");return;}
  // Skip removal if lock already owned, preventing same-thread reentry. An
  // intervening other-thread acquisition uses the native API's normal wait.
  {Lock lock(m);if(!lock.held){report("Waiting for native light lock");return;}}
  if(!release(*it)){report("FAILED native remove/release; further creation disabled");return;}
  it=owned.erase(it);
 }
 if(!render){report(faulted?"FAILED backend disabled":"Lights released; rendering inactive");return;}
 {
  Lock lock(m);if(!lock.held){report("Waiting for native light lock");return;}
  // Re-read manager while protected. No allocations/writes in a replaced context.
  std::uintptr_t current_graphics=0,current_manager=0;
  if(!manager(current_graphics,current_manager)||current_graphics!=graphics||current_manager!=m){report("Manager changed; frame discarded");return;}
  for(const auto& l:desired.lights){
   if(!l.enabled)continue;if(!valid(l)){armed=false;report("FAILED invalid light values; rendering disabled");return;}
   auto it=std::find_if(owned.begin(),owned.end(),[&](const auto& o){return o.editor_id==l.id;});
   if(it==owned.end()){
    // Reserve bookkeeping BEFORE a native allocation to avoid losing an owned ref.
    owned.reserve(owned.size()+1);
    void* body=allocate(m,l);if(!body){armed=false;report(faulted?"FAILED native factory fault":"FAILED native light allocation");return;}
    owned.push_back({l.id,body,m,l.type});it=owned.end()-1;++created;
   }
   if(!matches(*it)||!apply(*it,l)){faulted=true;armed=false;report("FAILED native light write; cleanup queued");return;}
  }
 }
 report(shadow_requested&&shadow_verified?"Native lights submitted; shadow requests enabled; visual validation required":"Native lights submitted; shadow requests off; visual validation required");
}

#include "EldenRingLightAdapter.h"
#include "GameProfile.h"
#include <atomic>
#include <mutex>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
namespace game_lights { namespace {
std::mutex mutex;
std::atomic_bool enabled=false, pending=false, linked=false, inspected=false;
std::atomic<ULONGLONG> heartbeat=0;
std::atomic_uint selected_collection=0,selected_page=0;
View state;
ULONGLONG last_scan=0;
std::uintptr_t previous_root=0,previous_manager=0;
struct Vector { std::uintptr_t begin,end,capacity; };
bool read(std::uintptr_t p,void* dst,std::size_t size){SIZE_T n=0;return p>=0x10000 && p<=0x00007FFFFFFFFFFFULL-size && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),dst,size,&n)&&n==size;}
template<class T> bool read(std::uintptr_t p,T& value){return read(p,&value,sizeof(value));}
void status(const char* text){if(state.diagnostic==text)return;state.diagnostic=text;wchar_t path[MAX_PATH]{};if(GetTempPathW(MAX_PATH,path)){std::ofstream log(std::filesystem::path(path)/L"TheaterModeGame.log",std::ios::app);log<<"LIGHTS "<<text<<'\n';}}
bool valid(const Vector& v){return (v.begin|v.end|v.capacity)%8==0&&v.begin<=v.end&&v.end<=v.capacity&&v.capacity<=0x00007FFFFFFFFFFFULL&&(v.begin>=0x10000||v.end==0);}
bool root(std::uintptr_t base,std::uintptr_t& graphics,std::uintptr_t& manager){return read(base+TM_VAL_LIGHT_ROOT_RVA,graphics)&&graphics>=0x10000&&graphics<=0x00007FFFFFFFFFFFULL-TM_OFF_GRAPHICS_LIGHT_MANAGER-8&&read(graphics+TM_OFF_GRAPHICS_LIGHT_MANAGER,manager)&&manager;}
bool guard(std::uintptr_t base){unsigned char a[7]{},b[7]{};constexpr unsigned char expected[]={0x48,0x8B,0x88,0x18,0xC5,0,0};std::int32_t delta=0;
 if(!read(base+TM_VAL_LIGHT_ROOT_SITE,a,7)||a[0]!=0x48||a[1]!=0x8B||a[2]!=0x05)return false;
 memcpy(&delta,a+3,4);return base+TM_VAL_LIGHT_ROOT_SITE+7+static_cast<std::intptr_t>(delta)==base+TM_VAL_LIGHT_ROOT_RVA&&read(base+TM_VAL_LIGHT_MANAGER_SITE,b,7)&&memcmp(b,expected,7)==0;}
}
View view(){std::lock_guard lock(mutex);auto v=state;v.monitoring=enabled;return v;}
void request_scan(){inspected=true;pending=true;}
void monitor(bool v){enabled=v;if(v){inspected=true;pending=true;}}
void page(unsigned collection,unsigned index){inspected=true;selected_collection=std::min(collection,1u);selected_page=index;pending=true;}
void host_connected(bool v){linked=v;if(v)heartbeat=GetTickCount64();}
}
extern "C" int tm_lights_requested(){return game_lights::inspected.load();}
extern "C" void tm_lights_tick(int active){using namespace game_lights;const auto now=GetTickCount64();
 std::unique_lock lock(mutex,std::try_to_lock);if(!lock.owns_lock())return;
 if(!active||!linked||now-heartbeat.load()>2000){state.available=false;state.rows=0;state.count[0]=state.count[1]=0;state.manager=0;previous_manager=previous_root=0;if(inspected)status("Waiting for offline loaded world and host connection");return;}
 if(!pending.load()&&(!enabled||now-last_scan<1000))return;
 pending=false;last_scan=now;state.available=false;state.rows=0;state.count[0]=state.count[1]=0;state.manager=0;
 const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));std::uintptr_t graphics=0,manager=0,vtable=0;
 if(!guard(base)){status("FAILED: exact-target light instruction guard");return;}
 if(!root(base,graphics,manager)||!read(manager,vtable)||vtable!=base+TM_VAL_LIGHT_MANAGER_VTABLE){status("Native GXLightManager unavailable or vtable mismatch");return;}
 Vector vectors[2]{};if(!read(manager+TM_OFF_LIGHT_COLLECTION_A,vectors[0])||!read(manager+TM_OFF_LIGHT_COLLECTION_B,vectors[1])||!valid(vectors[0])||!valid(vectors[1])){status("Invalid or changing native light collection header");return;}
 if(graphics!=previous_root||manager!=previous_manager){++state.generation;previous_root=graphics;previous_manager=manager;}
 View next=state;next.manager=manager;next.collection=selected_collection;next.page=selected_page;next.rows=0;
 for(unsigned i=0;i<2;++i)next.count[i]=(vectors[i].end-vectors[i].begin)/8;
 const auto start=std::uint64_t(next.page)*page_size;const auto count=next.count[next.collection];
 if(start<count){next.rows=static_cast<unsigned>(std::min<std::uint64_t>(page_size,count-start));std::array<std::uintptr_t,page_size> pointers{};
  if(!read(vectors[next.collection].begin+start*8,pointers.data(),next.rows*8)){status("Light page became unreadable; refresh");return;}
  for(unsigned i=0;i<next.rows;++i){auto& row=next.lights[i];row={};row.address=pointers[i];std::uintptr_t vt=0;
   row.readable=read(row.address,vt)&&read(row.address+TM_OFF_LIGHT_ID,row.id);if(!row.readable)continue;
   row.type=vt==base+TM_VAL_POINT_LIGHT_VTABLE?1:vt==base+TM_VAL_SPOT_LIGHT_VTABLE?2:0;
   if(row.type==1&&read(row.address+TM_OFF_POINT_LIGHT_SPATIAL,row.spatial,sizeof(row.spatial)))row.spatial_valid=std::all_of(std::begin(row.spatial),std::end(row.spatial),[](float f){return std::isfinite(f);});
  }
 }
 Vector after[2]{};std::uintptr_t graphics_after=0,manager_after=0,vt_after=0;
 if(!root(base,graphics_after,manager_after)||graphics_after!=graphics||manager_after!=manager||!read(manager,vt_after)||vt_after!=vtable||!read(manager+TM_OFF_LIGHT_COLLECTION_A,after[0])||!read(manager+TM_OFF_LIGHT_COLLECTION_B,after[1])||memcmp(vectors,after,sizeof(vectors))){status("Collection changed during observation; snapshot discarded");return;}
 next.available=true;next.sampled_ms=now;++next.scans;
 const bool changed=state.scans==0||state.manager!=next.manager||state.count[0]!=next.count[0]||state.count[1]!=next.count[1];
 state=next;status("Native light snapshot captured (read-only, best effort)");
 if(changed){wchar_t path[MAX_PATH]{};if(GetTempPathW(MAX_PATH,path)){std::ofstream log(std::filesystem::path(path)/L"TheaterModeGame.log",std::ios::app);log<<"LIGHTS manager="<<std::hex<<state.manager<<std::dec<<" observed_generation="<<state.generation<<" A="<<state.count[0]<<" B="<<state.count[1]<<" page_rows="<<state.rows<<'\n';}}
}

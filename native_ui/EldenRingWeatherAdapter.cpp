#include "EldenRingWeatherAdapter.h"
#include "GameProfile.h"
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
namespace game_weather {
namespace {
// Independently described native preset IDs; availability/appearance depends on the loaded area's params.
constexpr Preset catalog[]={
 {0,"Slightly cloudy"},{1,"Sunny"},{10,"Overcast"},{11,"Storm clouds"},
 {20,"Rain"},{21,"Heavy rain"},{30,"Downpour"},{31,"Fog"},
 {40,"Light snow"},{41,"Snow"},{50,"Freezing fog"},{51,"Deep freezing fog"},{52,"Freezing rainy fog"},
 {60,"Windy"},{81,"Blizzard"},{82,"Rain and snow"},{83,"Moonlight"},{99,"Clear / light fog"},
 {1001,"Regional variant (1001)"},{1010,"Regional variant (1010)"},{1011,"Regional variant (1011)"},
 {1020,"Regional variant (1020)"},{1021,"Regional variant (1021)"},{1040,"Regional variant (1040)"},
 {1050,"Regional variant (1050)"},{1051,"Regional variant (1051)"},{1052,"Regional variant (1052)"},
 {2010,"Regional variant (2010)"},{2011,"Regional variant (2011)"},{2020,"Regional variant (2020)"},{2021,"Regional variant (2021)"},
 {2110,"Regional variant (2110)"},{2111,"Regional variant (2111)"},{3010,"Regional variant (3010)"},{3011,"Regional variant (3011)"},
 {3101,"Regional variant (3101)"},{3110,"Regional variant (3110)"},{3111,"Regional variant (3111)"},{3120,"Regional variant (3120)"},
 {4000,"Regional variant (4000)"},{4010,"Regional variant (4010)"},{4011,"Regional variant (4011)"},{4040,"Regional variant (4040)"},
 {4110,"Regional variant (4110)"},{4111,"Regional variant (4111)"},{4140,"Regional variant (4140)"},
 {4201,"Regional variant (4201)"},{4210,"Regional variant (4210)"},{4211,"Regional variant (4211)"},
 {4220,"Regional variant (4220)"},{4221,"Regional variant (4221)"},{4230,"Regional variant (4230)"},{4231,"Regional variant (4231)"},
 {4240,"Regional variant (4240)"},{4241,"Regional variant (4241)"},{4250,"Regional variant (4250)"},
 {4251,"Regional variant (4251)"},{4252,"Regional variant (4252)"},{4260,"Regional variant (4260)"}
};
std::mutex mutex;
std::atomic_bool requested=false, linked=false;
std::atomic_int selected=1, desired=1;
std::atomic<ULONGLONG> hostTick=0;
View state;
std::uintptr_t owner=0;
short previous=-1,last=-1;
ULONGLONG started=0,lastSend=0;
bool validated=false;
std::string lastLog;
bool read(std::uintptr_t address,void* out,std::size_t size){SIZE_T n=0;return address>=0x10000&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out,size,&n)&&n==size;}
bool write(std::uintptr_t root,short value){SIZE_T n=0;return WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(root+TM_OFF_WEATHER_REQUEST),&value,sizeof(value),&n)&&n==sizeof(value);}
void status(const char* message){state.diagnostic=message;if(lastLog==message)return;lastLog=message;
 wchar_t path[MAX_PATH]{};GetTempPathW(MAX_PATH,path);std::ofstream f(std::filesystem::path(path)/L"TheaterModeGame.log",std::ios::app);f<<"WEATHER "<<message<<'\n';}
bool validate(std::uintptr_t base){
 unsigned char root[13],constructor[5],consumer[5];
 constexpr unsigned char c[]={0x66,0x44,0x89,0x61,0x02},q[]={0x66,0x83,0x7e,0x02,0xff};
 if(!read(base+TM_VAL_WEATHER_ROOT_SITE,root,sizeof(root))||root[0]!=0x48||root[1]!=0x8b||root[2]!=0x15||root[7]!=0x32||root[8]!=0xc0||root[9]!=0x48||root[10]!=0x85||root[11]!=0xd2)return false;
 std::int32_t displacement;memcpy(&displacement,root+3,4);
 if(base+TM_VAL_WEATHER_ROOT_SITE+7+static_cast<std::intptr_t>(displacement)!=base+TM_VAL_WEATHER_ROOT_RVA)return false;
 return read(base+TM_VAL_WEATHER_CONSTRUCTOR_SITE,constructor,5)&&memcmp(constructor,c,5)==0&&read(base+TM_VAL_WEATHER_CONSUMER_SITE,consumer,5)&&memcmp(consumer,q,5)==0;
}
void release(std::uintptr_t root,bool restore){
 if(owner){short pending=-1;
  if(root==owner&&read(root+TM_OFF_WEATHER_REQUEST,&pending,2)&&(pending==-1||pending==last)){
   // Native code consumes +2 then clears it. It is a command mailbox, NOT a persistent multiplier.
   if(!write(root,restore&&previous>=0?previous:short(-1)))status("Restore request FAILED");
   else status(restore?"Previous weather requested; automatic weather released":"Pending request cancelled; context no longer active");
  }else status("Weather owner/context changed; stale object not touched");
 }
 owner=0;state.applied=false;
}
}
const Preset* presets(std::size_t& count){count=std::size(catalog);return catalog;}
View view(){std::lock_guard lock(mutex);auto result=state;result.enabled=requested;result.selected=selected;return result;}
void select(int id){if(std::any_of(std::begin(catalog),std::end(catalog),[id](auto&p){return p.id==id;}))selected=id;}
void enable(bool value){if(value)desired=selected.load();requested=value;}
void host_connected(bool value){if(value)hostTick=GetTickCount64();linked=value;if(!value)requested=false;}
}
extern "C" void tm_weather_tick(int active){
 using namespace game_weather;std::unique_lock lock(mutex,std::try_to_lock);if(!lock.owns_lock())return;
 auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));std::uintptr_t root=0;
 if(!validated){if(!base||!validate(base)){requested=false;state.available=false;status("Exact-profile weather instructions rejected");return;}validated=true;}
 if(!read(base+TM_VAL_WEATHER_ROOT_RVA,&root,sizeof(root))||root<0x10000){requested=false;release(0,false);state.available=false;status("Weather manager unavailable");return;}
 short current=-1,pending=-1;
 if(!read(root+TM_OFF_WEATHER_CURRENT,&current,2)||!read(root+TM_OFF_WEATHER_REQUEST,&pending,2)){requested=false;release(0,false);state.available=false;status("Weather fields unreadable");return;}
 state.current=current;state.pending=pending;state.available=active&&linked&&current>=0;
 DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
 if(linked&&GetTickCount64()-hostTick.load()>2000){linked=false;requested=false;}
 if(!active||!linked||pid!=GetCurrentProcessId()){requested=false;release(root,active!=0);status("Weather OFF: loaded world, host and game focus required");return;}
 if(!requested){release(root,true);if(!owner&&state.diagnostic.empty())status("Automatic weather");return;}
 if(owner&&owner!=root){requested=false;release(root,false);status("Weather manager replaced; override disabled");return;}
 auto now=GetTickCount64();short target=static_cast<short>(desired.load());
 if(!owner){if(current<0||pending!=-1){requested=false;status("Native weather request busy; try again after transition");return;}owner=root;previous=current;last=target;started=now;lastSend=0;state.applied=false;status("Native weather override requested; visual validation required");}
 if(pending!=-1&&pending!=last){requested=false;release(root,false);status("Another weather request owns the mailbox; override released");return;}
 if(last!=target&&pending!=-1)return; // Let our earlier queued command finish before changing target.
 if(last!=target){last=target;started=now;state.applied=false;}
 // Do not restart the native transition every frame. Requeue only if the requested ID is not active.
 if(current==target){started=now;state.applied=true;status("Native weather ID matched; visual effect depends on area");return;}
 if(now-started>15000){requested=false;release(root,true);status("Weather ID not observed within 15 seconds; override released");return;}
 if(pending==-1&&now-lastSend>=1000){if(!write(root,target)){requested=false;release(root,true);status("Weather request write FAILED");return;}lastSend=now;}
}

extern "C" void tm_weather_disable(){game_weather::enable(false);}

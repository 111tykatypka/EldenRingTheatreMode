#include "WindController.h"
#include "EldenRingWeatherAdapter.h"
#include <mutex>
#include <cmath>
#include <algorithm>
#include <windows.h>
#include "GameProfile.h"
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
namespace game_wind { namespace {std::mutex mutex;View state;}
View view(){std::lock_guard lock(mutex);return state;}
void configure(bool enabled,float strength){
    std::lock_guard lock(mutex);
    if(!std::isfinite(strength)){state.enabled=false;return;}
    state.enabled=enabled;state.strength=std::clamp(strength,0.f,3.f);
}
namespace {std::atomic_bool inspection=false;}
void inspect(bool enabled){inspection=enabled;std::lock_guard lock(mutex);state.inspect=enabled;if(!enabled)state.probe_status=0;}
}
extern "C" int tm_wind_request(float* strength){
    DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
    const bool allowed=game_weather::view().available&&pid==GetCurrentProcessId();
    std::unique_lock lock(game_wind::mutex,std::try_to_lock);if(!lock.owns_lock())return -1;
    if(!allowed)game_wind::state.enabled=false;
    if(strength)*strength=game_wind::state.strength;
    return game_wind::state.enabled?1:0;
}
extern "C" void tm_wind_report(int status,std::uint32_t grass,std::uint32_t assets){
    std::unique_lock lock(game_wind::mutex,std::try_to_lock);if(!lock.owns_lock())return;
    game_wind::state.status=status;game_wind::state.grass_rows=grass;game_wind::state.asset_rows=assets;
    if(status<0)game_wind::state.enabled=false;
}
extern "C" void tm_wind_disable(){auto v=game_wind::view();game_wind::configure(false,v.strength);}
extern "C" void tm_wind_inspect_tick(int allowed){
    // Read-only diagnostic of a native force-field registry, NOT a write binding.
    using namespace game_wind;static ULONGLONG previous=0;static int lastStatus=-99;
    if(!inspection.load())return;
    auto now=GetTickCount64();if(now-previous<1000)return;previous=now;
    DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
    auto read=[](std::uintptr_t p,void*out,SIZE_T n){SIZE_T got=0;return p>=0x10000&&p<=0x00007fffffffffffULL-n&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),out,n,&got)&&got==n;};
    int status=0;std::uint32_t count=0,observed=0;
    if(allowed&&pid==GetCurrentProcessId()&&game_weather::view().available){
        const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        const unsigned char expected[]=TM_WIND_GUARD_LOOKUP;
        unsigned char bytes[sizeof(expected)]{};std::uintptr_t root=0,table=0;unsigned char active=0;
        if(!read(base+TM_VAL_WIND_LOOKUP_RVA,bytes,sizeof(bytes))||memcmp(bytes,expected,sizeof(bytes)))status=-1;
        else if(!read(base+TM_VAL_WIND_REGISTRY_RVA,&root,8)||!root||!read(root+TM_OFF_WIND_REGISTRY_ACTIVE,&active,1)||!active)status=0;
        else if(!read(root+TM_OFF_WIND_REGISTRY_SLOTS,&count,4)||!read(root+TM_OFF_WIND_REGISTRY_TABLE,&table,8))status=-2;
        else {
            // A bounded diagnostic sample, never an actor-count or recording limit.
            const auto end=std::min(count,256u);status=1;
            for(std::uint32_t i=0;i<end;++i){
                std::uintptr_t record=0,appearance=0,vtable=0;
                if(!read(table+std::uintptr_t(i)*8,&record,8)){status=-2;break;}
                if(record<0x10000||record==~std::uintptr_t(0))continue;
                if(read(record+TM_OFF_WIND_RECORD_APPEARANCE,&appearance,8)&&read(appearance,&vtable,8)&&vtable>=base&&vtable<base+0x7000000)++observed;
            }
            std::uintptr_t checkRoot=0,checkTable=0;std::uint32_t checkCount=0;
            if(!read(base+TM_VAL_WIND_REGISTRY_RVA,&checkRoot,8)||checkRoot!=root||!read(root+TM_OFF_WIND_REGISTRY_TABLE,&checkTable,8)||checkTable!=table||!read(root+TM_OFF_WIND_REGISTRY_SLOTS,&checkCount,4)||checkCount!=count)status=-3;
        }
    }
    if(status<0){count=0;observed=0;}
    {std::unique_lock lock(mutex,std::try_to_lock);if(!lock.owns_lock())return;state.probe_status=status;state.native_slots=count;state.observed_records=observed;}
    if(status!=lastStatus){lastStatus=status;wchar_t path[MAX_PATH]{};GetTempPathW(MAX_PATH,path);std::ofstream f(std::filesystem::path(path)/L"TheaterModeGame.log",std::ios::app);f<<"WIND_INSPECT status="<<status<<" registry_slots="<<count<<" readable_appearance_records="<<observed<<" (sample up to 256; read-only; force types and cloth consumers UNKNOWN)\n";}
}

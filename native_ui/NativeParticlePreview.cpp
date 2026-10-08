#include "NativeParticleBackend.h"
#include "GameProfile.h"
#include "CinematicCameraRuntime.h"
#include <windows.h>
#include <mutex>
#include <atomic>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace native_particles { namespace {
std::mutex preview_mutex;
PreviewView preview_state;
std::atomic_bool work=false;
bool pending=false,stop=false;
std::uint32_t requested_id=0;
cinematic::State requested_pose;
// Stable address: the native handle participates in intrusive tracking.
alignas(16) std::array<unsigned char,TM_VAL_VFX_HANDLE_SIZE> handle{};
ULONGLONG expires=0;
bool initialized=false;
using Create=void*(__fastcall*)(void*,void*,std::uint32_t,void*,const float*,int,int);
using HandleFn=void(__fastcall*)(void*);
bool read(std::uintptr_t p,void* out,std::size_t n){SIZE_T got=0;return p>=0x10000&&p<=0x00007FFFFFFFFFFFULL-n&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),out,n,&got)&&got==n;}
bool guard(std::uintptr_t base){
    struct Site {std::uintptr_t rva;std::array<unsigned char,10> bytes;};
    const Site sites[]={
        {TM_VAL_VFX_SCENE_CREATE_RVA,{0x40,0x53,0x48,0x81,0xec,0x80,0,0,0,0x48}},
        {TM_VAL_VFX_HANDLE_STOP_RVA,{0x48,0x89,0x4c,0x24,0x08,0x57,0x48,0x83,0xec,0x20}},
        {TM_VAL_VFX_HANDLE_RELEASE_RVA,{0x48,0x89,0x4c,0x24,0x08,0x57,0x48,0x83,0xec,0x30}}};
    for(const auto& s:sites){unsigned char actual[10]{};if(!read(base+s.rva,actual,10)||memcmp(actual,s.bytes.data(),10))return false;}return true;
}
bool resident(std::uintptr_t scene,std::uint32_t wanted){
    std::uintptr_t graphics=0,container=0,head=0,node=0;
    if(!read(scene+TM_OFF_FFX_GRAPHICS_MANAGER,&graphics,sizeof(graphics))||
       !read(graphics+TM_OFF_FFX_RESOURCE_CONTAINER,&container,sizeof(container))||
       !read(container+TM_OFF_FXR_LIST_HEAD,&head,sizeof(head))||!read(head,&node,sizeof(node)))return false;
    const auto deadline=GetTickCount64()+8;
    while(node&&node!=head&&GetTickCount64()<deadline){
        std::uint32_t id=0;std::uintptr_t next=0,wrapper=0,fxr=0;
        if(!read(node,&next,sizeof(next))||next==node||!read(node+TM_OFF_FXR_NODE_ID,&id,sizeof(id)))return false;
        if(id==wanted)return read(node+TM_OFF_FXR_NODE_WRAPPER,&wrapper,sizeof(wrapper))&&read(wrapper,&fxr,sizeof(fxr))&&fxr;
        node=next;
    }
    return false;
}
void log(const std::string& message){wchar_t p[MAX_PATH]{};if(GetTempPathW(MAX_PATH,p)){std::ofstream f(std::filesystem::path(p)/L"TheaterModeGame.log",std::ios::app);f<<"PARTICLE_PREVIEW "<<message<<'\n';}}
// Thin SEH boundary. A native exception faults this experiment permanently; the
// static handle is retained rather than destructing a partially initialized object.
bool invoke_create(std::uintptr_t base,std::uintptr_t scene,std::uint32_t id,const float* matrix){
    __try{reinterpret_cast<Create>(base+TM_VAL_VFX_SCENE_CREATE_RVA)(reinterpret_cast<void*>(scene),handle.data(),id,nullptr,matrix,0,0);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool invoke_cleanup(std::uintptr_t base){
    __try{reinterpret_cast<HandleFn>(base+TM_VAL_VFX_HANDLE_STOP_RVA)(handle.data());reinterpret_cast<HandleFn>(base+TM_VAL_VFX_HANDLE_RELEASE_RVA)(handle.data());return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
PreviewView preview_view(){std::lock_guard l(preview_mutex);return preview_state;}
bool preview(std::uint32_t id,const cinematic::State& pose){
    if(!id||!cinematic::valid(pose))return false;
    const auto catalog=view();
    if(!catalog.available||!std::binary_search(catalog.loaded_effect_ids.begin(),catalog.loaded_effect_ids.end(),id))return false;
    std::lock_guard l(preview_mutex);if(preview_state.faulted)return false;
    requested_id=id;requested_pose=pose;pending=true;stop=false;work=true;preview_state.status="Native FXR preview queued";return true;
}
void stop_preview(){std::lock_guard l(preview_mutex);pending=false;stop=true;work=true;}
}
extern "C" int tm_particles_preview_requested(){return native_particles::work.load();}
extern "C" void tm_particles_preview_tick(int active,std::uintptr_t manager){
    using namespace native_particles;
    if(!work.load())return;
    std::unique_lock l(preview_mutex,std::try_to_lock);if(!l.owns_lock())return;
    if(preview_state.faulted){work=false;return;}
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(!guard(base)){preview_state.status="Native FXR function guard failed";preview_state.faulted=initialized;pending=false;log(preview_state.status);work=false;return;}
    const auto now=GetTickCount64();
    if(initialized&&(stop||pending||!active||now>=expires)){
        if(!invoke_cleanup(base)){preview_state.faulted=true;preview_state.status="Native FXR cleanup exception; experiment disabled";log(preview_state.status);work=false;return;}
        initialized=false;preview_state.active=false;handle.fill(0);log("STOP owned preview handle released");
    }
    stop=false;
    if(pending){
        pending=false;
        std::uintptr_t scene=0;
        if(!active||!read(manager+TM_OFF_SFX_SCENE_CTRL,&scene,sizeof(scene))||!scene){preview_state.status="Native FXR preview refused: loaded world/scene unavailable";work=false;return;}
        if(!resident(scene,requested_id)){preview_state.status="FXR no longer resident or lookup exceeded scan budget; refresh catalog";work=false;return;}
        alignas(16) float matrix[16]{};camera_runtime::encode_pose(requested_pose,matrix);
        if(!invoke_create(base,scene,requested_id,matrix)){preview_state.faulted=true;preview_state.status="Native FXR creation exception; experiment disabled";log(preview_state.status);work=false;return;}
        initialized=true;std::uintptr_t object=0;memcpy(&object,handle.data()+TM_OFF_VFX_HANDLE_OBJECT,sizeof(object));
        preview_state.effect_id=requested_id;preview_state.active=object!=0;expires=now+2000;
        preview_state.status=object?"Native FXR handle created; two-second preview (visual verification required)":"Native FXR creation returned an empty handle";
        log("CREATE id="+std::to_string(requested_id)+" handle_present="+std::to_string(object!=0));
        if(!object){if(!invoke_cleanup(base)){preview_state.faulted=true;preview_state.status="Empty native FXR handle cleanup exception; experiment disabled";log(preview_state.status);}else{initialized=false;}work=false;}
    }
    if(!initialized){work=false;preview_state.active=false;if(preview_state.status.find("created")!=std::string::npos)preview_state.status="Native preview stopped";}
}

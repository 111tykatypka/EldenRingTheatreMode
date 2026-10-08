#include "NativeParticleBackend.h"
#include "GameProfile.h"
#include <atomic>
#include <mutex>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <unordered_set>
namespace native_particles { namespace {
std::mutex mutex;
std::atomic_bool requested=false;
View state;
bool read(std::uintptr_t address,void* out,std::size_t bytes) {
    SIZE_T read_bytes=0;
    return address>=0x10000 && address<=0x00007FFFFFFFFFFFULL-bytes &&
        ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out,bytes,&read_bytes) && read_bytes==bytes;
}
}
View view(){std::lock_guard lock(mutex);return state;}
void inspect(){requested=true;}
}
extern "C" int tm_particles_inspection_requested(){return native_particles::requested.load()||tm_particles_preview_requested();}
extern "C" void tm_particles_inspect(int active,std::uintptr_t manager){
    using namespace native_particles;
    tm_particles_preview_tick(active,manager);
    if(!requested.load())return;
    std::unique_lock lock(mutex,std::try_to_lock);if(!lock.owns_lock())return;
    requested=false;state.available=false;state.dispatcher_verified=false;state.spawn_supported=false;
    if(!active||!manager){state.status="Waiting for offline loaded world and CSSfx singleton";return;}
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    constexpr unsigned char expected[]={0x85,0xd2,0x74,0x0a,0x83,0xfa,0x01,0x75,0x0a,0xe9,0x12,0x9f,0xff,0xff,0xe9,0xdd,0xf7,0xff,0xff,0xc3};
    unsigned char actual[sizeof(expected)]{};
    if(!read(base+TM_VAL_VFX_DEBUG_DISPATCH_RVA,actual,sizeof(actual))||memcmp(actual,expected,sizeof(actual))){state.status="VFX dispatcher instruction guard failed";return;}
    state.dispatcher_verified=true;
    std::uintptr_t scene=0,vtable=0;
    if(!read(manager,&vtable,sizeof(vtable))||!read(manager+TM_OFF_SFX_SCENE_CTRL,&scene,sizeof(scene))||!scene||
       !read(manager+TM_OFF_SFX_DEBUG_FFX_ID,&state.debug_effect_id,sizeof(state.debug_effect_id))||
       !read(manager+TM_OFF_SFX_DEBUG_DISTANCE,&state.camera_distance,sizeof(state.camera_distance))||!std::isfinite(state.camera_distance)) {
        state.status="CSSfx scene/fields unavailable; no effect created";return;
    }
    state.available=true;++state.inspections;
    state.loaded_effect_ids.clear();
    std::uintptr_t graphics=0,container=0,head=0,node=0;std::uint64_t count=0;
    if(!read(scene+TM_OFF_FFX_GRAPHICS_MANAGER,&graphics,sizeof(graphics))||
       !read(graphics+TM_OFF_FFX_RESOURCE_CONTAINER,&container,sizeof(container))||
       !read(container+TM_OFF_FXR_LIST_HEAD,&head,sizeof(head))||
       !read(container+TM_OFF_FXR_LIST_LENGTH,&count,sizeof(count))||!read(head,&node,sizeof(node))){
        state.status="CSSfx readable; resident FXR list unavailable";return;
    }
    // Snapshot budget protects the game callback; this is a resident-resource page,
    // not a restriction on how many effects the game or editor can contain.
    std::unordered_set<std::uintptr_t> visited;
    const auto deadline=GetTickCount64()+8;
    while(node!=head&&state.loaded_effect_ids.size()<count&&GetTickCount64()<deadline){
        std::uint32_t id=0;std::uintptr_t next=0,wrapper=0,fxr=0;
        if(!node||!visited.insert(node).second||!read(node,&next,sizeof(next))||
           !read(node+TM_OFF_FXR_NODE_ID,&id,sizeof(id))||!read(node+TM_OFF_FXR_NODE_WRAPPER,&wrapper,sizeof(wrapper))||
           !read(wrapper,&fxr,sizeof(fxr)))break;
        if(id&&fxr)state.loaded_effect_ids.push_back(id);
        node=next;
    }
    std::sort(state.loaded_effect_ids.begin(),state.loaded_effect_ids.end());
    state.loaded_effect_ids.erase(std::unique(state.loaded_effect_ids.begin(),state.loaded_effect_ids.end()),state.loaded_effect_ids.end());
    state.spawn_supported=!state.loaded_effect_ids.empty();
    state.status=node==head?"Resident FXR catalog captured. Native preview is experimental.":"Partial resident FXR catalog (scan budget); refresh to rescan.";
}

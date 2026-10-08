// Sole native speed backend: signature-validated CSFlipperImp scalar used by CameraTools.
#include "EldenRingTimingAdapter.h"
#include "GameProfile.h"
#include "TheaterTimescale.h"
#include <atomic>
#include <mutex>
#include <cstring>
namespace game_timing {
namespace {
std::atomic_bool requested=false;
std::mutex state_mutex;
std::string diagnostic="World timing OFF";
std::uintptr_t owned_root=0;float saved=1,last=1;bool inhibited=false;
ULONGLONG last_diagnostic=0;
bool read(std::uintptr_t a,void*dst,std::size_t n){SIZE_T done=0;return a>=0x10000&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(a),dst,n,&done)&&done==n;}
bool write(std::uintptr_t a,float v){SIZE_T done=0;return WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(a),&v,4,&done)&&done==4;}
bool valid_sites(std::uintptr_t base){for(auto site:{TM_VAL_TIMING_SITE_A,TM_VAL_TIMING_SITE_B}){
 unsigned char code[23];if(!read(base+site,code,23)||memcmp(code,"\x48\x8b\x05",3))return false;
 const unsigned char expected[]={0xf3,0x0f,0x10,0x88,0xcc,2,0,0,0xf3,0x0f,0x59,0x88,0x68,2,0,0};if(memcmp(code+7,expected,16))return false;
 std::int32_t displacement;memcpy(&displacement,code+3,4);if(base+site+7+static_cast<std::intptr_t>(displacement)!=base+TM_VAL_TIMING_ROOT_RVA)return false;}return true;}
void release(std::uintptr_t current){if(owned_root){float observed=0;if(current==owned_root&&read(current+TM_OFF_TIMING_SCALE,&observed,4)&&observed==last){diagnostic=write(current+TM_OFF_TIMING_SCALE,saved)?"World timing restored":"World timing restore FAILED";}else diagnostic="Timing ownership changed; old object not touched";}owned_root=0;}
}
void enable(bool v){requested=v;}
bool enabled(){return requested.load();}
std::string status(){std::lock_guard lock(state_mutex);return diagnostic;}
}
// Called only by the verified game task after exact executable profile acceptance.
extern "C" void tm_world_timing_tick(int active,double speed){
 using namespace game_timing;std::unique_lock lock(state_mutex,std::try_to_lock);if(!lock.owns_lock())return;
 if(!requested&&!owned_root){inhibited=false;diagnostic="World timing OFF";return;}
 DWORD foregroundPid=0;GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);active=active&&foregroundPid==GetCurrentProcessId();
 auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));std::uintptr_t root=0;
 if(!base||!valid_sites(base)||!read(base+TM_VAL_TIMING_ROOT_RVA,&root,sizeof(root))||root<0x10000){release(0);diagnostic="Timing binding unavailable/rejected";return;}
 if(!requested||!active){release(root);inhibited=false;if(!owned_root&&diagnostic=="World timing OFF")diagnostic="World timing OFF";return;}
 if(!theater_timescale::valid(speed)){release(root);inhibited=true;diagnostic="Invalid world timescale; writes stopped";return;}
 float value=0;if(!read(root+TM_OFF_TIMING_SCALE,&value,4)||!std::isfinite(value)){release(root);inhibited=true;diagnostic="Invalid native timing scalar";return;}
 if(owned_root&&(root!=owned_root||value!=last)){release(root);inhibited=true;}
 if(inhibited)return;
 if(!owned_root){if(std::abs(value-1.f)>0.0001f){inhibited=true;diagnostic="Another owner/non-normal scalar; world timing rejected";return;}owned_root=root;saved=value;last=value;}
 float target=static_cast<float>(speed);if(target!=last){if(!write(root+TM_OFF_TIMING_SCALE,target)){release(root);inhibited=true;diagnostic="Timing write FAILED";return;}last=target;}
 auto now=GetTickCount64();if(now-last_diagnostic>=1000){last_diagnostic=now;char text[220];snprintf(text,sizeof(text),"Applied scalar=%.6f requested=%.6f root=0x%llX phase=PostPhysics; visual verification required",last,speed,static_cast<unsigned long long>(root));diagnostic=text;}
}
extern "C" int tm_world_timing_diagnostic(char*out,std::size_t size){if(!out||!size)return 0;auto text=game_timing::status();snprintf(out,size,"WORLD_TIMING %s enabled=%u",text.c_str(),game_timing::enabled());return 1;}

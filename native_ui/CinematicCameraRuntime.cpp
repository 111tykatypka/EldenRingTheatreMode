// Independently implemented, version-guarded camera-copy experiment.
// Native original always runs first. No GameRend owner flags or source cameras are patched.
#include <windows.h>
#include <realtimeapiset.h>
#pragma comment(lib,"mincore.lib")
#include <MinHook.h>
#include "CinematicCameraRuntime.h"
#include "CameraTelemetry.h"
#include "TheaterHotkeys.h"
#include <mutex>
#include <atomic>
#include <cstring>
#include <cstdio>
namespace camera_runtime {
namespace {
using Copy=void(__fastcall*)(void*,void*,void*);
Copy original=nullptr;
std::mutex mutex;
View state;
cinematic::Track track;
std::atomic_bool input_owned=false,ui_visible=true;
std::atomic<std::uint64_t> game_heartbeat=0;
std::atomic_bool game_allowed=false;
std::atomic_bool faulted=false;
std::atomic<HWND> game_window=nullptr;
std::atomic<long> mouse_x=0,mouse_y=0;
std::uint64_t clock_now(){ULONGLONG t=0;QueryInterruptTimePrecise(&t);return t*100;}
std::uint64_t timeline_ns=0,duration_ns=0,anchor_ns=0,host_heartbeat=0,last_tick=0,probe_until=0,next_id=1;
bool playing=false,linked=false,reset=true;
double speed=1;
std::string replay_path,keys_replay_path;
std::uintptr_t owner=0,destination=0;
bool read(std::uintptr_t address,void*out,std::size_t bytes){SIZE_T n=0;return address>=0x10000&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out,bytes,&n)&&n==bytes;}
bool write(std::uintptr_t address,const void*data,std::size_t bytes){SIZE_T n=0;return WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),data,bytes,&n)&&n==bytes;}
std::uint64_t time_at(std::uint64_t now){
 auto time=timeline_ns;
 if(playing&&now>=anchor_ns){auto delta=static_cast<long double>(now-anchor_ns)*speed;auto remaining=duration_ns>time?duration_ns-time:0;time+=static_cast<std::uint64_t>(std::min<long double>(delta,remaining));}
 return std::min(time,duration_ns);
}
void release(const char*reason){state.enabled=false;state.writing=false;input_owned=false;reset=true;probe_until=0;mouse_x=0;mouse_y=0;state.status=reason;}
// Matrix memory contains right/up/forward basis vectors, then position.
std::optional<cinematic::State> decode(const theater_camera::Slot&c){
 if(!theater_camera::valid(c)||c.fov>=3.14159265f||std::abs(c.matrix[15]-1)>0.01f)return {};
 for(int a=0;a<3;++a)for(int b=a+1;b<3;++b){double dot=0;for(int i=0;i<3;++i)dot+=c.matrix[a*4+i]*c.matrix[b*4+i];if(std::abs(dot)>.02)return {};}
 const auto&m=c.matrix;
 double det=m[0]*(m[5]*m[10]-m[6]*m[9])-m[4]*(m[1]*m[10]-m[2]*m[9])+m[8]*(m[1]*m[6]-m[2]*m[5]);if(det<.95||det>1.05)return {};
 cinematic::Quat q{};double tr=m[0]+m[5]+m[10];
 if(tr>0){double s=2*std::sqrt(tr+1);q={(m[6]-m[9])/s,(m[8]-m[2])/s,(m[1]-m[4])/s,s/4};}
 else if(m[0]>m[5]&&m[0]>m[10]){double s=2*std::sqrt(1+m[0]-m[5]-m[10]);q={s/4,(m[4]+m[1])/s,(m[8]+m[2])/s,(m[6]-m[9])/s};}
 else if(m[5]>m[10]){double s=2*std::sqrt(1+m[5]-m[0]-m[10]);q={(m[4]+m[1])/s,s/4,(m[9]+m[6])/s,(m[8]-m[2])/s};}
 else{double s=2*std::sqrt(1+m[10]-m[0]-m[5]);q={(m[8]+m[2])/s,(m[9]+m[6])/s,s/4,(m[1]-m[4])/s};}
 auto n=cinematic::normalized(q);if(!n)return {};
 return cinematic::State{{m[12],m[13],m[14]},*n,double(c.fov)*180/3.141592653589793};
}
void encode(const cinematic::State&s,float* m){
 const auto&q=s.orientation;double x=q[0],y=q[1],z=q[2],w=q[3];
 const double values[]={1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y),0,2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x),0,2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y),0,s.position[0],s.position[1],s.position[2],1};
 for(int i=0;i<16;++i)m[i]=static_cast<float>(values[i]);
}
cinematic::Quat product(cinematic::Quat a,cinematic::Quat b){return {a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};}
bool down(int key){return (GetAsyncKeyState(key)&0x8000)!=0;}
void move(double dt){
 if(ui_visible){mouse_x=0;mouse_y=0;return;}
 auto turn=[&](int axis,double angle){cinematic::Quat q{0,0,0,std::cos(angle/2)};q[axis]=std::sin(angle/2);state.pose.orientation=*cinematic::normalized(product(state.pose.orientation,q));};
 turn(1,(int(down(VK_RIGHT))-int(down(VK_LEFT)))*dt+std::clamp(mouse_x.exchange(0)*.0025,-.25,.25));
 turn(0,(int(down(VK_DOWN))-int(down(VK_UP)))*dt+std::clamp(mouse_y.exchange(0)*.0025,-.25,.25));
 turn(2,(int(down('X'))-int(down('Z')))*dt);
 float m[16];encode(state.pose,m);
 cinematic::Vec delta{};double right=int(down('D'))-int(down('A')),forward=int(down('W'))-int(down('S')),up=int(down('E'))-int(down('Q'));
 for(int i=0;i<3;++i)delta[i]=m[i]*right+m[8+i]*forward;delta[1]+=up;
 double n=cinematic::length(delta);if(n>1)delta=cinematic::mul(delta,1/n);
 state.pose.position=cinematic::add(state.pose.position,cinematic::mul(delta,3*dt*(down(VK_SHIFT)?5:down(VK_CONTROL)?.1:1)));
}
void update(void*rend){
 std::unique_lock lock(mutex,std::try_to_lock);if(!lock.owns_lock())return;
 const auto now=clock_now();std::uintptr_t out=0;
 theater_camera::Slot native;
 if(!read(reinterpret_cast<std::uintptr_t>(rend)+0x20,&out,sizeof(out))||!read(out+0x10,native.matrix,80)){state.observed=false;release("Camera output unavailable; native control restored");return;}
 auto pose=decode(native);if(!pose){state.observed=false;release("Invalid matrix/FOV; native control restored");return;}
 state.observed=true;state.timestamp_ns=now;
 if(!state.enabled||state.mode==0){state.pose=*pose;state.writing=false;input_owned=false;reset=true;last_tick=now;return;}
 // RegisterHotKey can route F6 to the host instead of the game window. Poll the
 // shared emergency key here too, before any camera write.
 if(down(theater_hotkeys::Key(theater_hotkeys::Action::StopRecording))){release("Emergency Stop key; native camera control restored");state.mode=0;return;}
 if(!game_allowed||now-game_heartbeat.load()>1000000000ULL||!linked||now-host_heartbeat>1000000000ULL){release("Player/offline/host context lost; native control restored");return;}
 if(GetForegroundWindow()!=game_window.load()){release("Game focus lost; native control restored");return;}
 if(probe_until&&now>=probe_until){release("Two-second camera probe finished; native control restored");return;}
 if(reset){owner=reinterpret_cast<std::uintptr_t>(rend);destination=out;state.pose=*pose;if(probe_until)state.pose.position[0]+=.25;reset=false;last_tick=now;}
 if(owner!=reinterpret_cast<std::uintptr_t>(rend)||destination!=out){release("Camera generation changed; native control restored");return;}
 double dt=std::min(double(now-last_tick)/1e9,.05);last_tick=now;
 if(!probe_until){
  if(state.mode==1)move(dt);
  else if(!track.keys().empty()&&keys_replay_path!=replay_path){release("Dolly keys belong to another replay; clear them before creating a new path");return;}
  else if(auto value=track.evaluate(time_at(now))){
   if(!state.writing&&cinematic::length(cinematic::sub(value->position,state.pose.position))>20){release("Dolly start exceeds 20 units; move camera near the path first");return;}state.pose=*value;
  }
 }
 float matrix[16];encode(state.pose,matrix);float fov=static_cast<float>(state.pose.fov_degrees*3.141592653589793/180);
 if(!cinematic::valid(state.pose)||!std::all_of(matrix,matrix+16,[](float v){return std::isfinite(v);})||!write(out+0x10,matrix,64)||!write(out+0x50,&fov,4)){release("Camera write failed; native control restored on next native copy");return;}
 state.writing=true;input_owned=true;state.status=probe_until?"Two-second offset probe active":"Experimental camera override active (runtime unverified)";
}
void __fastcall hook(void*rend,void*a,void*b){
 original(rend,a,b);if(faulted)return;
 try{update(rend);}catch(...){faulted=true;input_owned=false;}
}
}
View view(){std::lock_guard lock(mutex);auto copy=state;copy.keys=track.keys();if(faulted){copy.enabled=false;copy.writing=false;copy.status="Camera backend faulted; overrides disabled until process restart";}return copy;}
std::optional<cinematic::State> decode_candidate(const theater_camera::Slot&slot){return decode(slot);}
void encode_pose(const cinematic::State&pose,float*matrix){encode(pose,matrix);}
void mode(unsigned value){std::lock_guard lock(mutex);state.mode=value%3;state.writing=false;input_owned=false;probe_until=0;if(state.mode==0)reset=true;state.status=state.enabled?"Camera mode changed":"Camera selected; enable experimental writes to apply";}
void enable(bool value){std::lock_guard lock(mutex);if(!value){release("Camera overrides disabled; native control restored");return;}if(faulted||!state.hook_ready||!state.observed||clock_now()-state.timestamp_ns>500000000ULL||!game_allowed||!linked){release("Cannot arm: fresh camera, offline player and host required; backend must not be faulted");return;}state.enabled=true;reset=true;state.status="Experimental writes armed";}
void stop(){std::lock_guard lock(mutex);release("Camera stopped; native control restored");state.mode=0;}
void probe(){enable(true);std::lock_guard lock(mutex);if(state.enabled){state.mode=1;probe_until=clock_now()+2000000000ULL;}}
void add_key(){std::lock_guard lock(mutex);const auto now=clock_now();if(!state.observed||now-state.timestamp_ns>500000000ULL||!linked||now-host_heartbeat>1000000000ULL){state.status="Cannot add key: fresh native camera and host timeline required";return;}
 if(!duration_ns||replay_path.empty()){state.status="Load a replay before adding Dolly keys";return;}
 if(!track.keys().empty()&&keys_replay_path!=replay_path){state.status="Keys belong to another replay; confirm deletion first";return;}
 keys_replay_path=replay_path;
 auto keys=track.keys();auto time=time_at(now);auto found=std::find_if(keys.begin(),keys.end(),[&](const auto&k){return k.time_ns==time;});
 if(found!=keys.end())found->state=state.pose;else{cinematic::Key k;k.id=next_id++;k.time_ns=time;k.state=state.pose;keys.push_back(k);}
 if(track.replace(std::move(keys)))state.status="Dolly key captured from camera at current ReplayTime";
}
void clear_keys(){std::lock_guard lock(mutex);track.replace({});keys_replay_path.clear();if(state.mode==2)release("Dolly track deleted; native control restored");state.status="All dolly keys deleted";}
void timeline(std::uint64_t time,std::uint64_t duration,std::uint64_t anchor,bool play,double rate,bool link,const char* path){std::lock_guard lock(mutex);timeline_ns=time;duration_ns=duration;anchor_ns=anchor;playing=play;speed=rate;linked=link;replay_path=path?std::string(path,strnlen_s(path,260)):std::string{};host_heartbeat=clock_now();if(!link)release("Host disconnected; native control restored");}
bool owns_input(){return input_owned.load();}
void overlay_visible(bool visible){ui_visible=visible;}
void window(void* hwnd){game_window=static_cast<HWND>(hwnd);}
void mouse_delta(long x,long y){if(input_owned&&!ui_visible){mouse_x.fetch_add(x);mouse_y.fetch_add(y);}}
void fov(double value){std::lock_guard lock(mutex);if(state.enabled&&std::isfinite(value)&&value>0&&value<179)state.pose.fov_degrees=value;}
}
extern "C" int tm_camera_runtime_start(void*address){
 using namespace camera_runtime;
 const unsigned char expected[]={0x4c,0x8b,0x49,0x18,0x4c,0x8b,0xd1,0x8b,0x42,0x50,0x41,0x89,0x41,0x50,0x8b,0x42};unsigned char bytes[16];
 if(!read(reinterpret_cast<std::uintptr_t>(address),bytes,16)||memcmp(bytes,expected,16))return 0;
 if(MH_CreateHook(address,reinterpret_cast<void*>(hook),reinterpret_cast<void**>(&original))!=MH_OK)return 0;
 if(MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);return 0;}
 std::lock_guard lock(mutex);state.hook_ready=true;state.status="Native camera-copy observer ready; writes OFF";return 1;
}
extern "C" void tm_camera_game_context(int allowed){camera_runtime::game_allowed=allowed!=0;camera_runtime::game_heartbeat=camera_runtime::clock_now();}
extern "C" void tm_camera_runtime_stop(){camera_runtime::stop();}
extern "C" int tm_camera_runtime_diagnostic(char*out,std::size_t size){
 std::unique_lock lock(camera_runtime::mutex,std::try_to_lock);if(!lock.owns_lock()||!out||!size)return 0;
 const auto&s=camera_runtime::state;
 snprintf(out,size,"CAMERA_RUNTIME hook=%u observed=%u enabled=%u writing=%u mode=%u keys=%zu position=(%.3f,%.3f,%.3f) fov_deg=%.3f status=%s",s.hook_ready,s.observed,s.enabled,s.writing,s.mode,camera_runtime::track.keys().size(),s.pose.position[0],s.pose.position[1],s.pose.position[2],s.pose.fov_degrees,s.status.c_str());return 1;
}

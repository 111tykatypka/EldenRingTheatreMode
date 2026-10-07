// Independent camera write suppression at the statically verified CameraTools site.
// The native copy runs unchanged unless this callback successfully supplies an override.
#include <windows.h>
#include <realtimeapiset.h>
#pragma comment(lib,"mincore.lib")
#include <MinHook.h>
#include "CinematicCameraRuntime.h"
#include "CameraTelemetry.h"
#include "TheaterHotkeys.h"
#include "CameraProject.h"
#include "GameProfile.h"
#include "NativeCameraMemory.h"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <atomic>
#include <cstring>
#include <cstdio>
extern "C" {
void* tm_camera_original=nullptr;
void* tm_camera_continue=nullptr;
void tm_camera_detour();
}
namespace camera_runtime {
namespace {
std::mutex mutex;
View state;
cinematic::Track track;
cinematic::CameraCutTrack cut_track;
std::atomic_bool input_owned=false,ui_visible=true;
std::atomic<std::uint64_t> game_heartbeat=0;
std::atomic_bool game_allowed=false;
std::atomic_bool faulted=false;
std::atomic<HWND> game_window=nullptr;
std::atomic<long> mouse_x=0,mouse_y=0;
std::atomic<std::uint64_t> hook_calls=0,lock_skips=0,hook_cost_ns=0;
std::atomic<std::uint64_t> read_cost_ns=0,write_cost_ns=0,write_calls=0,hook_max_ns=0,interval_calls=0,interval_cost_ns=0;
std::uint64_t clock_now(){ULONGLONG t=0;QueryInterruptTimePrecise(&t);return t*100;}
std::uint64_t timeline_ns=0,duration_ns=0,anchor_ns=0,host_heartbeat=0,last_tick=0,probe_until=0,next_id=1;
bool playing=false,linked=false,reset=true;
double speed=1;
std::string replay_path,keys_replay_path;
std::uintptr_t owner=0,destination=0;
cinematic::Vec velocity{};
std::atomic<int> selected_bone=-1;
std::optional<cinematic::State> bone_pose;
std::uint64_t bone_time=0;
bool read(std::uintptr_t address,void*out,std::size_t bytes){SIZE_T n=0;return address>=0x10000&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out,bytes,&n)&&n==bytes;}
std::uint64_t time_at(std::uint64_t now){
 auto time=timeline_ns;
 if(playing&&now>=anchor_ns){auto delta=static_cast<long double>(now-anchor_ns)*speed;auto remaining=duration_ns>time?duration_ns-time:0;time+=static_cast<std::uint64_t>(std::min<long double>(delta,remaining));}
 return std::min(time,duration_ns);
}
void release(const char*reason){state.enabled=false;state.writing=false;input_owned=false;reset=true;probe_until=0;mouse_x=0;mouse_y=0;velocity={};state.status=reason;}
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
bool held(theater_hotkeys::Action action){return down(theater_hotkeys::Key(action));}
void move(double dt){
 if(ui_visible){mouse_x=0;mouse_y=0;velocity={};return;}
 auto turn=[&](int axis,double angle){cinematic::Quat q{0,0,0,std::cos(angle/2)};q[axis]=std::sin(angle/2);state.pose.orientation=*cinematic::normalized(product(state.pose.orientation,q));};
 using A=theater_hotkeys::Action;
 turn(1,(int(held(A::YawRight))-int(held(A::YawLeft)))*dt+std::clamp(mouse_x.exchange(0)*state.mouse_sensitivity,-.25,.25));
 turn(0,(int(held(A::PitchDown))-int(held(A::PitchUp)))*dt+std::clamp(mouse_y.exchange(0)*state.mouse_sensitivity,-.25,.25));
 turn(2,(int(held(A::RollRight))-int(held(A::RollLeft)))*dt);
 float m[16];encode(state.pose,m);
 if(held(A::ResetRoll)){double horizontal=std::hypot(m[8],m[10]);if(horizontal>1e-5){
  theater_camera::Slot slot;std::copy(m,m+16,slot.matrix);slot.fov=1;slot.aspect=1;slot.near_plane=.1f;slot.far_plane=1000;
  slot.matrix[0]=m[10]/horizontal;slot.matrix[1]=0;slot.matrix[2]=-m[8]/horizontal;
  slot.matrix[4]=-m[9]*m[8]/horizontal;slot.matrix[5]=horizontal;slot.matrix[6]=-m[9]*m[10]/horizontal;
  if(auto upright=decode(slot))state.pose.orientation=upright->orientation;encode(state.pose,m);}}
 cinematic::Vec delta{};double right=int(held(A::Right))-int(held(A::Left)),forward=int(held(A::Forward))-int(held(A::Backward)),up=int(held(A::Up))-int(held(A::Down));
 for(int i=0;i<3;++i)delta[i]=m[i]*right+m[8+i]*forward;delta[1]+=up;
 double n=cinematic::length(delta);if(n>1)delta=cinematic::mul(delta,1/n);
 auto desired=cinematic::mul(delta,state.movement_speed*(held(A::Fast)?5:held(A::Slow)?.1:1));
 state.pose.fov_degrees=std::clamp(state.pose.fov_degrees+(int(held(A::FovUp))-int(held(A::FovDown)))*20*dt,1.,178.);
 if(held(A::ResetFov))state.pose.fov_degrees=60;
 static bool faster=false,slower=false;bool fastNow=held(A::SpeedUp),slowNow=held(A::SpeedDown);
 if(fastNow&&!faster)state.movement_speed=std::min(state.movement_speed*1.25,1e6);if(slowNow&&!slower)state.movement_speed=std::max(state.movement_speed/1.25,1e-6);faster=fastNow;slower=slowNow;
 velocity=cinematic::mix(velocity,desired,state.smoothing_seconds>0?1-std::exp(-dt/state.smoothing_seconds):1);
 state.pose.position=cinematic::add(state.pose.position,cinematic::mul(velocity,dt));
}
bool update(void* output,void* source){
 std::unique_lock lock(mutex,std::try_to_lock);if(!lock.owns_lock()){++lock_skips;return false;}
 const auto now=clock_now();std::uintptr_t out=reinterpret_cast<std::uintptr_t>(output);
 theater_camera::Slot native;
 auto read_start=clock_now();bool memory_ok=camera_local_read(source,native.matrix);read_cost_ns.fetch_add(clock_now()-read_start,std::memory_order_relaxed);
 if(!memory_ok){state.observed=false;release("Camera output unavailable; native control restored");return false;}
 auto pose=decode(native);if(!pose){state.observed=false;release("Invalid matrix/FOV; native control restored");return false;}
 state.observed=true;state.timestamp_ns=now;
 auto effective_mode=state.cuts_enabled?static_cast<unsigned>(cut_track.evaluate(time_at(now))):state.mode;
 if(!state.enabled||effective_mode==0){state.pose=*pose;state.writing=false;input_owned=false;reset=true;last_tick=now;return false;}
 // RegisterHotKey can route F6 to the host instead of the game window. Poll the
 // shared emergency key here too, before any camera write.
 if(down(theater_hotkeys::Key(theater_hotkeys::Action::StopRecording))){release("Emergency Stop key; native camera control restored");state.mode=0;return false;}
 if(!game_allowed||now-game_heartbeat.load()>1000000000ULL||!linked||now-host_heartbeat>1000000000ULL){release("Player/offline/host context lost; native control restored");return false;}
 auto focused=GetForegroundWindow();auto game=game_window.load();DWORD pid=0;GetWindowThreadProcessId(focused,&pid);
 if(!game||!focused||pid!=GetCurrentProcessId()||GetAncestor(focused,GA_ROOT)!=GetAncestor(game,GA_ROOT)){release("Game focus lost; native control restored");return false;}
 if(probe_until&&now>=probe_until){release("Two-second camera probe finished; native control restored");return false;}
 if(reset){owner=reinterpret_cast<std::uintptr_t>(source);destination=out;state.pose=*pose;if(probe_until)state.pose.position[0]+=.25;reset=false;last_tick=now;}
 if(owner!=reinterpret_cast<std::uintptr_t>(source)||destination!=out){release("Camera generation changed; native control restored");return false;}
 double dt=std::min(double(now-last_tick)/1e9,.05);last_tick=now;
 if(!probe_until){
  if(effective_mode==1)move(dt);
  else if(effective_mode==3){
   if(!bone_pose||now<bone_time||now-bone_time>250000000ULL){release("Bone camera target unavailable or stale; native camera restored");return false;}
   float basis[16];encode(*bone_pose,basis);state.pose.orientation=bone_pose->orientation;state.pose.position=bone_pose->position;
   for(int i=0;i<3;++i)for(int j=0;j<3;++j)state.pose.position[i]+=basis[j*4+i]*state.bone_offset[j];
  }
  else if(!track.keys().empty()&&keys_replay_path!=replay_path){release("Dolly keys belong to another replay; clear them before creating a new path");return false;}
  else if(auto value=track.evaluate(time_at(now))){
   if(!state.writing&&cinematic::length(cinematic::sub(value->position,state.pose.position))>20){release("Dolly start exceeds 20 units; move camera near the path first");return false;}state.pose=*value;
  } else {release("Dolly cut has no path; native camera restored");return false;}
 }
 auto rendered=state.pose;
 // Pure timeline-based oscillation: seek/pause produce the same result and
 // shake never accumulates into the editable pose or captured camera nodes.
 if(!probe_until&&state.shake_frequency>0&&(state.shake_position>0||state.shake_rotation>0)){
  double t=double(time_at(now))/1e9;
  for(int i=0;i<3;++i){double phase=t*state.shake_frequency*6.283185307179586*(1+i*.173)+i*2.1;
   if(!std::isfinite(phase)){release("Shake phase overflow; native camera restored");return false;}double wave=std::sin(phase);
   rendered.position[i]+=wave*state.shake_position;cinematic::Quat q{0,0,0,1};double angle=wave*(state.shake_rotation*(3.141592653589793/180));q[i]=std::sin(angle/2);q[3]=std::cos(angle/2);auto rotation=cinematic::normalized(product(rendered.orientation,q));if(!rotation){release("Invalid shake orientation; native camera restored");return false;}rendered.orientation=*rotation;}
 }
 float matrix[16];encode(rendered,matrix);float fov=static_cast<float>(rendered.fov_degrees*3.141592653589793/180);
 if(!cinematic::valid(state.pose)||!std::all_of(matrix,matrix+16,[](float v){return std::isfinite(v);})) {release("Invalid camera output; native control restored");return false;}
 auto write_start=clock_now();bool written=camera_local_write(output,matrix,fov);write_cost_ns.fetch_add(clock_now()-write_start,std::memory_order_relaxed);++write_calls;
 if(!written){release("Camera write failed; native control restored on next native copy");return false;}
 if(!state.writing)state.status=probe_until?"Two-second offset probe active":"Experimental camera override active (runtime unverified)";
 state.writing=true;input_owned=true;
 return true;
}

}
View view(){std::lock_guard lock(mutex);auto copy=state;copy.keys=track.keys();copy.cuts=cut_track.cuts();if(faulted){copy.enabled=false;copy.writing=false;copy.status="Camera backend faulted; overrides disabled until process restart";}return copy;}
std::optional<cinematic::State> decode_candidate(const theater_camera::Slot&slot){return decode(slot);}
void encode_pose(const cinematic::State&pose,float*matrix){encode(pose,matrix);}
void mode(unsigned value){std::lock_guard lock(mutex);state.mode=value%4;state.writing=false;input_owned=false;probe_until=0;if(state.mode==0)reset=true;state.status=state.enabled?"Camera mode changed":"Camera selected; enable experimental writes to apply";}
void enable(bool value){std::lock_guard lock(mutex);if(!value){release("Camera overrides disabled; native control restored");return;}if(faulted||!state.hook_ready||!state.observed||clock_now()-state.timestamp_ns>500000000ULL||!game_allowed||!linked){release("Cannot arm: fresh camera, offline player and host required; backend must not be faulted");return;}
 if(state.mode==2&&(track.keys().empty()||keys_replay_path!=replay_path)){release("Capture or load Dolly keys for this replay before selecting Dolly");return;}
 if(state.mode==3&&(selected_bone<0||!bone_pose||clock_now()-bone_time>250000000ULL)){release("Select an available player bone before selecting Bone camera");return;}
 if(!state.enabled)reset=true;state.enabled=true;state.status="Experimental writes armed";}
void stop(){std::lock_guard lock(mutex);release("Camera stopped; native control restored");state.mode=0;state.cuts_enabled=false;}
void probe(){enable(true);std::lock_guard lock(mutex);if(state.enabled){state.mode=1;probe_until=clock_now()+2000000000ULL;}}
void add_key(){std::lock_guard lock(mutex);const auto now=clock_now();if(!state.observed||now-state.timestamp_ns>500000000ULL||!linked||now-host_heartbeat>1000000000ULL){state.status="Cannot add key: fresh native camera and host timeline required";return;}
 if(!duration_ns||replay_path.empty()){state.status="Load a replay before adding Dolly keys";return;}
 if(!track.keys().empty()&&keys_replay_path!=replay_path){state.status="Keys belong to another replay; confirm deletion first";return;}
 if(keys_replay_path!=replay_path)++state.project_generation;keys_replay_path=replay_path;
 auto keys=track.keys();auto time=time_at(now);auto found=std::find_if(keys.begin(),keys.end(),[&](const auto&k){return k.time_ns==time;});
 if(found!=keys.end())found->state=state.pose;else{cinematic::Key k;k.id=next_id++;k.time_ns=time;k.state=state.pose;keys.push_back(k);}
 if(track.replace(std::move(keys)))state.status="Dolly key captured from camera at current ReplayTime";
}
void clear_keys(){std::lock_guard lock(mutex);track.replace({});keys_replay_path.clear();++state.project_generation;if(state.mode==2)release("Dolly track deleted; native control restored");state.status="All dolly keys deleted";}
void edit_key(cinematic::Key key){
 std::vector<cinematic::Key> keys;std::string identity;std::uint64_t duration;
 {std::lock_guard lock(mutex);keys=track.keys();identity=keys_replay_path;duration=duration_ns;}
 auto at=std::find_if(keys.begin(),keys.end(),[&](const auto&k){return k.id==key.id;});
 cinematic::Track next;bool ok=at!=keys.end()&&key.time_ns<=duration;
 if(ok){*at=key;ok=next.replace(std::move(keys));}
 std::lock_guard lock(mutex);if(ok&&identity==keys_replay_path&&identity==replay_path){track=std::move(next);state.status="Dolly key updated";}else state.status="Invalid key, duplicate timestamp, or replay changed; edit rejected";
}
void delete_key(std::uint64_t id){std::vector<cinematic::Key> keys;std::string identity;
 {std::lock_guard lock(mutex);keys=track.keys();identity=keys_replay_path;}
 keys.erase(std::remove_if(keys.begin(),keys.end(),[&](const auto&k){return k.id==id;}),keys.end());cinematic::Track next;if(!next.replace(std::move(keys)))return;
 std::lock_guard lock(mutex);if(identity!=keys_replay_path)return;track=std::move(next);if(track.keys().empty()&&state.mode==2)release("Last Dolly key deleted; native camera restored");state.status="Dolly key deleted";
}
void save_path(){std::string identity,text;{std::lock_guard lock(mutex);identity=keys_replay_path;if(identity.empty()||identity!=replay_path){state.status="No camera track for the loaded replay";return;}text=cinematic::save_project(identity,track.keys());}
 bool ok=false;try{auto path=std::filesystem::u8path(identity+".ercam");auto temp=path;temp+=L".tmp";
  {std::ofstream file(temp,std::ios::binary|std::ios::trunc);file.write(text.data(),text.size());file.flush();ok=bool(file);}
  if(ok)ok=MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
 }catch(...){ok=false;}std::lock_guard lock(mutex);state.status=ok?"Camera path saved beside replay (.ercam)":"Camera path save failed";
}
void load_path(){std::string identity;std::uint64_t duration;{std::lock_guard lock(mutex);identity=replay_path;duration=duration_ns;}
 bool ok=false;cinematic::Track next;try{if(!identity.empty()){auto path=std::filesystem::u8path(identity+".ercam");auto size=std::filesystem::file_size(path);
  // Resource guard for untrusted text import, not a recording duration limit.
  if(size<=16*1024*1024){std::ifstream file(path,std::ios::binary);std::string text(static_cast<std::size_t>(size),'\0');file.read(text.data(),text.size());std::vector<cinematic::Key> keys;
   ok=bool(file)&&cinematic::load_project(text,identity,keys)&&std::all_of(keys.begin(),keys.end(),[&](const auto&k){return k.time_ns<=duration&&k.id<UINT64_MAX;});if(ok)ok=next.replace(std::move(keys));}}
 }catch(...){ok=false;}
 std::lock_guard lock(mutex);if(ok&&identity==replay_path){release("Camera track loaded; writes OFF");track=std::move(next);keys_replay_path=identity;++state.project_generation;next_id=1;for(auto&k:track.keys())next_id=std::max(next_id,k.id+1);}else state.status="Camera load failed: missing, invalid, oversized or incompatible .ercam";
}
void movement(double value,double sensitivity,double smoothing){std::lock_guard lock(mutex);if(std::isfinite(value)&&value>0&&std::isfinite(sensitivity)&&sensitivity>0&&std::isfinite(smoothing)&&smoothing>=0){state.movement_speed=value;state.mouse_sensitivity=sensitivity;state.smoothing_seconds=smoothing;}}
void bone(int index,cinematic::Vec offset){std::lock_guard lock(mutex);if(index>=-1&&cinematic::finite(offset)){state.bone_index=index;state.bone_offset=offset;selected_bone=index;bone_pose.reset();state.bone_available=false;}}
void shake(double position,double rotation,double frequency){std::lock_guard lock(mutex);if(std::isfinite(position)&&position>=0&&std::isfinite(rotation)&&rotation>=0&&std::isfinite(frequency)&&frequency>=0){state.shake_position=position;state.shake_rotation=rotation;state.shake_frequency=frequency;}}
std::optional<cinematic::State> bone_world(const float*root,const float*qs){
 if(!root||!qs)return {};theater_camera::Slot slot;std::copy(root,root+16,slot.matrix);slot.fov=1;slot.aspect=1;slot.near_plane=.1f;slot.far_plane=1000;
 auto world=decode(slot);if(!world||!std::all_of(qs,qs+12,[](float x){return std::isfinite(x);}))return {};
 cinematic::Quat rotation{qs[4],qs[5],qs[6],qs[7]};auto normalized=cinematic::normalized(rotation);if(!normalized)return {};
 for(int i=0;i<3;++i)for(int j=0;j<3;++j)world->position[i]+=root[j*4+i]*qs[j];
 world->orientation=*cinematic::normalized(product(world->orientation,*normalized));return world;
}
void timeline(std::uint64_t time,std::uint64_t duration,std::uint64_t anchor,bool play,double rate,bool link,const char* path){std::lock_guard lock(mutex);timeline_ns=time;duration_ns=duration;anchor_ns=anchor;playing=play;speed=rate;linked=link;auto identity=path?std::string(path,strnlen_s(path,260)):std::string{};if(identity!=replay_path){state.cuts_enabled=false;cut_track.replace({},duration);release("Replay changed; camera writes and cuts disabled");}replay_path=std::move(identity);host_heartbeat=clock_now();if(!link)release("Host disconnected; native control restored");}
bool owns_input(){return input_owned.load();}
void overlay_visible(bool visible){ui_visible=visible;}
void window(void* hwnd){game_window=static_cast<HWND>(hwnd);}
void mouse_delta(long x,long y){if(input_owned&&!ui_visible){mouse_x.fetch_add(x);mouse_y.fetch_add(y);}}
void fov(double value){std::lock_guard lock(mutex);if(state.enabled&&std::isfinite(value)&&value>0&&value<179)state.pose.fov_degrees=value;}
void cuts(bool enabled,std::vector<cinematic::CameraCut> values){std::lock_guard lock(mutex);if(enabled&&(track.keys().empty()||keys_replay_path!=replay_path)){state.status="Create this replay's Dolly path before enabling cuts";return;}if(!cut_track.replace(std::move(values),duration_ns)){state.status="Invalid cut: overlaps, duplicate IDs or outside replay";return;}state.cuts_enabled=enabled;state.writing=false;reset=true;state.status="Camera cuts updated; explicitly arm camera writes";}
}
extern "C" int tm_camera_bone_index(){return camera_runtime::selected_bone.load();}
extern "C" void tm_camera_bone_publish(const float* root,const float*qs){
 auto value=camera_runtime::bone_world(root,qs);std::unique_lock lock(camera_runtime::mutex,std::try_to_lock);if(!lock.owns_lock())return;
 camera_runtime::bone_pose=value;camera_runtime::bone_time=camera_runtime::clock_now();camera_runtime::state.bone_available=value.has_value();
}
extern "C" int tm_camera_intercept(void* output,void* source){
 using namespace camera_runtime;if(faulted)return 0;
 const auto start=clock_now();++hook_calls;bool applied=false;
 try{applied=update(output,source);}catch(...){faulted=true;input_owned=false;}
 auto elapsed=clock_now()-start;hook_cost_ns.fetch_add(elapsed,std::memory_order_relaxed);++interval_calls;interval_cost_ns.fetch_add(elapsed,std::memory_order_relaxed);auto high=hook_max_ns.load();while(elapsed>high&&!hook_max_ns.compare_exchange_weak(high,elapsed)){}return applied?1:0;
}
extern "C" int tm_camera_runtime_start(void* address){
 using namespace camera_runtime;
 auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
 const unsigned char expected[]=TM_CAMERA_INTERCEPT_BYTES;unsigned char bytes[sizeof(expected)];
 static_assert(sizeof(expected)==TM_VAL_CAMERA_CONTINUE_RVA-TM_VAL_CAMERA_INTERCEPT_RVA);
 if(reinterpret_cast<std::uintptr_t>(address)!=base+TM_VAL_CAMERA_INTERCEPT_RVA||!read(reinterpret_cast<std::uintptr_t>(address),bytes,sizeof(bytes))||memcmp(bytes,expected,sizeof(bytes)))return 0;
 tm_camera_continue=reinterpret_cast<void*>(base+TM_VAL_CAMERA_CONTINUE_RVA);
 if(MH_CreateHook(address,reinterpret_cast<void*>(tm_camera_detour),&tm_camera_original)!=MH_OK)return 0;
 if(MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);tm_camera_original=nullptr;return 0;}
 std::lock_guard lock(mutex);state.hook_ready=true;state.status="CameraTools-equivalent native interception ready; writes OFF";return 1;
}
extern "C" void tm_camera_game_context(int allowed){camera_runtime::game_allowed=allowed!=0;camera_runtime::game_heartbeat=camera_runtime::clock_now();}
extern "C" void tm_camera_runtime_stop(){camera_runtime::stop();}
extern "C" int tm_camera_runtime_diagnostic(char*out,std::size_t size){
 std::unique_lock lock(camera_runtime::mutex,std::try_to_lock);if(!lock.owns_lock()||!out||!size)return 0;
 const auto&s=camera_runtime::state;
 const auto calls=camera_runtime::hook_calls.load();
 const auto writes=camera_runtime::write_calls.load(),window_calls=camera_runtime::interval_calls.exchange(0),window_cost=camera_runtime::interval_cost_ns.exchange(0);
 snprintf(out,size,"CAMERA_RUNTIME backend=C7_local_store hook=%u observed=%u enabled=%u writing=%u mode=%u keys=%zu position=(%.3f,%.3f,%.3f) fov_deg=%.3f callbacks=%llu lock_skips=%llu mean_hook_us=%.2f window_callbacks=%llu window_mean_hook_us=%.2f max_hook_us=%.2f mean_local_read_us=%.2f mean_local_write_us=%.2f status=%s",s.hook_ready,s.observed,s.enabled,s.writing,s.mode,camera_runtime::track.keys().size(),s.pose.position[0],s.pose.position[1],s.pose.position[2],s.pose.fov_degrees,static_cast<unsigned long long>(calls),static_cast<unsigned long long>(camera_runtime::lock_skips.load()),calls?double(camera_runtime::hook_cost_ns.load())/calls/1000:0,static_cast<unsigned long long>(window_calls),window_calls?double(window_cost)/window_calls/1000:0,double(camera_runtime::hook_max_ns.load())/1000,calls?double(camera_runtime::read_cost_ns.load())/calls/1000:0,writes?double(camera_runtime::write_cost_ns.load())/writes/1000:0,s.status.c_str());return 1;
}

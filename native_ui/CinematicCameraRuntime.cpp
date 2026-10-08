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
#include <unordered_set>
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
std::uint64_t shake_epoch=0;
bool playing=false,linked=false,reset=true;
std::vector<TargetSample> recorded_player_targets;
double speed=1;
std::string replay_path,keys_replay_path;
struct HistoryEntry {std::string replay,keys_replay;std::vector<cinematic::Key> keys;std::vector<cinematic::CameraCut> cuts;cinematic::TrackSettings settings;};
std::vector<HistoryEntry> undo_stack,redo_stack;
bool edit_group=false,edit_saved=false;
HistoryEntry history_entry(){return {replay_path,keys_replay_path,track.keys(),cut_track.cuts(),state.track_settings};}
void remember(bool edit=false){
 if(edit&&edit_group&&edit_saved)return;
 if(!edit){edit_group=false;edit_saved=false;}
 if(undo_stack.size()>=64)undo_stack.erase(undo_stack.begin());
 undo_stack.push_back(history_entry());redo_stack.clear();if(edit_group)edit_saved=true;
}
std::uintptr_t owner=0,destination=0;
std::atomic<double> fov_wheel{0};
std::atomic_bool free_input{false};
double fov_target=60;
cinematic::Vec rotation_pending{};
cinematic::Vec velocity{};
std::atomic<int> selected_bone=-1;
// Bone camera: the camera sits at a fixed place relative to the chosen bone (position in the bone's axes, orientation relative to
// the bone) and is steered with the ordinary free-camera controls, so it follows the bone while still being movable and turnable.
cinematic::Vec bone_local_pos{0,0,-1};cinematic::Quat bone_local_rot{0,0,0,1};bool bone_local_init=false;
std::vector<std::string> bone_name_list;
std::vector<std::array<float,3>> bone_dot_list;std::uint64_t bone_dot_time=0;std::atomic_bool bone_dots_wanted=false;
std::optional<cinematic::State> bone_pose;
std::uint64_t bone_time=0;
bool read(std::uintptr_t address,void*out,std::size_t bytes){SIZE_T n=0;return address>=0x10000&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out,bytes,&n)&&n==bytes;}
std::uint64_t time_at(std::uint64_t now){
 auto time=timeline_ns;
 if(playing&&duration_ns&&now>=anchor_ns){const auto delta=static_cast<long double>(now-anchor_ns)*speed;const auto step=static_cast<std::uint64_t>(std::min<long double>(delta,UINT64_MAX));const auto remaining=duration_ns>time?duration_ns-time:0;time=step>=remaining?(step-remaining)%duration_ns:time+step;}
 return std::min(time,duration_ns);
}
void release(const char*reason){state.enabled=false;state.writing=false;input_owned=false;free_input=false;reset=true;probe_until=0;mouse_x=0;mouse_y=0;fov_wheel=0;velocity={};state.status=reason;}
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
 if(ui_visible||down(VK_MENU)){mouse_x=0;mouse_y=0;fov_wheel=0;velocity={};rotation_pending={};return;}
 using A=theater_hotkeys::Action;
 const double yaw=(int(held(A::YawRight))-int(held(A::YawLeft)))*dt+std::clamp(mouse_x.exchange(0)*state.mouse_sensitivity,-.25,.25);
 const double pitch=(int(held(A::PitchDown))-int(held(A::PitchUp)))*dt+std::clamp(mouse_y.exchange(0)*state.mouse_sensitivity,-.25,.25);
 const double roll=(int(held(A::RollRight))-int(held(A::RollLeft)))*dt;
 rotation_pending=cinematic::add(rotation_pending,{yaw,pitch,roll});
 const auto applied=cinematic::take_smoothed_angles(rotation_pending,dt,state.rotation_smoothing_seconds);
 if(auto orientation=cinematic::mouse_look(state.pose.orientation,applied[0],applied[1],applied[2]))state.pose.orientation=*orientation;
 float m[16];encode(state.pose,m);
 if(held(A::ResetRoll)){double horizontal=std::hypot(m[8],m[10]);if(horizontal>1e-5){
  theater_camera::Slot slot;std::copy(m,m+16,slot.matrix);slot.fov=1;slot.aspect=1;slot.near_plane=.1f;slot.far_plane=1000;
  slot.matrix[0]=m[10]/horizontal;slot.matrix[1]=0;slot.matrix[2]=-m[8]/horizontal;
  slot.matrix[4]=-m[9]*m[8]/horizontal;slot.matrix[5]=horizontal;slot.matrix[6]=-m[9]*m[10]/horizontal;
  if(auto upright=decode(slot))state.pose.orientation=upright->orientation;rotation_pending={};encode(state.pose,m);}}
 cinematic::Vec delta{};double right=int(held(A::Right))-int(held(A::Left)),forward=int(held(A::Forward))-int(held(A::Backward)),up=int(held(A::Up))-int(held(A::Down));
 for(int i=0;i<3;++i)delta[i]=m[i]*right+m[8+i]*forward;delta[1]+=up;
 double n=cinematic::length(delta);if(n>1)delta=cinematic::mul(delta,1/n);
 auto desired=cinematic::mul(delta,state.movement_speed*(held(A::Fast)?5:held(A::Slow)?.1:1));
 fov_target=cinematic::wheel_fov(fov_target,fov_wheel.exchange(0),held(A::Slow)?.1:held(A::Fast)?.25:1.);
 fov_target=std::clamp(fov_target+(int(held(A::FovUp))-int(held(A::FovDown)))*20*dt,1.,178.);
 if(held(A::ResetFov))fov_target=state.pose.fov_degrees=60;
 state.pose.fov_degrees=cinematic::smooth_fov(state.pose.fov_degrees,fov_target,dt);
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
 state.observed=true;state.timestamp_ns=now;state.native_near_plane=native.near_plane;
 auto effective_mode=state.cuts_enabled?static_cast<unsigned>(cut_track.evaluate(time_at(now))):state.mode;
 if(!state.enabled||effective_mode==0){state.pose=*pose;state.writing=false;input_owned=false;free_input=false;reset=true;last_tick=now;return false;}
 // RegisterHotKey can route F6 to the host instead of the game window. Poll the
 // shared emergency key here too, before any camera write.
 if(down(theater_hotkeys::Key(theater_hotkeys::Action::StopRecording))){release("Emergency Stop key; native camera control restored");state.mode=0;return false;}
 if(!game_allowed||now-game_heartbeat.load()>1000000000ULL||!linked||now-host_heartbeat>1000000000ULL){release("Player/offline/host context lost; native control restored");return false;}
 auto focused=GetForegroundWindow();auto game=game_window.load();DWORD pid=0;GetWindowThreadProcessId(focused,&pid);
 if(!game||!focused||pid!=GetCurrentProcessId()||GetAncestor(focused,GA_ROOT)!=GetAncestor(game,GA_ROOT)){release("Game focus lost; native control restored");return false;}
 if(probe_until&&now>=probe_until){release("Two-second camera probe finished; native control restored");return false;}
 if(reset){owner=reinterpret_cast<std::uintptr_t>(source);destination=out;state.pose=*pose;rotation_pending={};fov_target=pose->fov_degrees;fov_wheel=0;if(probe_until)state.pose.position[0]+=.25;reset=false;last_tick=now;}
 if(owner!=reinterpret_cast<std::uintptr_t>(source)||destination!=out){release("Camera generation changed; native control restored");return false;}
 double dt=std::min(double(now-last_tick)/1e9,.05);last_tick=now;
 if(!probe_until){
  if(effective_mode==1||(effective_mode==2&&!state.dolly_preview&&!state.cuts_enabled)){
   // Authoring keeps its own pose through replay play/pause and seeks. Only
   // explicit path preview/cuts may evaluate the path into the active camera.
   move(dt);
  }
  else if(effective_mode==3){
   if(!bone_pose||now<bone_time||now-bone_time>250000000ULL){release("Bone camera target unavailable or stale; native camera restored");return false;}
   const auto bp=*bone_pose;float basis[16];encode(bp,basis);
   if(!bone_local_init){bone_local_pos=state.bone_offset;bone_local_rot={0,0,0,1};bone_local_init=true;}
   // 1. place the camera from the bone and the stored local pose; 2. let the free-camera controls move/turn it in the world;
   // 3. store the result back relative to the bone, so the next frame follows the bone's new transform.
   {cinematic::Vec p=bp.position;for(int i=0;i<3;++i)for(int j=0;j<3;++j)p[i]+=basis[j*4+i]*bone_local_pos[j];
    state.pose.position=p;if(auto o=cinematic::normalized(product(bp.orientation,bone_local_rot)))state.pose.orientation=*o;}
   move(dt);
   {cinematic::Vec d=cinematic::sub(state.pose.position,bp.position);for(int j=0;j<3;++j)bone_local_pos[j]=basis[j*4]*d[0]+basis[j*4+1]*d[1]+basis[j*4+2]*d[2];
    if(auto r=cinematic::normalized(product(cinematic::conjugate(bp.orientation),state.pose.orientation)))bone_local_rot=*r;
    state.bone_offset=bone_local_pos;}
  }
  else if(!track.keys().empty()&&keys_replay_path!=replay_path){release("Dolly keys belong to another replay; clear them before creating a new path");return false;}
  else if(auto value=cinematic::dolly_evaluate(track,time_at(now),state.dolly_smoothing_seconds,[](cinematic::TargetType type,std::uint64_t,std::uint64_t time)->std::optional<cinematic::Vec>{
   if(type!=cinematic::TargetType::RecordedPlayer||recorded_player_targets.empty())return {};
   const auto& samples=recorded_player_targets;if(time<samples.front().time_ns||time>samples.back().time_ns)return {};
   auto hi=std::lower_bound(samples.begin(),samples.end(),time,[](const auto&s,auto t){return s.time_ns<t;});
   if(hi==samples.begin()||hi->time_ns==time)return hi->position;auto lo=hi-1;
   return cinematic::mix(lo->position,hi->position,double(time-lo->time_ns)/double(hi->time_ns-lo->time_ns));
  },&state.evaluation)){
   // An explicitly selected path preview may jump the camera to its authored
   // pose. It never moves the player. Keep the displacement guard for automatic
   // camera-cut ownership changes, plus all native/finite/context guards.
   if(!state.writing&&state.cuts_enabled&&cinematic::length(cinematic::sub(value->position,state.pose.position))>20){release("Dolly cut start exceeds 20 units; move camera near the path first");return false;}state.pose=*value;rotation_pending={};fov_target=value->fov_degrees;
  } else {release("Dolly cut has no path; native camera restored");return false;}
 }
 auto rendered=state.pose;
 // Real-time shake continues during pause and ignores world/replay timescale.
 // It never accumulates into the editable pose or captured camera nodes.
 if(!probe_until&&(effective_mode!=2||state.shake_dolly)&&state.shake_frequency>0&&(state.shake_position>0||state.shake_rotation>0)){
  if(!shake_epoch)shake_epoch=now;
  double t=double(now-shake_epoch)/1e9;
  for(int i=0;i<3;++i){double phase=t*state.shake_frequency*6.283185307179586*(1+i*.173)+i*2.1;
   if(!std::isfinite(phase)){release("Shake phase overflow; native camera restored");return false;}double wave=cinematic::shake_wave(t,i,state.shake_frequency,state.shake_speed,state.shake_smoothing_seconds);
   rendered.position[i]+=wave*state.shake_position;cinematic::Quat q{0,0,0,1};double angle=wave*(state.shake_rotation*(3.141592653589793/180));q[i]=std::sin(angle/2);q[3]=std::cos(angle/2);auto rotation=cinematic::normalized(product(rendered.orientation,q));if(!rotation){release("Invalid shake orientation; native camera restored");return false;}rendered.orientation=*rotation;}
 }
 float matrix[16];encode(rendered,matrix);float fov=static_cast<float>(rendered.fov_degrees*3.141592653589793/180);
 if(!cinematic::valid(state.pose)||!std::all_of(matrix,matrix+16,[](float v){return std::isfinite(v);})) {release("Invalid camera output; native control restored");return false;}
 const float near_plane=std::isfinite(native.far_plane)&&native.far_plane>state.near_plane?float(state.near_plane):0;
 auto write_start=clock_now();bool written=camera_local_write(output,matrix,fov,near_plane);write_cost_ns.fetch_add(clock_now()-write_start,std::memory_order_relaxed);++write_calls;
 if(!written){release("Camera write failed; native control restored on next native copy");return false;}
 if(!state.writing)state.status=probe_until?"Two-second offset probe active":"Experimental camera override active (runtime unverified)";
 state.writing=true;input_owned=true;free_input=(effective_mode==1||(effective_mode==2&&!state.dolly_preview&&!state.cuts_enabled))&&!probe_until;
 return true;
}

}
View view(bool include_keys){std::lock_guard lock(mutex);auto copy=state;copy.track_current=keys_replay_path==replay_path&&!track.keys().empty();copy.key_count=track.keys().size();if(include_keys){copy.keys=track.keys();copy.cuts=cut_track.cuts();}if(faulted){copy.enabled=false;copy.writing=false;copy.status="Camera backend faulted; overrides disabled until process restart";}return copy;}
std::vector<std::string> bone_names(){std::lock_guard lock(mutex);return bone_name_list;}
void set_bone_dots(bool wanted){bone_dots_wanted=wanted;}
std::optional<std::uint64_t> replay_time(){std::lock_guard lock(mutex);if(!linked||!duration_ns)return std::nullopt;return time_at(clock_now());}
std::vector<std::array<float,3>> bone_dots(){std::lock_guard lock(mutex);if(clock_now()-bone_dot_time>500000000ULL)return {};return bone_dot_list;}
std::optional<cinematic::State> decode_candidate(const theater_camera::Slot&slot){return decode(slot);}
void encode_pose(const cinematic::State&pose,float*matrix){encode(pose,matrix);}
void mode(unsigned value){std::lock_guard lock(mutex);state.mode=value%4;state.cuts_enabled=false;if(state.mode==2)state.dolly_preview=false;rotation_pending={};mouse_x=0;mouse_y=0;velocity={};fov_target=state.pose.fov_degrees;fov_wheel=0;state.writing=false;input_owned=false;free_input=false;probe_until=0;if(state.mode==0)reset=true;state.status=state.enabled?"Camera mode changed":"Camera selected; enable experimental writes to apply";}
void enable(bool value){std::lock_guard lock(mutex);if(!value){release("Camera overrides disabled; native control restored");return;}if(faulted||!state.hook_ready||!state.observed||clock_now()-state.timestamp_ns>500000000ULL||!game_allowed||!linked){release("Cannot arm: fresh camera, offline player and host required; backend must not be faulted");return;}
 if(state.mode==3&&(selected_bone<0||!bone_pose||clock_now()-bone_time>250000000ULL)){release("Select an available player bone before selecting Bone camera");return;}
 if(!state.enabled)reset=true;state.enabled=true;state.status="Experimental writes armed";}
void stop(){std::lock_guard lock(mutex);release("Camera stopped; native control restored");state.mode=0;state.cuts_enabled=false;state.dolly_preview=false;}
void probe(){enable(true);std::lock_guard lock(mutex);if(state.enabled){state.mode=1;probe_until=clock_now()+2000000000ULL;}}
void add_key(unsigned channels){std::lock_guard lock(mutex);if(!channels||(channels&~cinematic::AllChannels))return;if(state.mode!=2||!state.enabled||!state.writing){state.status="Select and activate Dolly camera before capturing a key";return;}const auto now=clock_now();if(!state.observed||now-state.timestamp_ns>500000000ULL||!linked||now-host_heartbeat>1000000000ULL){state.status="Cannot add key: fresh native camera and host timeline required";return;}
 if(!duration_ns||replay_path.empty()){state.status="Load a replay before adding Dolly keys";return;}
 if(!track.keys().empty()&&keys_replay_path!=replay_path){state.status="Keys belong to another replay; confirm deletion first";return;}
 if(keys_replay_path!=replay_path)++state.project_generation;keys_replay_path=replay_path;
 auto keys=track.keys();auto time=time_at(now);auto found=std::find_if(keys.begin(),keys.end(),[&](const auto&k){return k.time_ns==time;});
 auto captured=state.pose;auto [base,roll]=cinematic::split_roll(captured.orientation);
 captured.orientation=base;captured.roll_degrees=roll;
 for(auto at=keys.rbegin();at!=keys.rend();++at)if(at->time_ns<time&&(at->channels&cinematic::Roll)){captured.roll_degrees+=360*std::round((at->state.roll_degrees-captured.roll_degrees)/360);break;}
 if(found!=keys.end()){
  if(channels&cinematic::Position)found->state.position=state.pose.position;
  if(channels&cinematic::Rotation)found->state.orientation=captured.orientation;
  if(channels&cinematic::Fov)found->state.fov_degrees=state.pose.fov_degrees;
  if(channels&cinematic::Roll)found->state.roll_degrees=captured.roll_degrees;
  if(channels&cinematic::Focus)found->state.focus_distance=state.pose.focus_distance;
  if(channels&cinematic::Target)found->state.look_at_target=state.track_settings.target_position;
  found->channels|=channels;
 }else{cinematic::Key k;k.id=next_id++;k.time_ns=time;k.state=(channels&cinematic::Roll)?captured:state.pose;if(channels&cinematic::Rotation)k.state.orientation=captured.orientation;k.channels=channels;k.state.look_at_target=state.track_settings.target_position;keys.push_back(k);}
 cinematic::Track next=track;if(next.replace(std::move(keys),state.track_settings)){remember();track=std::move(next);++state.project_generation;state.status="Dolly key captured from camera at current ReplayTime";}
}
void duplicate_key(std::uint64_t id,std::uint64_t time){
 std::lock_guard lock(mutex);if(keys_replay_path!=replay_path||time>duration_ns)return;
 auto keys=track.keys();auto at=std::find_if(keys.begin(),keys.end(),[&](const auto&k){return k.id==id;});if(at==keys.end())return;
 auto key=*at;key.id=next_id;key.time_ns=time;keys.push_back(key);cinematic::Track next=track;
 if(!next.replace(std::move(keys),state.track_settings)){state.status="Duplicate rejected: overlapping channel at this timestamp";return;}
 remember();++next_id;track=std::move(next);++state.project_generation;state.status="Camera key duplicated at timeline cursor";
}
void track_settings(cinematic::TrackSettings settings){
 std::lock_guard lock(mutex);cinematic::Track next=track;
 if(!next.replace(track.keys(),settings)){state.status="Invalid motion settings, or constant-speed track contains a cut";return;}
 remember();state.track_settings=settings;track=std::move(next);++state.project_generation;state.status="Dolly motion settings updated";
}
void player_targets(const std::vector<TargetSample>& samples){
 std::lock_guard lock(mutex);if(samples.empty())return;if(samples.size()>64){recorded_player_targets.clear();return;}
 for(std::size_t i=0;i<samples.size();++i)if(!cinematic::finite(samples[i].position)||(i&&samples[i].time_ns<=samples[i-1].time_ns)){recorded_player_targets.clear();return;}
 recorded_player_targets=samples;
}
std::optional<std::uint64_t> player_target_query(){
 std::lock_guard lock(mutex);
 if(!linked||state.track_settings.target!=cinematic::TargetType::RecordedPlayer||(state.track_settings.rotation!=cinematic::RotationMode::LookAt&&state.track_settings.rotation!=cinematic::RotationMode::LookAtRoll))return {};
 return time_at(clock_now());
}
void clear_keys(){std::lock_guard lock(mutex);if(!track.keys().empty()||!cut_track.cuts().empty())remember();track.replace({});keys_replay_path.clear();++state.project_generation;if(state.mode==2){release("Dolly track deleted; native control restored");state.mode=0;}state.dolly_preview=false;state.cuts_enabled=false;cut_track.replace({},duration_ns);state.status="All dolly keys and camera cuts deleted";}
void edit_key(cinematic::Key key){
 std::vector<cinematic::Key> keys;std::string identity;std::uint64_t duration,generation;cinematic::TrackSettings settings;cinematic::Track source;
 {std::lock_guard lock(mutex);source=track;keys=track.keys();identity=keys_replay_path;duration=duration_ns;generation=state.project_generation;settings=state.track_settings;}
 auto at=std::find_if(keys.begin(),keys.end(),[&](const auto&k){return k.id==key.id;});
 cinematic::Track next=std::move(source);bool ok=at!=keys.end()&&key.time_ns<=duration;
 if(ok){*at=key;ok=next.replace(std::move(keys),settings);}
 std::lock_guard lock(mutex);if(ok&&identity==keys_replay_path&&identity==replay_path&&generation==state.project_generation){remember(true);track=std::move(next);++state.project_generation;state.status="Dolly key updated";}else state.status="Invalid key, duplicate timestamp, or track changed; edit rejected";
}
void delete_key(std::uint64_t id){delete_keys({id});}
void delete_keys(const std::vector<std::uint64_t>& ids){
 std::lock_guard lock(mutex);if(ids.empty()||keys_replay_path!=replay_path)return;
 auto keys=track.keys();const auto before=keys.size();
 std::erase_if(keys,[&](const auto&k){return std::find(ids.begin(),ids.end(),k.id)!=ids.end();});
 if(keys.size()==before)return;
 cinematic::Track next=track;if(!next.replace(std::move(keys),state.track_settings))return;
 remember();track=std::move(next);++state.project_generation;
 if(track.keys().empty()&&state.mode==2){state.dolly_preview=false;state.cuts_enabled=false;release("Last Dolly key deleted; native camera restored");}
 state.status="Selected Dolly keys deleted";
}
void set_interpolation(const std::vector<std::uint64_t>& ids,cinematic::Interpolation mode){
 std::lock_guard lock(mutex);
 if(ids.empty()||keys_replay_path!=replay_path||static_cast<unsigned>(mode)>static_cast<unsigned>(cinematic::Interpolation::Step))return;
 const std::unordered_set<std::uint64_t> selected(ids.begin(),ids.end());
 auto keys=track.keys();bool changed=false;
 for(auto&key:keys)if(selected.contains(key.id)){
  auto rotation=mode==cinematic::Interpolation::Linear?cinematic::RotationInterpolation::Slerp:mode==cinematic::Interpolation::Step?cinematic::RotationInterpolation::Step:cinematic::RotationInterpolation::Squad;
  auto scalar=mode==cinematic::Interpolation::Linear?cinematic::ScalarInterpolation::Linear:mode==cinematic::Interpolation::Step?cinematic::ScalarInterpolation::Step:cinematic::ScalarInterpolation::Smooth;
  if(key.outgoing!=mode||key.rotation_interpolation!=rotation||key.scalar_interpolation!=scalar){key.outgoing=mode;key.rotation_interpolation=rotation;key.scalar_interpolation=scalar;changed=true;}
 }
 if(!changed)return;
 cinematic::Track next=track;if(!next.replace(std::move(keys),state.track_settings)){state.status="Interpolation rejected: Step cuts require keyframe-time timing";return;}
 remember();track=std::move(next);++state.project_generation;state.status="Dolly interpolation updated for selected keys";
}
void begin_edit(){std::lock_guard lock(mutex);edit_group=true;edit_saved=false;}
void end_edit(){std::lock_guard lock(mutex);edit_group=false;edit_saved=false;}
namespace {
void restore_history(std::vector<HistoryEntry>&from,std::vector<HistoryEntry>&to,const char*label){
 if(from.empty()){state.status="No camera edit to restore";return;}
 const auto&entry=from.back();if(entry.replay!=replay_path){state.status="Camera history belongs to another replay";return;}
 cinematic::Track restored;cinematic::CameraCutTrack restored_cuts;
 if(!restored.replace(entry.keys,entry.settings)||!restored_cuts.replace(entry.cuts,duration_ns)){state.status="Camera history validation failed";return;}
 if(to.size()>=64)to.erase(to.begin());to.push_back(history_entry());
 keys_replay_path=entry.keys_replay;state.track_settings=entry.settings;track=std::move(restored);cut_track=std::move(restored_cuts);from.pop_back();
 for(const auto&key:track.keys())next_id=std::max(next_id,key.id+1);
 edit_group=edit_saved=false;state.dolly_preview=state.cuts_enabled=false;++state.project_generation;++state.history_generation;
 if(track.keys().empty()&&state.mode==2)release("Empty restored track; native camera restored");
 state.status=label;
}
}
void undo(){std::lock_guard lock(mutex);restore_history(undo_stack,redo_stack,"Camera edit undone (Alt+Z)");}
void redo(){std::lock_guard lock(mutex);restore_history(redo_stack,undo_stack,"Camera edit redone (Alt+Shift+Z)");}
void save_path(){std::string identity,text;{std::lock_guard lock(mutex);identity=keys_replay_path;if(identity.empty()||identity!=replay_path){state.status="No camera track for the loaded replay";return;}text=cinematic::save_project(identity,track.keys(),state.track_settings);}
 bool ok=false;try{auto path=std::filesystem::u8path(identity+".ercam");auto temp=path;temp+=L".tmp";
  {std::ofstream file(temp,std::ios::binary|std::ios::trunc);file.write(text.data(),text.size());file.flush();ok=bool(file);}
  if(ok)ok=MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
 }catch(...){ok=false;}std::lock_guard lock(mutex);state.status=ok?"Camera path saved beside replay (.ercam)":"Camera path save failed";
}
void load_path(){std::string identity;std::uint64_t duration;{std::lock_guard lock(mutex);identity=replay_path;duration=duration_ns;}
 bool ok=false;cinematic::Track next;cinematic::TrackSettings settings;try{if(!identity.empty()){auto path=std::filesystem::u8path(identity+".ercam");auto size=std::filesystem::file_size(path);
  // Resource guard for untrusted text import, not a recording duration limit.
  if(size<=16*1024*1024){std::ifstream file(path,std::ios::binary);std::string text(static_cast<std::size_t>(size),'\0');file.read(text.data(),text.size());std::vector<cinematic::Key> keys;
   ok=bool(file)&&cinematic::load_project(text,identity,keys,&settings)&&std::all_of(keys.begin(),keys.end(),[&](const auto&k){return k.time_ns<=duration&&k.id<UINT64_MAX;});if(ok)ok=next.replace(std::move(keys),settings);}}
 }catch(...){ok=false;}
 std::lock_guard lock(mutex);if(ok&&identity==replay_path){remember();release("Camera track loaded; writes OFF");state.track_settings=settings;track=std::move(next);keys_replay_path=identity;++state.project_generation;next_id=1;for(auto&k:track.keys())next_id=std::max(next_id,k.id+1);}else state.status="Camera load failed: missing, invalid, oversized or incompatible .ercam";
}
void movement(double value,double sensitivity,double smoothing,double rotation_smoothing){std::lock_guard lock(mutex);if(std::isfinite(value)&&value>0&&std::isfinite(sensitivity)&&sensitivity>0&&std::isfinite(smoothing)&&smoothing>=0&&std::isfinite(rotation_smoothing)&&rotation_smoothing>=0){state.rotation_smoothing_seconds=rotation_smoothing;state.movement_speed=value;state.mouse_sensitivity=sensitivity;state.smoothing_seconds=smoothing;}}
void preview(bool enabled){std::lock_guard lock(mutex);
 if(enabled&&(state.mode!=2||track.keys().size()<2||keys_replay_path!=replay_path)){state.status="Path preview requires Dolly mode and at least two keys for this replay";return;}
 state.dolly_preview=enabled;rotation_pending={};velocity={};mouse_x=0;mouse_y=0;
 state.status=enabled?"Dolly path preview at ReplayTime":"Dolly authoring: move camera and capture keys";
}
void bone(int index,cinematic::Vec offset){std::lock_guard lock(mutex);if(index>=-1&&cinematic::finite(offset)){bone_local_init=false;state.bone_index=index;state.bone_offset=offset;selected_bone=index;bone_pose.reset();state.bone_available=false;}}
void dolly_smoothing(double seconds){std::lock_guard lock(mutex);if(std::isfinite(seconds)&&seconds>=0&&seconds<=2)state.dolly_smoothing_seconds=seconds;}
void high_quality_lods(bool enabled){std::lock_guard lock(mutex);state.high_quality_lods=enabled;}
void close_up(bool prevent,double near_plane){std::lock_guard lock(mutex);if(std::isfinite(near_plane)&&near_plane>=.001&&near_plane<=1){state.prevent_asset_fade=prevent;state.near_plane=near_plane;}}
void shake(double position,double rotation,double frequency,double speed,double smoothing,bool dolly){std::lock_guard lock(mutex);if(std::isfinite(position)&&position>=0&&position<=5&&std::isfinite(rotation)&&rotation>=0&&rotation<=30&&std::isfinite(frequency)&&frequency>=0&&frequency<=30&&std::isfinite(speed)&&speed>=0&&speed<=10&&std::isfinite(smoothing)&&smoothing>=0&&smoothing<=2){state.shake_position=position;state.shake_rotation=rotation;state.shake_frequency=frequency;state.shake_speed=speed;state.shake_smoothing_seconds=smoothing;state.shake_dolly=dolly;}}
std::optional<cinematic::State> bone_world(const float*root,const float*qs){
 if(!root||!qs)return {};theater_camera::Slot slot;std::copy(root,root+16,slot.matrix);slot.fov=1;slot.aspect=1;slot.near_plane=.1f;slot.far_plane=1000;
 auto world=decode(slot);if(!world||!std::all_of(qs,qs+12,[](float x){return std::isfinite(x);}))return {};
 cinematic::Quat rotation{qs[4],qs[5],qs[6],qs[7]};auto normalized=cinematic::normalized(rotation);if(!normalized)return {};
 for(int i=0;i<3;++i)for(int j=0;j<3;++j)world->position[i]+=root[j*4+i]*qs[j];
 world->orientation=*cinematic::normalized(product(world->orientation,*normalized));return world;
}
void timeline(std::uint64_t time,std::uint64_t duration,std::uint64_t anchor,bool play,double rate,bool link,const char* path){
 std::lock_guard lock(mutex);
 // Transport updates never switch authoring into path preview or change its
 // camera pose. K captures that pose at the same authoritative ReplayTime.
 timeline_ns=time;duration_ns=duration;anchor_ns=anchor;playing=play;speed=rate;linked=link;
 auto identity=path?std::string(path,strnlen_s(path,260)):std::string{};
 if(identity!=replay_path){undo_stack.clear();redo_stack.clear();edit_group=edit_saved=false;state.dolly_preview=false;state.cuts_enabled=false;cut_track.replace({},duration);release("Replay changed; camera writes and cuts disabled");}
 if(identity!=replay_path)recorded_player_targets.clear();
 replay_path=std::move(identity);host_heartbeat=clock_now();if(!link)release("Host disconnected; native control restored");
}
bool owns_input(){return input_owned.load();}
void overlay_visible(bool visible){ui_visible=visible;}
void window(void* hwnd){game_window=static_cast<HWND>(hwnd);}
void mouse_delta(long x,long y){if(input_owned&&!ui_visible){mouse_x.fetch_add(x);mouse_y.fetch_add(y);}}
bool mouse_wheel(double notches){if(!free_input||ui_visible||!std::isfinite(notches))return false;fov_wheel.fetch_add(notches);return true;}
void fov(double value){std::lock_guard lock(mutex);if(state.enabled&&std::isfinite(value)&&value>0&&value<179)fov_target=state.pose.fov_degrees=value;}
void cuts(bool enabled,std::vector<cinematic::CameraCut> values){std::lock_guard lock(mutex);if(enabled&&(track.keys().empty()||keys_replay_path!=replay_path)){state.status="Create this replay's Dolly path before enabling cuts";return;}if(!cut_track.replace(std::move(values),duration_ns)){state.status="Invalid cut: overlaps, duplicate IDs or outside replay";return;}state.cuts_enabled=enabled;state.writing=false;reset=true;state.status="Camera cuts updated; explicitly arm camera writes";}
}
extern "C" int tm_camera_bone_index(){return camera_runtime::selected_bone.load();}
extern "C" int tm_camera_asset_fade_requested(){
 using namespace camera_runtime;std::unique_lock lock(mutex,std::try_to_lock);if(!lock.owns_lock())return -1;
 const auto now=clock_now();const auto focused=GetForegroundWindow(),game=game_window.load();
 return state.prevent_asset_fade&&state.enabled&&state.writing&&(state.mode==1||state.mode==2)&&game_allowed&&linked&&now-host_heartbeat<1000000000ULL&&game&&focused&&GetAncestor(focused,GA_ROOT)==GetAncestor(game,GA_ROOT)?1:0;
}
extern "C" int tm_camera_quality_requested(){
 using namespace camera_runtime;std::unique_lock lock(mutex,std::try_to_lock);if(!lock.owns_lock())return 0;
 const auto now=clock_now();const auto focused=GetForegroundWindow(),game=game_window.load();
 return state.high_quality_lods&&state.enabled&&state.writing&&(state.mode==1||state.mode==2)&&game_allowed&&linked&&now-host_heartbeat<1000000000ULL&&game&&focused&&GetAncestor(focused,GA_ROOT)==GetAncestor(game,GA_ROOT)?1:0;
}
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

// Names of the player skeleton's bones, published by the adapter (one string per bone, index = bone index).
extern "C" void tm_camera_bone_names(const char*const*names,int count){
 std::lock_guard lock(camera_runtime::mutex);camera_runtime::bone_name_list.clear();
 if(!names||count<=0||count>1024)return;
 for(int i=0;i<count;++i)camera_runtime::bone_name_list.emplace_back(names[i]?names[i]:"");}

extern "C" int tm_camera_bone_dots_wanted(){return camera_runtime::bone_dots_wanted.load()?1:0;}
extern "C" void tm_camera_bone_positions(const float*xyz,int count){
 std::lock_guard lock(camera_runtime::mutex);camera_runtime::bone_dot_list.clear();
 if(!xyz||count<=0||count>1024)return;
 for(int i=0;i<count;++i)camera_runtime::bone_dot_list.push_back({xyz[i*3],xyz[i*3+1],xyz[i*3+2]});
 camera_runtime::bone_dot_time=camera_runtime::clock_now();}

#include "LightEditor.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <limits>
#include <map>
namespace light_editor {namespace {
struct Anim{bool on=false;std::vector<LightKey> keys;};
std::map<std::uint64_t,Anim> anims;
// A hand edit of an animated light holds it where it was put until the playhead moves away (so "move it, then add a key" works).
std::map<std::uint64_t,std::uint64_t> holds;std::uint64_t last_eval_time=0;
std::mutex mutex;View state;std::uint64_t next_id=1;TimeView time_state;
bool valid(const Light& l){return l.id&&unsigned(l.type)<=1&&cinematic::valid(l.transform)&&std::all_of(std::begin(l.rgb),std::end(l.rgb),[](float v){return std::isfinite(v)&&v>=0&&v<=1;})&&std::isfinite(l.radius)&&l.radius>0&&std::isfinite(l.intensity)&&l.intensity>=0&&std::isfinite(l.cone_degrees)&&l.cone_degrees>0&&l.cone_degrees<180&&std::isfinite(l.softness)&&l.softness>=0&&l.softness<=1&&l.shadow_level>=1&&l.shadow_level<=5&&std::isfinite(l.shadow_strength)&&l.shadow_strength>=0&&l.shadow_strength<=1&&std::isfinite(l.shadow_bias)&&l.shadow_bias>=-7&&l.shadow_bias<=7&&std::isfinite(l.source_radius)&&l.source_radius>=0&&std::isfinite(l.scattering)&&l.scattering>=0&&std::all_of(std::begin(l.specular_rgb),std::end(l.specular_rgb),[](float v){return std::isfinite(v)&&v>=0&&v<=1;});}
std::filesystem::path path(){wchar_t directory[32768]{};auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",directory,32768);if(!n||n>=32768)throw std::runtime_error("LOCALAPPDATA unavailable");return std::filesystem::path(directory)/L"EldenRingTheaterMode"/L"lights.ertlights";}
}
View view(){std::lock_guard lock(mutex);return state;}
bool snapshot(View& out){std::unique_lock lock(mutex,std::try_to_lock);if(!lock.owns_lock())return false;out=state;return true;}
void create(Type type,const cinematic::State& camera){if(!cinematic::valid(camera)||unsigned(type)>1)return;std::lock_guard lock(mutex);if(!next_id||next_id==std::numeric_limits<std::uint64_t>::max()){state.status="Light ID space exhausted";return;}Light light;light.id=next_id++;light.type=type;light.transform=camera;light.name=(type==Type::Point?"Point ":"Spot ")+std::to_string(light.id);state.selected=light.id;state.lights.push_back(light);state.status="Light created; renderer request queued";}
bool duplicate(std::uint64_t id){
 std::lock_guard lock(mutex);auto source=std::find_if(state.lights.begin(),state.lights.end(),[id](const auto& l){return l.id==id;});
 if(source==state.lights.end()||!valid(*source)||!next_id||next_id==std::numeric_limits<std::uint64_t>::max()){state.status="Cannot duplicate selected light";return false;}
 Light copy=*source;copy.id=next_id++;copy.name=source->name+" copy "+std::to_string(copy.id);
 const auto selected=copy.id;state.lights.push_back(std::move(copy));state.selected=selected;
 state.status="Light duplicated at the same transform; select its handle to move it";return true;
}
void select(std::uint64_t id){std::lock_guard lock(mutex);if(std::any_of(state.lights.begin(),state.lights.end(),[&](auto& l){return l.id==id;}))state.selected=id;}
void edit(Light light){if(!valid(light))return;light.transform.orientation=*cinematic::normalized(light.transform.orientation);std::lock_guard lock(mutex);for(auto& l:state.lights)if(l.id==light.id){{auto a=anims.find(light.id);if(a!=anims.end()&&a->second.on)holds[light.id]=last_eval_time;}l=std::move(light);state.status="Light updated; renderer request queued";return;}}
void remove(std::uint64_t id){std::lock_guard lock(mutex);anims.erase(id);std::erase_if(state.lights,[&](auto& l){return l.id==id;});if(state.selected==id)state.selected=state.lights.empty()?0:state.lights.back().id;state.status="Definition removed";}
void save(){std::lock_guard lock(mutex);try{auto p=path();std::filesystem::create_directories(p.parent_path());auto temp=p;temp+=L".tmp";std::ofstream f(temp,std::ios::binary|std::ios::trunc);f<<"ERTLIGHTS 2\n"<<std::setprecision(17);for(auto& l:state.lights){f<<l.id<<' '<<unsigned(l.type)<<' '<<std::quoted(l.name)<<' '<<l.enabled;for(auto v:l.transform.position)f<<' '<<v;for(auto v:l.transform.orientation)f<<' '<<v;for(auto v:l.rgb)f<<' '<<v;f<<' '<<l.radius<<' '<<l.intensity<<' '<<l.cone_degrees<<' '<<l.softness<<' '<<l.shadows<<' '<<l.shadow_level<<' '<<l.shadow_strength<<' '<<l.shadow_bias<<' '<<l.source_radius<<' '<<l.scattering;for(auto v:l.specular_rgb)f<<' '<<v;f<<'\n';}f.flush();if(!f)throw std::runtime_error("write failed");f.close();if(!MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("file replacement failed");state.status="Light setup saved";}catch(const std::exception& e){state.status=std::string("Save failed: ")+e.what();}}
void load(){std::lock_guard lock(mutex);try{std::ifstream f(path(),std::ios::binary);std::string magic;unsigned version=0;if(!(f>>magic>>version)||magic!="ERTLIGHTS"||(version!=1&&version!=2))throw std::runtime_error("unsupported or missing light file");std::vector<Light> parsed;std::uint64_t next=1;for(;;){f>>std::ws;if(f.eof())break;Light l;unsigned type=0;if(!(f>>l.id>>type>>std::quoted(l.name)>>l.enabled))throw std::runtime_error("invalid light entry");l.type=static_cast<Type>(type);for(auto& v:l.transform.position)f>>v;for(auto& v:l.transform.orientation)f>>v;for(auto& v:l.rgb)f>>v;f>>l.radius>>l.intensity>>l.cone_degrees>>l.softness;if(version>=2){f>>l.shadows>>l.shadow_level>>l.shadow_strength>>l.shadow_bias>>l.source_radius>>l.scattering;for(auto& v:l.specular_rgb)f>>v;}if(!f||!valid(l)||l.id==std::numeric_limits<std::uint64_t>::max()||std::any_of(parsed.begin(),parsed.end(),[&](auto& old){return old.id==l.id;}))throw std::runtime_error("invalid values or duplicate light ID");l.transform.orientation=*cinematic::normalized(l.transform.orientation);next=std::max(next,l.id+1);parsed.push_back(std::move(l));}state.lights=std::move(parsed);state.selected=state.lights.empty()?0:state.lights.front().id;next_id=next;state.status="Light setup loaded; enable rendering to apply";}catch(const std::exception& e){state.status=std::string("Load failed: ")+e.what();}}
bool animated(std::uint64_t id){std::lock_guard lock(mutex);auto it=anims.find(id);return it!=anims.end()&&it->second.on;}
void set_animated(std::uint64_t id,bool on){std::lock_guard lock(mutex);if(std::none_of(state.lights.begin(),state.lights.end(),[&](auto&l){return l.id==id;}))return;anims[id].on=on;state.status=on?"Light animation on: add keys at different times":"Light animation off: the light stays where it is";}
std::vector<LightKey> light_keys(std::uint64_t id){std::lock_guard lock(mutex);auto it=anims.find(id);return it==anims.end()?std::vector<LightKey>{}:it->second.keys;}
bool add_light_key(std::uint64_t id,std::uint64_t time_ns){
 std::lock_guard lock(mutex);auto light=std::find_if(state.lights.begin(),state.lights.end(),[&](auto&l){return l.id==id;});
 if(light==state.lights.end()||!cinematic::valid(light->transform))return false;
 auto&keys=anims[id].keys;LightKey key;key.time_ns=time_ns;key.transform=light->transform;
 auto at=std::lower_bound(keys.begin(),keys.end(),time_ns,[](const LightKey&k,std::uint64_t t){return k.time_ns<t;});
 if(at!=keys.end()&&at->time_ns==time_ns)*at=key;else keys.insert(at,key);
 holds.erase(id);
 state.status="Light key stored";return true;}
void remove_light_key(std::uint64_t id,std::uint64_t time_ns){std::lock_guard lock(mutex);auto it=anims.find(id);if(it!=anims.end())std::erase_if(it->second.keys,[&](auto&k){return k.time_ns==time_ns;});}
void clear_light_keys(std::uint64_t id){std::lock_guard lock(mutex);auto it=anims.find(id);if(it!=anims.end())it->second.keys.clear();}
namespace {
cinematic::Vec catmull(cinematic::Vec p0,cinematic::Vec p1,cinematic::Vec p2,cinematic::Vec p3,double t){
 cinematic::Vec r{};for(int i=0;i<3;++i){const double t2=t*t,t3=t2*t;r[i]=.5*((2*p1[i])+(-p0[i]+p2[i])*t+(2*p0[i]-5*p1[i]+4*p2[i]-p3[i])*t2+(-p0[i]+3*p1[i]-3*p2[i]+p3[i])*t3);}return r;}
cinematic::State sample(const std::vector<LightKey>&k,std::uint64_t t,cinematic::State base){
 if(k.empty())return base;
 if(t<=k.front().time_ns||k.size()==1){base.position=k.front().transform.position;base.orientation=k.front().transform.orientation;return base;}
 if(t>=k.back().time_ns){base.position=k.back().transform.position;base.orientation=k.back().transform.orientation;return base;}
 auto hi=std::upper_bound(k.begin(),k.end(),t,[](std::uint64_t v,const LightKey&key){return v<key.time_ns;});auto lo=hi-1;
 const double u=double(t-lo->time_ns)/double(std::max<std::uint64_t>(1,hi->time_ns-lo->time_ns));
 const auto p0=(lo==k.begin()?lo:lo-1)->transform.position,p3=(hi+1==k.end()?hi:hi+1)->transform.position;
 base.position=catmull(p0,lo->transform.position,hi->transform.position,p3,u);
 base.orientation=cinematic::slerp(lo->transform.orientation,hi->transform.orientation,u);return base;}
}
void evaluate_animation(std::uint64_t time_ns,std::uint64_t skip_id){
 std::lock_guard lock(mutex);
 last_eval_time=time_ns;
 for(auto it=holds.begin();it!=holds.end();){const auto d=time_ns>it->second?time_ns-it->second:it->second-time_ns;if(d>100000000ULL)it=holds.erase(it);else ++it;}
 for(auto&l:state.lights){if(l.id==skip_id||holds.contains(l.id))continue;auto it=anims.find(l.id);if(it==anims.end()||!it->second.on||it->second.keys.empty())continue;
  auto moved=sample(it->second.keys,time_ns,l.transform);if(cinematic::valid(moved))l.transform=moved;}}
TimeView time_view(){std::lock_guard lock(mutex);return time_state;}
void time(float hour){if(!std::isfinite(hour))return;hour=std::clamp(hour,0.f,23.f+59.f/60.f);int minutes=std::clamp(int(std::lround(hour*60)),0,1439);{std::lock_guard lock(mutex);time_state.requested=true;time_state.target=minutes/60.f;}tm_lighting_time_request(minutes);}
void restore_time(){{std::lock_guard lock(mutex);time_state.requested=false;}tm_lighting_time_request(-1);}
}
extern "C" void tm_lighting_time_observe(float hour,int status){using namespace light_editor;std::unique_lock lock(mutex,std::try_to_lock);if(!lock.owns_lock())return;time_state.available=status>0&&std::isfinite(hour)&&hour>=0&&hour<24;time_state.state=status;if(time_state.available)time_state.hour=hour;if(status==3||status<=0)time_state.requested=false;}

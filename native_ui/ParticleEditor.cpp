#include "ParticleEditor.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <iterator>
namespace particle_editor { namespace {
std::mutex mutex; View state; std::uint64_t next_id = 1;
constexpr Preset kPresets[] = {
 {1001,Category::Ambient,"Ambient motes"},{1002,Category::Ambient,"Fireflies"},{2001,Category::Environment,"Dust"},{2002,Category::Environment,"Leaves"},
 {3001,Category::Weather,"Rain"},{3002,Category::Weather,"Snow"},{3003,Category::Weather,"Ash"},{4001,Category::Fire,"Torch flame"},{4002,Category::Fire,"Embers"},
 {5001,Category::Smoke,"Smoke"},{5002,Category::Smoke,"Mist"},{6001,Category::Magic,"Magic sparks"},{6002,Category::Magic,"Glint"},{7001,Category::Combat,"Hit sparks"},{7002,Category::Combat,"Blood spray"},{8001,Category::Water,"Water spray"},{9000,Category::Custom,"Custom effect ID"}
};
std::filesystem::path path(){wchar_t d[32768]{};auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",d,32768);if(!n||n>=32768)throw std::runtime_error("LOCALAPPDATA unavailable");return std::filesystem::path(d)/L"EldenRingTheaterMode"/L"particles.ertparticles";}
bool valid(const Emitter&e){return e.id&&e.preset>=0&&cinematic::valid(e.transform)&&std::isfinite(e.duration_seconds)&&e.duration_seconds>=0&&e.duration_seconds<=3600&&std::isfinite(e.repeat_seconds)&&e.repeat_seconds>=0&&e.repeat_seconds<=3600&&std::isfinite(e.scale)&&e.scale>0&&e.scale<=100&&std::isfinite(e.intensity)&&e.intensity>=0&&e.intensity<=100;}
}}
namespace particle_editor {
const Preset* presets(std::size_t& n){n=std::size(kPresets);return kPresets;}
View view(){std::lock_guard l(mutex);return state;}
bool snapshot(View&o){std::unique_lock l(mutex,std::try_to_lock);if(!l.owns_lock())return false;o=state;return true;}
void create(int p,const cinematic::State&c){if(!cinematic::valid(c))return;std::lock_guard l(mutex);Emitter e;e.id=next_id++;e.preset=p;e.transform=c;e.name="Emitter "+std::to_string(e.id);state.selected=e.id;state.emitters.push_back(std::move(e));state.status="Emitter created; native spawn is not validated";}
void select(std::uint64_t id){std::lock_guard l(mutex);if(std::any_of(state.emitters.begin(),state.emitters.end(),[&](auto&e){return e.id==id;}))state.selected=id;}
void edit(Emitter e){if(!valid(e))return;e.transform.orientation=*cinematic::normalized(e.transform.orientation);std::lock_guard l(mutex);for(auto&v:state.emitters)if(v.id==e.id){v=std::move(e);state.status="Emitter updated";return;}}
void remove(std::uint64_t id){std::lock_guard l(mutex);std::erase_if(state.emitters,[&](auto&e){return e.id==id;});if(state.selected==id)state.selected=state.emitters.empty()?0:state.emitters.back().id;state.status="Emitter removed";}
void clear(){std::lock_guard l(mutex);state.emitters.clear();state.selected=0;state.status="All emitters removed";}
void save(){std::lock_guard l(mutex);try{auto p=path();std::filesystem::create_directories(p.parent_path());auto t=p;t+=L".tmp";std::ofstream f(t,std::ios::binary|std::ios::trunc);f<<"ERTPARTICLES 1\n"<<std::setprecision(17);for(auto&e:state.emitters){f<<e.id<<' '<<e.preset<<' '<<std::quoted(e.name)<<' '<<e.enabled<<' '<<e.loop<<' '<<e.duration_seconds<<' '<<e.repeat_seconds<<' '<<e.scale<<' '<<e.intensity;for(auto v:e.transform.position)f<<' '<<v;for(auto v:e.transform.orientation)f<<' '<<v;f<<' '<<e.transform.fov_degrees<<' '<<e.transform.roll_degrees<<'\n';}f.flush();if(!f)throw std::runtime_error("write failed");f.close();if(!MoveFileExW(t.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("file replacement failed");state.status="Particle setup saved";}catch(const std::exception&e){state.status=std::string("Save failed: ")+e.what();}}
void load(){std::lock_guard l(mutex);try{std::ifstream f(path(),std::ios::binary);std::string m;unsigned v=0;if(!(f>>m>>v)||m!="ERTPARTICLES"||v!=1)throw std::runtime_error("unsupported or missing particle file");std::vector<Emitter> q;std::uint64_t next=1;for(;;){f>>std::ws;if(f.eof())break;Emitter e;if(!(f>>e.id>>e.preset>>std::quoted(e.name)>>e.enabled>>e.loop>>e.duration_seconds>>e.repeat_seconds>>e.scale>>e.intensity))throw std::runtime_error("invalid emitter entry");for(auto&x:e.transform.position)f>>x;for(auto&x:e.transform.orientation)f>>x;f>>e.transform.fov_degrees>>e.transform.roll_degrees;if(!f||!valid(e)||std::any_of(q.begin(),q.end(),[&](auto&o){return o.id==e.id;}))throw std::runtime_error("invalid values or duplicate emitter ID");e.transform.orientation=*cinematic::normalized(e.transform.orientation);next=std::max(next,e.id+1);q.push_back(std::move(e));}state.emitters=std::move(q);state.selected=state.emitters.empty()?0:state.emitters.front().id;next_id=next;state.status="Particle setup loaded; native spawn is not validated";}catch(const std::exception&e){state.status=std::string("Load failed: ")+e.what();}}
}

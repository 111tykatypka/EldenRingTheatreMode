#pragma once
#include "CameraTrack.h"
#include <cstdint>
#include <string>
#include <vector>
namespace light_editor {
enum class Type:unsigned {Point,Spot};
struct Light {
 std::uint64_t id=0;Type type=Type::Point;std::string name;
 cinematic::State transform;
 float rgb[3]{1,1,1},radius=5,intensity=1,cone_degrees=45,softness=.25f;
 // Native illumination applies core fields; shadow/scattering remain drafts.
 bool enabled=true,shadows=false;
 unsigned shadow_level=2;
 float shadow_strength=1,source_radius=.1f,scattering=1;
 int shadow_bias=0;
 float specular_rgb[3]{1,1,1};
};
// Keyframe animation of a light's position and rotation against replay time (like the dolly camera). Turned on per light with the clock
// toggle; while on, the light follows its keys whenever the timeline is loaded.
struct LightKey {std::uint64_t time_ns=0;cinematic::State transform;};
bool animated(std::uint64_t id);
void set_animated(std::uint64_t id,bool on);
std::vector<LightKey> light_keys(std::uint64_t id);
bool add_light_key(std::uint64_t id,std::uint64_t time_ns); // stores the light's current transform at that time (replaces a key at the same time)
void remove_light_key(std::uint64_t id,std::uint64_t time_ns);
void clear_light_keys(std::uint64_t id);
void evaluate_animation(std::uint64_t time_ns,std::uint64_t skip_id); // skip_id: the light being dragged by hand
struct View {std::vector<Light> lights;std::uint64_t selected=0;std::string status;};
View view();
bool snapshot(View& out); // Nonblocking game-task copy; no native pointers.
void create(Type type,const cinematic::State& camera);
bool duplicate(std::uint64_t id);
void select(std::uint64_t id);
void edit(Light light);
void remove(std::uint64_t id);
void save();void load();
struct TimeView {bool available=false,requested=false;float hour=12,target=12;int state=0;};
TimeView time_view();
void time(float hour);void restore_time();
}
extern "C" void tm_lighting_time_observe(float hour,int state);
extern "C" void tm_lighting_time_request(int minutes);
extern "C" void tm_lighting_time_connected(int connected);

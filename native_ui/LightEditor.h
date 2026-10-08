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
 // Draft properties only: native application is a separate capability.
 bool enabled=true,shadows=false;
 unsigned shadow_level=2;
 float shadow_strength=1,source_radius=.1f,scattering=1;
 int shadow_bias=0;
 float specular_rgb[3]{1,1,1};
};
struct View {std::vector<Light> lights;std::uint64_t selected=0;std::string status;};
View view();
void create(Type type,const cinematic::State& camera);
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

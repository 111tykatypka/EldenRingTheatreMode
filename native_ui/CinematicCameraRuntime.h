#pragma once
#include "CinematicCamera.h"
#include "CameraTelemetry.h"
#include "CameraCutTrack.h"
#include <string>
namespace camera_runtime {
struct View {
 bool hook_ready=false,observed=false,enabled=false,writing=false;
 unsigned mode=0;std::uint64_t timestamp_ns=0;
 cinematic::State pose;std::vector<cinematic::Key> keys;
 std::string status;
 double movement_speed=3,mouse_sensitivity=.0025,smoothing_seconds=0;
 int bone_index=-1;bool bone_available=false;cinematic::Vec bone_offset{0,0,-1};
 std::uint64_t project_generation=0;
 double shake_position=0,shake_rotation=0,shake_frequency=1;
 bool cuts_enabled=false;std::vector<cinematic::CameraCut> cuts;
};
View view();
void mode(unsigned value);
void enable(bool value);
void stop();
void probe(); // +0.25 X, automatically releases after two real seconds
void add_key();
void clear_keys();
void edit_key(cinematic::Key key);
void delete_key(std::uint64_t id);
void save_path();
void load_path();
void movement(double speed,double sensitivity,double smoothing_seconds);
void bone(int index,cinematic::Vec offset);
void shake(double position_units,double rotation_degrees,double frequency_hz);
std::optional<cinematic::State> bone_world(const float* root_matrix,const float* model_qs);
void timeline(std::uint64_t time,std::uint64_t duration,std::uint64_t anchor,bool playing,double speed,bool linked,const char* replay_path=nullptr);
bool owns_input();
void overlay_visible(bool visible);
void window(void* hwnd);
void mouse_delta(long x,long y);
void fov(double degrees);
void cuts(bool enabled,std::vector<cinematic::CameraCut> values);
std::optional<cinematic::State> decode_candidate(const theater_camera::Slot& slot);
void encode_pose(const cinematic::State& pose,float* matrix);
}

#pragma once
#include "CinematicCamera.h"
#include "CameraTelemetry.h"
#include <string>
namespace camera_runtime {
struct View {
 bool hook_ready=false,observed=false,enabled=false,writing=false;
 unsigned mode=0;std::uint64_t timestamp_ns=0;
 cinematic::State pose;std::vector<cinematic::Key> keys;
 std::string status;
};
View view();
void mode(unsigned value);
void enable(bool value);
void stop();
void probe(); // +0.25 X, automatically releases after two real seconds
void add_key();
void clear_keys();
void timeline(std::uint64_t time,std::uint64_t duration,std::uint64_t anchor,bool playing,double speed,bool linked,const char* replay_path=nullptr);
bool owns_input();
void overlay_visible(bool visible);
void window(void* hwnd);
void mouse_delta(long x,long y);
void fov(double degrees);
std::optional<cinematic::State> decode_candidate(const theater_camera::Slot& slot);
void encode_pose(const cinematic::State& pose,float* matrix);
}

#pragma once
#include <cmath>
#include <cstdint>
#include <atomic>
namespace theater_camera {
inline std::atomic_bool probe_enabled{false};
// Read-only candidates, not a claim that any slot is the active rendered view.
struct Slot { float matrix[16]{}, fov{}, aspect{}, near_plane{}, far_plane{}; std::uint32_t valid{}; };
struct Telemetry { std::uint64_t timestamp_ns{}; std::uint32_t mask{}, available{}; Slot slots[4]{}; };
static_assert(sizeof(Slot)==84 && sizeof(Telemetry)==352);
inline bool valid(const Slot& s) {
 for(float x:s.matrix) if(!std::isfinite(x)) return false;
 // FOV units are not yet runtime verified; do not label this as degrees.
 if(!std::isfinite(s.fov)||s.fov<=0||s.fov>180||!std::isfinite(s.aspect)||s.aspect<=0||s.aspect>20||
    !std::isfinite(s.near_plane)||!std::isfinite(s.far_plane)||s.near_plane<=0||s.far_plane<=s.near_plane) return false;
 for(int row=0;row<3;++row){float n=0;for(int col=0;col<3;++col)n+=s.matrix[row*4+col]*s.matrix[row*4+col];if(n<0.8f||n>1.2f)return false;}
 return true;
}
}

#pragma once
#include "CameraMath.h"
#include "CameraTrack.h"
namespace cinematic {
// Integer/rational output scheduling. No wall clock and no realtime timescale.
// Range is [start,end); caller must wait for game application + GPU fence before encoding.
inline std::optional<std::uint64_t> frame_time(std::uint64_t start,std::uint64_t end,std::uint64_t n,std::uint32_t fps_num,std::uint32_t fps_den=1){
 if(!fps_num||!fps_den||end<=start)return {};
 const auto duration=end-start,max_seconds=duration/1000000000ULL;
 const auto q=n/fps_num,r=n%fps_num;
 if(q>max_seconds/fps_den)return {};
 const auto product=r*std::uint64_t(fps_den); // two uint32 operands fit uint64
 const auto seconds=q*fps_den+product/fps_num;
 if(seconds>max_seconds)return {};
 const auto fraction=(product%fps_num)*1000000000ULL/fps_num;
 const auto whole=seconds*1000000000ULL;
 if(whole>=duration||fraction>=duration-whole)return {};
 return start+whole+fraction;
}
}

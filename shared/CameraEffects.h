#pragma once
#include "CinematicCamera.h"
namespace cinematic {
// Stateless five-tap temporal filter. Shrink the window at every authored key;
// exact key poses, cuts and endpoints remain exact. No seek/history dependence.
inline std::optional<State> dolly_evaluate(const Track& track,std::uint64_t time,double seconds){
 auto base=track.evaluate(time);if(!base||!std::isfinite(seconds)||seconds<=0)return base;
 const auto& keys=track.keys();auto next=std::upper_bound(keys.begin(),keys.end(),time,[](auto t,const Key& k){return t<k.time_ns;});
 if(next==keys.begin()||next==keys.end())return base;auto prev=next-1;
 if(prev->time_ns==time||prev->outgoing==Interpolation::Step)return base;
 const double radius=std::min({seconds*1e9,double(time-prev->time_ns),double(next->time_ns-time)});
 const double weights[]={1,4,6,4,1};State result{};result.fov_degrees=0;double total=0;
 for(int i=0;i<5;++i){auto sample=track.evaluate(std::uint64_t(double(time)+(i-2)*radius*.5));if(!sample)return base;
  result.position=add(result.position,mul(sample->position,weights[i]));result.fov_degrees+=sample->fov_degrees*weights[i];
  result.orientation=total?slerp(result.orientation,sample->orientation,weights[i]/(total+weights[i])):sample->orientation;total+=weights[i];}
 result.position=mul(result.position,1/total);result.fov_degrees/=total;return result;
}
// Gaussian time-domain low-pass of the original sinusoidal shake. Analytic,
// deterministic, allocation-free: paused/rewound frames give identical output.
inline double shake_wave(double replay_seconds,int axis,double frequency,double speed,double smoothing){
 const double omega=frequency*speed*6.283185307179586*(1+axis*.173);
 return std::sin(replay_seconds*omega+axis*2.1)*std::exp(-.5*omega*omega*smoothing*smoothing);
}
}

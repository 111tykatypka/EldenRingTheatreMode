#pragma once
#include "CameraMath.h"
namespace cinematic {
// Live rigs only. The closed-form solution for a fixed target composes over
// different frame rates; the replay Track deliberately owns no such state.
struct CriticalSpring {
 Vec position{},velocity{};
 bool advance(Vec target,double dt,double angular_frequency){
  if(!finite(target)||!finite(position)||!finite(velocity)||!std::isfinite(dt)||dt<0||!std::isfinite(angular_frequency)||angular_frequency<=0)return false;
  auto offset=sub(position,target),impulse=add(velocity,mul(offset,angular_frequency));
  double decay=std::exp(-angular_frequency*dt);
  auto p=add(target,mul(add(offset,mul(impulse,dt)),decay));
  auto v=mul(sub(velocity,mul(impulse,angular_frequency*dt)),decay);
  if(!finite(p)||!finite(v))return false;position=p;velocity=v;return true;
 }
};
}

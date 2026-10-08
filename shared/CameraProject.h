#pragma once
#include "CinematicCamera.h"
#include <iomanip>
#include <sstream>
#include <string>
#include <locale>
namespace cinematic {
inline void write_ease(std::ostream& out,const EaseCurve& curve){out<<' '<<unsigned(curve.mode)<<' '<<curve.x1<<' '<<curve.y1<<' '<<curve.x2<<' '<<curve.y2;}
inline bool read_ease(std::istream& in,EaseCurve& curve){unsigned mode=0;if(!(in>>mode>>curve.x1>>curve.y1>>curve.x2>>curve.y2)||mode>unsigned(Easing::CubicBezier))return false;curve.mode=Easing(mode);return curve.valid();}
// v2 stores independent channel membership, timing, rotation and target data.
// v1 is migrated on load. No pointers or engine addresses are serialized.
inline std::string save_project(const std::string& replay,const std::vector<Key>&keys,const TrackSettings& settings={}){
 std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
 out<<"ERTCAM 2\n"<<std::quoted(replay)<<'\n';
 out<<unsigned(settings.timing)<<' '<<settings.world_units_per_second<<' '<<settings.arc_samples<<' '<<unsigned(settings.rotation)<<' '<<unsigned(settings.target)<<' '<<settings.target_actor_id;
 for(auto v:settings.target_position)out<<' '<<v;for(auto v:settings.target_offset)out<<' '<<v;write_ease(out,settings.time_remap);out<<'\n'<<keys.size()<<'\n';
 for(const auto&k:keys){out<<k.id<<' '<<k.time_ns<<' '<<unsigned(k.outgoing)<<' '<<k.constant_speed;
  for(auto v:k.state.position)out<<' '<<v;for(auto v:k.state.orientation)out<<' '<<v;
  out<<' '<<k.state.fov_degrees;for(auto v:k.tangent_in)out<<' '<<v;for(auto v:k.tangent_out)out<<' '<<v;
  out<<' '<<k.ease_in<<' '<<k.ease_out;
  out<<' '<<k.channels<<' '<<unsigned(k.tangent_mode)<<' '<<unsigned(k.rotation_interpolation)<<' '<<unsigned(k.scalar_interpolation)<<' '<<k.state.roll_degrees<<' '<<k.state.focus_distance;
  for(auto v:k.state.look_at_target)out<<' '<<v;
  write_ease(out,k.position_ease);write_ease(out,k.rotation_ease);write_ease(out,k.scalar_ease);out<<'\n';}
 return out.str();
}
inline bool load_project(const std::string&text,const std::string&expected,std::vector<Key>&result,TrackSettings* result_settings=nullptr){
 std::istringstream in(text);in.imbue(std::locale::classic());std::string magic,replay;unsigned version=0;std::size_t count=0;TrackSettings settings;
 if(!(in>>magic>>version)||magic!="ERTCAM"||(version!=1&&version!=2)||!(in>>std::quoted(replay))||replay!=expected)return false;
 if(version==2){unsigned timing,rotation,target;
  if(!(in>>timing>>settings.world_units_per_second>>settings.arc_samples>>rotation>>target>>settings.target_actor_id)||timing>2||rotation>3||target>3)return false;
  settings.timing=TimingMode(timing);settings.rotation=RotationMode(rotation);settings.target=TargetType(target);
  for(auto&v:settings.target_position)if(!(in>>v))return false;for(auto&v:settings.target_offset)if(!(in>>v))return false;
  if(!read_ease(in,settings.time_remap)||!settings.valid())return false;
 }
 if(!(in>>count)||count>text.size()/8)return false;
 std::vector<Key> keys;
 for(std::size_t i=0;i<count;++i){Key k;unsigned mode=0,constant=0;if(!(in>>k.id>>k.time_ns>>mode>>constant)||mode>unsigned(Interpolation::Step)||constant>1)return false;
  k.outgoing=Interpolation(mode);k.constant_speed=constant!=0;
  for(auto&v:k.state.position)if(!(in>>v))return false;for(auto&v:k.state.orientation)if(!(in>>v))return false;
  if(!(in>>k.state.fov_degrees))return false;for(auto&v:k.tangent_in)if(!(in>>v))return false;for(auto&v:k.tangent_out)if(!(in>>v))return false;
  if(!(in>>k.ease_in>>k.ease_out))return false;
  if(version==2){unsigned tangent,rotation,scalar;
   if(!(in>>k.channels>>tangent>>rotation>>scalar>>k.state.roll_degrees>>k.state.focus_distance)||tangent>2||rotation>2||scalar>2)return false;
   k.tangent_mode=TangentMode(tangent);k.rotation_interpolation=RotationInterpolation(rotation);k.scalar_interpolation=ScalarInterpolation(scalar);
   for(auto&v:k.state.look_at_target)if(!(in>>v))return false;
   if(!read_ease(in,k.position_ease)||!read_ease(in,k.rotation_ease)||!read_ease(in,k.scalar_ease))return false;
  }else{
   // Old tracks used one eased time for every channel. Preserve their look
   // while newly authored tracks use independent smooth rotation/scalars.
   k.rotation_interpolation=RotationInterpolation::Slerp;k.scalar_interpolation=ScalarInterpolation::Linear;
   if(k.outgoing==Interpolation::Smooth)k.rotation_ease.mode=k.scalar_ease.mode=Easing::Smoothstep;
   if(k.outgoing==Interpolation::Curve){k.rotation_ease.mode=k.scalar_ease.mode=Easing::CubicBezier;k.rotation_ease.y1=k.scalar_ease.y1=k.ease_in;k.rotation_ease.y2=k.scalar_ease.y2=1-k.ease_out;}
   if(k.outgoing==Interpolation::Step){k.rotation_interpolation=RotationInterpolation::Step;k.scalar_interpolation=ScalarInterpolation::Step;}
  }
  keys.push_back(k);
 }
 in>>std::ws;if(!in.eof())return false;Track validated;if(!validated.replace(std::move(keys),settings))return false;
 result=validated.keys();if(result_settings)*result_settings=settings;return true;
}
}

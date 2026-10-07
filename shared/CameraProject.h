#pragma once
#include "CinematicCamera.h"
#include <iomanip>
#include <sstream>
#include <string>
#include <locale>
namespace cinematic {
// Independent text format, UTF-8 replay identity and IEEE double round trips.
// Loading never arms a camera or alters gameplay state.
inline std::string save_project(const std::string& replay,const std::vector<Key>&keys){
 std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
 out<<"ERTCAM 1\n"<<std::quoted(replay)<<'\n'<<keys.size()<<'\n';
 for(const auto&k:keys){out<<k.id<<' '<<k.time_ns<<' '<<unsigned(k.outgoing)<<' '<<k.constant_speed;
  for(auto v:k.state.position)out<<' '<<v;for(auto v:k.state.orientation)out<<' '<<v;
  out<<' '<<k.state.fov_degrees;for(auto v:k.tangent_in)out<<' '<<v;for(auto v:k.tangent_out)out<<' '<<v;
  out<<' '<<k.ease_in<<' '<<k.ease_out<<'\n';}
 return out.str();
}
inline bool load_project(const std::string&text,const std::string&expected,std::vector<Key>&result){
 std::istringstream in(text);in.imbue(std::locale::classic());std::string magic,replay;unsigned version=0;std::size_t count=0;
 if(!(in>>magic>>version)||magic!="ERTCAM"||version!=1||!(in>>std::quoted(replay)>>count)||replay!=expected||count>text.size()/8)return false;
 std::vector<Key> keys;
 for(std::size_t i=0;i<count;++i){Key k;unsigned mode=0,constant=0;if(!(in>>k.id>>k.time_ns>>mode>>constant)||mode>unsigned(Interpolation::Step)||constant>1)return false;
  k.outgoing=Interpolation(mode);k.constant_speed=constant!=0;
  for(auto&v:k.state.position)if(!(in>>v))return false;for(auto&v:k.state.orientation)if(!(in>>v))return false;
  if(!(in>>k.state.fov_degrees))return false;for(auto&v:k.tangent_in)if(!(in>>v))return false;for(auto&v:k.tangent_out)if(!(in>>v))return false;
  if(!(in>>k.ease_in>>k.ease_out))return false;keys.push_back(k);}
 in>>std::ws;if(!in.eof())return false;Track validated;if(!validated.replace(std::move(keys)))return false;result=validated.keys();return true;
}
}

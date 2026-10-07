#pragma once
#include "CinematicCamera.h"
#include <sstream>
#include <string>
#include <locale>
namespace cinematic {
// CPU reference for independent 3D .cube grading. Not a renderer installation.
struct CubeLut {
 unsigned size=0;Vec domain_min{0,0,0},domain_max{1,1,1};std::vector<Vec> data;
 static std::optional<CubeLut> parse(const std::string&text){
  CubeLut lut;bool sized=false,minSeen=false,maxSeen=false;std::istringstream file(text);file.imbue(std::locale::classic());std::string line;
  while(std::getline(file,line)){auto comment=line.find('#');if(comment!=std::string::npos)line.resize(comment);std::istringstream row(line);row.imbue(std::locale::classic());std::string token;if(!(row>>token))continue;
   if(token=="TITLE")continue;
   if(token=="LUT_3D_SIZE"){if(sized||!(row>>lut.size)||lut.size<2||lut.size>256)return {};sized=true;}
   else if(token=="DOMAIN_MIN"||token=="DOMAIN_MAX"){bool&seen=token=="DOMAIN_MIN"?minSeen:maxSeen;if(seen||!lut.data.empty())return {};seen=true;auto&v=token=="DOMAIN_MIN"?lut.domain_min:lut.domain_max;for(auto&x:v)if(!(row>>x))return {};if(!finite(v))return {};}
   else{if(!sized||token.starts_with("LUT_"))return {};Vec v{};std::istringstream number(token);number.imbue(std::locale::classic());if(!(number>>v[0]))return {};number>>std::ws;if(!number.eof()||!(row>>v[1]>>v[2])||!finite(v))return {};lut.data.push_back(v);if(lut.data.size()>std::size_t(lut.size)*lut.size*lut.size)return {};}
   row>>std::ws;if(!row.eof())return {};
  }
  if(!sized||lut.data.size()!=std::size_t(lut.size)*lut.size*lut.size)return {};
  for(int i=0;i<3;++i)if(lut.domain_max[i]<=lut.domain_min[i])return {};return lut;
 }
 std::optional<Vec> sample(Vec rgb)const{
  if(!finite(rgb)||size<2||data.size()!=std::size_t(size)*size*size)return {};
  unsigned lo[3],hi[3];Vec frac{};for(int i=0;i<3;++i){double u=std::clamp((rgb[i]-domain_min[i])/(domain_max[i]-domain_min[i]),0.,1.)*(size-1);lo[i]=static_cast<unsigned>(u);hi[i]=std::min(lo[i]+1,size-1);frac[i]=u-lo[i];}
  Vec out{};for(unsigned corner=0;corner<8;++corner){double weight=1;unsigned index[3];for(int i=0;i<3;++i){bool high=(corner&(1<<i))!=0;index[i]=high?hi[i]:lo[i];weight*=high?frac[i]:1-frac[i];}
   // .cube red changes fastest, blue slowest.
   out=add(out,mul(data[index[0]+size*(index[1]+size*index[2])],weight));}return out;
 }
};
}

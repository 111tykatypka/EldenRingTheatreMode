#pragma once
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
namespace theater_timescale {
// Replay speed range (Phase 1.3 spec: 0.01x to 4x, logarithmic). The replay has its own clock;
// this never changes the game's speed.
inline constexpr double minimum=0.01, maximum=4.0, normal=1.0;
// Quick-set marks drawn on the slider; clicking a mark (or dragging close to it) snaps to it.
inline constexpr double marks[]={0.1,0.25,0.5,1.0,2.0};
inline bool valid(double value){return std::isfinite(value)&&value>=minimum&&value<=maximum;}
inline double clamp(double value){return std::clamp(value,minimum,maximum);}
inline double normalized(double value){return std::log(clamp(value)/minimum)/std::log(maximum/minimum);}
inline double from_normalized(double value){return minimum*std::pow(maximum/minimum,std::clamp(value,0.0,1.0));}
inline double adjust(double value,double normalized_delta){return from_normalized(normalized(value)+normalized_delta);}
inline std::uint64_t encode(double value){return std::bit_cast<std::uint64_t>(value);}
inline double decode(std::uint64_t value){return std::bit_cast<double>(value);}
inline bool parse(std::string_view input,double& value){
    std::string text(input);auto last=text.find_last_not_of(" \t\r\n");
    if(last==std::string::npos)return false;text.resize(last+1);
    if(text.back()=='x'||text.back()=='X')text.pop_back();
    char* end=nullptr;const double parsed=std::strtod(text.c_str(),&end);
    if(end==text.c_str()||!std::isfinite(parsed)||parsed<=0)return false;
    while(*end==' '||*end=='\t'||*end=='\r'||*end=='\n')++end;
    if(*end)return false;value=clamp(parsed);return true;
}
inline void format(double value,char* text,std::size_t size){
    const int digits=value>=0.1?3:value>=0.01?4:6;
    std::snprintf(text,size,"%.*fx",digits,value);
}
}

#pragma once
#include <algorithm>
#include <cmath>
namespace editor {
struct TimeView {
 double begin{}, span{60};
 double time(double pixel,double width,double duration)const{return std::clamp(begin+pixel/std::max(1.0,width)*span,0.0,duration);}
 double pixel(double time,double width)const{return (time-begin)/std::max(.001,span)*width;}
 void fit(double duration){begin=0;span=std::max(.001,duration);}
 void zoom(double factor,double anchor,double duration){const auto old=span;span=std::clamp(span*factor,.001,std::max(.001,duration));begin=std::clamp(begin+(old-span)*anchor,0.0,std::max(0.0,duration-span));}
 void pan(double seconds,double duration){begin=std::clamp(begin+seconds,0.0,std::max(0.0,duration-span));}
};
inline double start_distance(const float* a,const float* b){double d=0;for(int i=0;i<3;++i)d+=std::pow(double(a[i])-b[i],2);return std::sqrt(d);}
}

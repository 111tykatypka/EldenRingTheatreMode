#pragma once
#include "CameraMath.h"
namespace cinematic {
enum class Easing:unsigned { Linear,EaseIn,EaseOut,EaseInOut,Smoothstep,Smootherstep,CubicBezier };
struct EaseCurve {
 Easing mode=Easing::Linear;
 double x1=.3333333333333333,y1=.3333333333333333,x2=.6666666666666666,y2=.6666666666666666;
 bool valid()const {return unsigned(mode)<=unsigned(Easing::CubicBezier)&&std::isfinite(x1)&&std::isfinite(y1)&&std::isfinite(x2)&&std::isfinite(y2)&&x1>=0&&x2<=1&&x1<=x2&&y1>=0&&y1<=1&&y2>=0&&y2<=1;}
 double evaluate(double t)const {
  t=std::clamp(t,0.,1.);if(t==0||t==1)return t;
  switch(mode){
  case Easing::EaseIn:return t*t;
  case Easing::EaseOut:return t*(2-t);
  case Easing::EaseInOut:return t<.5?2*t*t:1-2*(1-t)*(1-t);
  case Easing::Smoothstep:return t*t*(3-2*t);
  case Easing::Smootherstep:return t*t*t*(t*(6*t-15)+10);
  case Easing::CubicBezier:{
   // The editor supplies x=time, y=distance. Invert monotonic x(u), then
   // evaluate y(u). Substituting time directly for u is not a timing curve.
   auto cubic=[](double u,double a,double b){double v=1-u;return 3*v*v*u*a+3*v*u*u*b+u*u*u;};
   double lo=0,hi=1;for(int n=0;n<48;++n){double u=(lo+hi)*.5;if(cubic(u,x1,x2)<t)lo=u;else hi=u;}
   return cubic((lo+hi)*.5,y1,y2);
  }
  default:return t;
  }
 }
};
struct CubicSegment {
 Vec a{},b{},c{},d{};
 Vec evaluate(double u)const {return add(a,mul(add(b,mul(add(c,mul(d,u)),u)),u));}
 Vec derivative(double u)const {return add(b,mul(add(mul(c,2),mul(d,3*u)),u));}
 static CubicSegment hermite(Vec p,Vec q,Vec m,Vec n){auto delta=sub(q,p);return {p,m,sub(mul(delta,3),add(mul(m,2),n)),add(mul(delta,-2),add(m,n))};}
 static CubicSegment linear(Vec p,Vec q){return {p,sub(q,p),{}, {}};}
 static CubicSegment bezier(Vec p,Vec h1,Vec h2,Vec q){return hermite(p,q,mul(sub(h1,p),3),mul(sub(q,h2),3));}
 static CubicSegment catmull(Vec p0,Vec p1,Vec p2,Vec p3){
  if(length(sub(p2,p1))<1e-9)return {p1,{},{},{}};
  if(length(sub(p1,p0))<1e-9)p0=sub(mul(p1,2),p2);
  if(length(sub(p3,p2))<1e-9)p3=sub(mul(p2,2),p1);
  // Knot intervals = chord length^alpha, alpha=.5. Convert the
  // nonuniform Catmull-Rom derivatives to local Hermite u derivatives.
  double h0=std::sqrt(length(sub(p1,p0))),h1=std::sqrt(length(sub(p2,p1))),h2=std::sqrt(length(sub(p3,p2)));
  Vec m=mul(add(sub(mul(sub(p1,p0),1/h0),mul(sub(p2,p0),1/(h0+h1))),mul(sub(p2,p1),1/h1)),h1);
  Vec n=mul(add(sub(mul(sub(p2,p1),1/h1),mul(sub(p3,p1),1/(h1+h2))),mul(sub(p3,p2),1/h2)),h1);
  return hermite(p1,p2,m,n);
 }
};
inline Vec spline(Vec p0,Vec p1,Vec p2,Vec p3,double u){return CubicSegment::catmull(p0,p1,p2,p3).evaluate(std::clamp(u,0.,1.));}
class ArcLengthTable {
 std::vector<double> distances_;
public:
 void build(const CubicSegment& curve,unsigned samples){
  samples=std::clamp(samples,16u,8192u);
  distances_.assign(samples+1,0);Vec prev=curve.evaluate(0);
  for(unsigned i=1;i<=samples;++i){auto next=curve.evaluate(double(i)/samples);distances_[i]=distances_[i-1]+cinematic::length(sub(next,prev));prev=next;}
 }
 double length()const{return distances_.empty()?0:distances_.back();}
 double parameter_to_distance(double u)const {
  if(distances_.size()<2)return 0;u=std::clamp(u,0.,1.);double v=u*(distances_.size()-1);auto i=std::min(std::size_t(v),distances_.size()-2);
  return distances_[i]+(distances_[i+1]-distances_[i])*(v-i);
 }
 double distance_to_parameter(double distance)const {
  if(length()<1e-12)return 0;distance=std::clamp(distance,0.,length());
  if(distance==length())return 1;
  auto hi=std::upper_bound(distances_.begin(),distances_.end(),distance);auto i=std::clamp<std::size_t>(hi-distances_.begin(),1,distances_.size()-1);
  double span=distances_[i]-distances_[i-1];return (i-1+(span>1e-12?(distance-distances_[i-1])/span:0))/(distances_.size()-1);
 }
};
}

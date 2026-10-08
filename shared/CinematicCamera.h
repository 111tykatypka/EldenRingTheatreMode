#pragma once
// Independent, pure evaluator. Input time always comes from the authoritative sequencer.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>
namespace cinematic {
using Vec=std::array<double,3>;
using Quat=std::array<double,4>; // XYZW
inline Vec add(Vec a,Vec b){for(int i=0;i<3;++i)a[i]+=b[i];return a;}
inline Vec sub(Vec a,Vec b){for(int i=0;i<3;++i)a[i]-=b[i];return a;}
inline Vec mul(Vec a,double s){for(auto&v:a)v*=s;return a;}
inline Vec mix(Vec a,Vec b,double u){return add(mul(a,1-u),mul(b,u));}
inline double length(Vec v){return std::hypot(v[0],v[1],v[2]);}
inline bool finite(Vec v){return std::all_of(v.begin(),v.end(),[](double x){return std::isfinite(x);});}
inline std::optional<Quat> normalized(Quat q){double n=0;for(auto v:q){if(!std::isfinite(v))return {};n+=v*v;}if(!std::isfinite(n)||n<1e-12)return {};for(auto&v:q)v/=std::sqrt(n);return q;}
// Mouse yaw uses world up. Pitch and deliberate roll use camera-local axes.
// Post-multiplying yaw after pitch banks the horizon during ordinary mouse look.
inline Quat compose(Quat a,Quat b){return {a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};}
inline std::optional<Quat> mouse_look(Quat orientation,double yaw,double pitch,double roll){
 if(!std::isfinite(yaw)||!std::isfinite(pitch)||!std::isfinite(roll))return {};
 auto unit=normalized(orientation);if(!unit)return {};
 auto axis=[](int i,double angle){Quat q{0,0,0,std::cos(angle/2)};q[i]=std::sin(angle/2);return q;};
 return normalized(compose(compose(compose(axis(1,yaw),*unit),axis(0,pitch)),axis(2,roll)));
}
inline Quat slerp(Quat a,Quat b,double t){double d=0;for(int i=0;i<4;++i)d+=a[i]*b[i];if(d<0){for(auto&v:b)v=-v;d=-d;}d=std::clamp(d,-1.,1.);
 double x=1-t,y=t;if(d<0.9995){double angle=std::acos(d),s=std::sin(angle);x=std::sin((1-t)*angle)/s;y=std::sin(t*angle)/s;}
 for(int i=0;i<4;++i)a[i]=a[i]*x+b[i]*y;return *normalized(a);}
// Exponential response in real time: identical convergence at different refresh rates.
inline double smooth_fov(double current,double target,double dt){
 if(!std::isfinite(current)||!std::isfinite(target)||!std::isfinite(dt)||dt<0)return current;
 target=std::clamp(target,1.,178.);
 double value=current+(target-current)*(-std::expm1(-dt/.12));
 return std::abs(value-target)<.001?target:value;
}
inline double wheel_fov(double target,double notches,double precision=1){
 if(!std::isfinite(target)||!std::isfinite(notches)||!std::isfinite(precision))return target;
 return std::clamp(target-notches*3*precision,1.,178.);
}
enum class Interpolation:std::uint8_t { Linear,Smooth,Bezier,Curve,Spline,Step };
struct State { Vec position{};Quat orientation{0,0,0,1};double fov_degrees=60; };
struct Key {
 std::uint64_t id=0,time_ns=0;State state;
 Interpolation outgoing=Interpolation::Spline;
 Vec tangent_in{},tangent_out{}; // offsets relative to node position
 double ease_in=0,ease_out=0; // timing cubic Bezier control ordinates, X at 1/3 and 2/3
 bool constant_speed=false;
};
inline bool valid(const State&s){return finite(s.position)&&normalized(s.orientation).has_value()&&std::isfinite(s.fov_degrees)&&s.fov_degrees>0&&s.fov_degrees<180;}
// Centripetal Catmull-Rom (alpha=0.5). Repeated control points use reflected
// neighbors; zero-length segments stay fixed. Endpoint conditions are explicit.
inline Vec spline(Vec p0,Vec p1,Vec p2,Vec p3,double u){
 if(length(sub(p2,p1))<1e-9)return p1;
 if(length(sub(p1,p0))<1e-9)p0=sub(mul(p1,2),p2);
 if(length(sub(p3,p2))<1e-9)p3=sub(mul(p2,2),p1);
 double t0=0,t1=std::sqrt(length(sub(p1,p0))),t2=t1+std::sqrt(length(sub(p2,p1))),t3=t2+std::sqrt(length(sub(p3,p2))),t=t1+u*(t2-t1);
 auto blend=[t](Vec a,Vec b,double x,double y){return mix(a,b,(t-x)/(y-x));};
 auto a1=blend(p0,p1,t0,t1),a2=blend(p1,p2,t1,t2),a3=blend(p2,p3,t2,t3);
 auto b1=blend(a1,a2,t0,t2),b2=blend(a2,a3,t1,t3);return blend(b1,b2,t1,t2);
}
class Track {
 std::vector<Key> keys_;
 struct Arc { std::array<double,129> distance{}; };
 std::vector<Arc> arcs_;
 Vec position(std::size_t i,double u)const {
  const auto&a=keys_[i];const auto&b=keys_[i+1];
  switch(a.outgoing){
  case Interpolation::Step:return a.state.position;
  case Interpolation::Bezier:{auto p0=a.state.position,p1=add(p0,a.tangent_out),p3=b.state.position,p2=add(p3,b.tangent_in);double v=1-u;return add(add(mul(p0,v*v*v),mul(p1,3*v*v*u)),add(mul(p2,3*v*u*u),mul(p3,u*u*u)));}
  case Interpolation::Spline:return spline(i?keys_[i-1].state.position:sub(mul(a.state.position,2),b.state.position),a.state.position,b.state.position,i+2<keys_.size()?keys_[i+2].state.position:sub(mul(b.state.position,2),a.state.position),u);
  default:return mix(a.state.position,b.state.position,u);
  }
 }
public:
 const std::vector<Key>&keys()const{return keys_;}
 bool replace(std::vector<Key> keys){
  std::sort(keys.begin(),keys.end(),[](const Key&a,const Key&b){return a.time_ns<b.time_ns;});
  std::vector<std::uint64_t> ids;
  for(std::size_t i=0;i<keys.size();++i){auto&k=keys[i];if(!k.id||!valid(k.state)||!finite(k.tangent_in)||!finite(k.tangent_out)||
   unsigned(k.outgoing)>unsigned(Interpolation::Step)||!std::isfinite(k.ease_in)||!std::isfinite(k.ease_out)||k.ease_in<0||k.ease_in>1||k.ease_out<0||k.ease_out>1||(i&&keys[i-1].time_ns==k.time_ns))return false;
   k.state.orientation=*normalized(k.state.orientation);ids.push_back(k.id);}
  std::sort(ids.begin(),ids.end());if(std::adjacent_find(ids.begin(),ids.end())!=ids.end())return false;
  keys_=std::move(keys);arcs_.assign(keys_.empty()?0:keys_.size()-1,{});
  // Rebuild only after an edit, never per playback frame. Bounded subdivisions
  // give approximate constant speed; very tight curves need future adaptive sampling.
  for(std::size_t i=0;i<arcs_.size();++i){Vec prev=position(i,0);for(int j=1;j<=128;++j){Vec next=position(i,double(j)/128);arcs_[i].distance[j]=arcs_[i].distance[j-1]+length(sub(next,prev));prev=next;}}
  return true;
 }
 std::optional<State> evaluate(std::uint64_t time_ns)const {
  if(keys_.empty())return {};if(time_ns<=keys_.front().time_ns)return keys_.front().state;if(time_ns>=keys_.back().time_ns)return keys_.back().state;
  auto b=std::upper_bound(keys_.begin(),keys_.end(),time_ns,[](auto t,const Key&k){return t<k.time_ns;});auto i=std::size_t(b-keys_.begin()-1);const auto&a=keys_[i];
  if(a.outgoing==Interpolation::Step)return a.state;
  double u=double(time_ns-a.time_ns)/double(b->time_ns-a.time_ns);
  if(a.outgoing==Interpolation::Smooth)u=u*u*(3-2*u);
  if(a.outgoing==Interpolation::Curve){double v=1-u;u=3*v*v*u*a.ease_in+3*v*u*u*(1-a.ease_out)+u*u*u;}
  double position_u=u;
  if(a.constant_speed){const auto&d=arcs_[i].distance;double total=d.back();if(total>1e-9){double target=u*total;auto hi=std::lower_bound(d.begin(),d.end(),target);auto j=std::clamp<std::size_t>(hi-d.begin(),1,128);double span=d[j]-d[j-1];position_u=(double(j-1)+(span>1e-12?(target-d[j-1])/span:0))/128;}}
  return State{position(i,position_u),slerp(a.state.orientation,b->state.orientation,u),a.state.fov_degrees+(b->state.fov_degrees-a.state.fov_degrees)*u};
 }
};
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

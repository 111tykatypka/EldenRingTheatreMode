#pragma once
#include "CameraCurves.h"
#include "CameraQuaternion.h"
#include <functional>
namespace cinematic {
enum class Interpolation:std::uint8_t { Linear,Smooth,Bezier,Curve,Spline,Step };
enum class TangentMode:unsigned { Auto,Linear,Free,Aligned,Broken };
enum class RotationInterpolation:unsigned { Slerp,Squad,Step };
enum class ScalarInterpolation:unsigned { Linear,Smooth,Step };
enum class TimingMode:unsigned { KeyframeTime,ConstantSpeed,TimeRemap };
enum class RotationMode:unsigned { Manual,LookAt,LookAlongPath,LookAtRoll };
enum class TargetType:unsigned { World,RecordedPlayer,RecordedActor,Keyframed };
enum Channel:unsigned { Position=1,Rotation=2,Fov=4,Roll=8,Focus=16,Target=32,PoseChannels=15,AllChannels=63 };
struct State {
 Vec position{};Quat orientation{0,0,0,1};double fov_degrees=60;
 double roll_degrees=0,focus_distance=0;Vec look_at_target{};bool has_look_at_target=false;
};
struct Key {
 std::uint64_t id=0,time_ns=0;State state;
 Interpolation outgoing=Interpolation::Spline;
 Vec tangent_in{},tangent_out{};
 double ease_in=0,ease_out=0; // legacy Curve x controls fixed at 1/3, 2/3
 bool constant_speed=false;
 unsigned channels=PoseChannels;
 TangentMode tangent_mode=TangentMode::Free; // existing handles retain their meaning
 RotationInterpolation rotation_interpolation=RotationInterpolation::Squad;
 ScalarInterpolation scalar_interpolation=ScalarInterpolation::Smooth;
 EaseCurve position_ease,rotation_ease,scalar_ease;
};
struct TrackSettings {
 TimingMode timing=TimingMode::KeyframeTime;
 double world_units_per_second=0; // 0 = fit total path length to first/last position times
 unsigned arc_samples=256;
 EaseCurve time_remap;
 RotationMode rotation=RotationMode::Manual;
 TargetType target=TargetType::World;
 Vec target_position{},target_offset{};
 std::uint64_t target_actor_id=0;
 bool valid()const {return unsigned(timing)<=2&&unsigned(rotation)<=3&&unsigned(target)<=3&&std::isfinite(world_units_per_second)&&world_units_per_second>=0&&arc_samples>=16&&arc_samples<=8192&&time_remap.valid()&&finite(target_position)&&finite(target_offset);}
};
struct EvaluationDebug {std::size_t segment=0;double normalized_time=0,parameter=0,distance=0,total_distance=0;Interpolation interpolation=Interpolation::Spline;bool target_resolved=false;};
// Resolver consumes stable recorded identity and *the requested replay timestamp*.
// It must never substitute a stale live actor or a runtime pointer.
using TargetResolver=std::function<std::optional<Vec>(TargetType,std::uint64_t,std::uint64_t)>;
inline bool valid(const State&s){return finite(s.position)&&normalized(s.orientation).has_value()&&std::isfinite(s.fov_degrees)&&s.fov_degrees>0&&s.fov_degrees<180&&std::isfinite(s.roll_degrees)&&std::isfinite(s.focus_distance)&&s.focus_distance>=0&&finite(s.look_at_target);}
class Track {
 std::vector<Key> keys_;
 TrackSettings settings_;
 std::array<std::vector<std::size_t>,6> channels_;
 struct Geometry {CubicSegment curve;ArcLengthTable arc;std::vector<Vec> ups;};
 std::vector<Geometry> geometry_;
 std::vector<double> prefixes_;
 std::vector<Quat> squad_in_,squad_out_;
 std::uint64_t geometry_generation_=0;
 std::size_t segment(const std::vector<std::size_t>& index,std::uint64_t time)const {
  auto next=std::upper_bound(index.begin(),index.end(),time,[&](auto t,auto i){return t<keys_[i].time_ns;});
  if(next==index.begin())return 0;return std::min<std::size_t>(next-index.begin()-1,index.size()-2);
 }
 static double fraction(const Key&a,const Key&b,std::uint64_t time){if(time<=a.time_ns)return 0;if(time>=b.time_ns)return 1;return double(time-a.time_ns)/double(b.time_ns-a.time_ns);}
 double position_time(const Key&key,double u)const {
  if(key.outgoing==Interpolation::Smooth)u=u*u*(3-2*u);
  if(key.outgoing==Interpolation::Curve){double v=1-u;u=3*v*v*u*key.ease_in+3*v*u*u*(1-key.ease_out)+u*u*u;}
  return key.position_ease.evaluate(u);
 }
 Vec auto_handle(std::size_t i)const {
  const auto&index=channels_[0];const auto&p=keys_[index[i]].state.position;
  if(index.size()<2)return {};
  if(i==0)return mul(sub(keys_[index[1]].state.position,p),1./3);
  if(i+1==index.size())return mul(sub(p,keys_[index[i-1]].state.position),1./3);
  return mul(sub(keys_[index[i+1]].state.position,keys_[index[i-1]].state.position),1./6);
 }
 void rebuild(bool reuse_geometry=false){
  for(auto&c:channels_)c.clear();for(std::size_t i=0;i<keys_.size();++i)for(int c=0;c<6;++c)if(keys_[i].channels&(1u<<c))channels_[c].push_back(i);
  for(std::size_t i=1;i<channels_[1].size();++i){auto index=channels_[1][i],prev=channels_[1][i-1];keys_[index].state.orientation=hemisphere(keys_[index].state.orientation,keys_[prev].state.orientation);}
  const auto&idx=channels_[0];
  if(!reuse_geometry){++geometry_generation_;geometry_.clear();prefixes_.assign(1,0);
  for(std::size_t i=0;i+1<idx.size();++i){const auto&a=keys_[idx[i]];const auto&b=keys_[idx[i+1]];auto p=a.state.position,q=b.state.position;
   auto curve=CubicSegment::linear(p,q);
   if(a.outgoing==Interpolation::Step)curve={p,{},{},{}};
   else if(a.outgoing==Interpolation::Spline)curve=CubicSegment::catmull(i?keys_[idx[i-1]].state.position:sub(mul(p,2),q),p,q,i+2<idx.size()?keys_[idx[i+2]].state.position:sub(mul(q,2),p));
   else if(a.outgoing==Interpolation::Bezier){
    Vec h1=a.tangent_mode==TangentMode::Auto?auto_handle(i):a.tangent_mode==TangentMode::Linear?mul(sub(q,p),1./3):a.tangent_out;
    Vec h2=b.tangent_mode==TangentMode::Auto?mul(auto_handle(i+1),-1):b.tangent_mode==TangentMode::Linear?mul(sub(p,q),1./3):b.tangent_in;
    curve=CubicSegment::bezier(p,add(p,h1),add(q,h2),q);
   }
   Geometry g;g.curve=curve;g.arc.build(curve,settings_.arc_samples);prefixes_.push_back(prefixes_.back()+g.arc.length());geometry_.push_back(std::move(g));
  }
  // Cache rotation-minimizing frames for Look Along Path. Parallel transport
  // uses the shortest rotation between neighboring analytical tangents;
  // zero curvature/zero derivative keeps the last valid frame, not Frenet.
  Vec up{0,1,0},previous{0,0,1};bool first=true;
  for(std::size_t i=0;i<geometry_.size();++i){auto&g=geometry_[i];if(i&&keys_[idx[i-1]].outgoing==Interpolation::Step)first=true;
   for(unsigned j=0;j<=settings_.arc_samples;++j){auto tangent=direction(g.curve.derivative(double(j)/settings_.arc_samples),previous);
    if(first){if(std::abs(dot(tangent,up))>.99)up={1,0,0};first=false;}
    else{auto axis=cross(previous,tangent);double sine=length(axis),cosine=std::clamp(dot(previous,tangent),-1.,1.);
     if(sine>1e-10){axis=mul(axis,1/sine);double angle=std::atan2(sine,cosine);up=add(add(mul(up,std::cos(angle)),mul(cross(axis,up),std::sin(angle))),mul(axis,dot(axis,up)*(1-std::cos(angle))));}
    }
    up=direction(sub(up,mul(tangent,dot(up,tangent))),{0,1,0});g.ups.push_back(up);previous=tangent;
   }
  }
  } // Geometry/tangent frame cache
  const auto&r=channels_[1];squad_in_.clear();squad_out_.clear();
  for(std::size_t i=0;i<r.size();++i){auto q=keys_[r[i]].state.orientation;
   if(i==0||i+1==r.size()||keys_[r[i-1]].rotation_interpolation==RotationInterpolation::Step){squad_in_.push_back(q);squad_out_.push_back(q);continue;}
   auto prev=hemisphere(keys_[r[i-1]].state.orientation,q),next=hemisphere(keys_[r[i+1]].state.orientation,q);
   auto left=qlog(compose(conjugate(q),prev)),right=qlog(compose(conjugate(q),next));
   double h0=double(keys_[r[i]].time_ns-keys_[r[i-1]].time_ns)/1e9,h1=double(keys_[r[i+1]].time_ns-keys_[r[i]].time_ns)/1e9;
   // Time-weighted angular tangent. Separate incoming/outgoing controls
   // preserve angular velocity in seconds even for unequal key intervals.
   auto velocity=mul(add(mul(left,-h1/h0),mul(right,h0/h1)),1/(h0+h1));
   squad_in_.push_back(compose(q,qexp(mul(sub(mul(left,-1),mul(velocity,h0)),.5))));
   squad_out_.push_back(compose(q,qexp(mul(sub(mul(velocity,h1),right),.5))));
  }
 }
 double scalar(std::uint64_t time,int channel,double State::* member,double fallback)const {
  const auto&idx=channels_[channel];if(idx.empty())return fallback;
  if(idx.size()==1||time<=keys_[idx.front()].time_ns)return keys_[idx.front()].state.*member;
  if(time>=keys_[idx.back()].time_ns)return keys_[idx.back()].state.*member;
  auto i=segment(idx,time);const auto&a=keys_[idx[i]];const auto&b=keys_[idx[i+1]];
  if(a.scalar_interpolation==ScalarInterpolation::Step)return a.state.*member;
  double u=a.scalar_ease.evaluate(fraction(a,b,time)),p=a.state.*member,q=b.state.*member;
  if(a.scalar_interpolation==ScalarInterpolation::Linear)return p+(q-p)*u;
  auto slope=[&](std::size_t j){
   std::size_t lo=j?j-1:j,hi=j+1<idx.size()?j+1:j;
   return (keys_[idx[hi]].state.*member-keys_[idx[lo]].state.*member)/(double(keys_[idx[hi]].time_ns-keys_[idx[lo]].time_ns)/1e9);
  };
  double h=double(b.time_ns-a.time_ns)/1e9,m=slope(i)*h,n=slope(i+1)*h;
  return p+(m+((3*(q-p)-2*m-n)+(2*(p-q)+m+n)*u)*u)*u;
 }
public:
 const std::vector<Key>&keys()const{return keys_;}
 const TrackSettings&settings()const{return settings_;}
 std::uint64_t geometry_generation()const{return geometry_generation_;}
 double total_length()const{return prefixes_.empty()?0:prefixes_.back();}
 const ArcLengthTable* arc(std::size_t i)const{return i<geometry_.size()?&geometry_[i].arc:nullptr;}
 std::optional<State> evaluate_seconds(double time,const TargetResolver& resolver={},EvaluationDebug* debug=nullptr)const {
  if(!std::isfinite(time)||time<0||time>=double(UINT64_MAX)/1e9)return {};
  return evaluate(static_cast<std::uint64_t>(time*1e9),resolver,debug);
 }
 bool replace(std::vector<Key> keys){return replace(std::move(keys),settings_);}
 bool replace(std::vector<Key> keys,TrackSettings settings){
  if(!settings.valid())return false;
  std::sort(keys.begin(),keys.end(),[](const Key&a,const Key&b){return a.time_ns==b.time_ns?a.id<b.id:a.time_ns<b.time_ns;});
  std::vector<std::uint64_t> ids;unsigned used=0;std::uint64_t timestamp=0;
  for(auto&k:keys){
   if(!k.id||!valid(k.state)||!k.channels||(k.channels&~AllChannels)||!finite(k.tangent_in)||!finite(k.tangent_out)||unsigned(k.outgoing)>5||unsigned(k.rotation_interpolation)>2||unsigned(k.scalar_interpolation)>2||unsigned(k.tangent_mode)>2||!k.position_ease.valid()||!k.rotation_ease.valid()||!k.scalar_ease.valid()||!std::isfinite(k.ease_in)||!std::isfinite(k.ease_out)||k.ease_in<0||k.ease_in>1||k.ease_out<0||k.ease_out>1)return false;
   if(k.time_ns!=timestamp){used=0;timestamp=k.time_ns;}if(used&k.channels)return false;used|=k.channels;
   if(settings.timing!=TimingMode::KeyframeTime&&(k.channels&Position)&&k.outgoing==Interpolation::Step)return false;
   k.state.orientation=*normalized(k.state.orientation);ids.push_back(k.id);
  }
  std::sort(ids.begin(),ids.end());if(std::adjacent_find(ids.begin(),ids.end())!=ids.end())return false;
  // Construct caches transactionally: failed finite/overflow validation does
  // not replace the previous editable track.
  bool reuse=settings.arc_samples==settings_.arc_samples;
  std::vector<const Key*> old_positions,new_positions;
  for(auto&key:keys_)if(key.channels&Position)old_positions.push_back(&key);
  for(auto&key:keys)if(key.channels&Position)new_positions.push_back(&key);
  if(old_positions.size()!=new_positions.size())reuse=false;
  if(reuse)for(std::size_t i=0;i<old_positions.size();++i){const auto&a=*old_positions[i],&b=*new_positions[i];if(a.state.position!=b.state.position||a.outgoing!=b.outgoing||a.tangent_in!=b.tangent_in||a.tangent_out!=b.tangent_out||a.tangent_mode!=b.tangent_mode){reuse=false;break;}}
  Track next;next.geometry_generation_=geometry_generation_;
  if(reuse){next.geometry_=geometry_;next.prefixes_=prefixes_;}
  next.keys_=std::move(keys);next.settings_=settings;next.rebuild(reuse);
  if(!std::isfinite(next.total_length()))return false;
  for(auto&g:next.geometry_)if(!finite(g.curve.a)||!finite(g.curve.b)||!finite(g.curve.c)||!finite(g.curve.d))return false;
  *this=std::move(next);return true;
 }
 std::optional<State> evaluate(std::uint64_t time,const TargetResolver& resolver={},EvaluationDebug* debug=nullptr)const {
  if(keys_.empty())return {};State out=keys_.front().state;out.has_look_at_target=false;
  EvaluationDebug info;info.total_distance=total_length();
  const auto&p=channels_[0];std::size_t pi=0;double pu=0;
  if(p.size()==1)out.position=keys_[p.front()].state.position;
  else if(p.size()>1){
   pi=segment(p,time);const auto&a=keys_[p[pi]],&b=keys_[p[pi+1]];
   double u=fraction(a,b,time);info.normalized_time=u;
   if(settings_.timing==TimingMode::KeyframeTime){pu=position_time(a,u);if(a.constant_speed)pu=geometry_[pi].arc.distance_to_parameter(pu*geometry_[pi].arc.length());}
   else{
    double elapsed=time>keys_[p.front()].time_ns?double(time-keys_[p.front()].time_ns)/1e9:0;
    double duration=double(keys_[p.back()].time_ns-keys_[p.front()].time_ns)/1e9;
    double distance=settings_.timing==TimingMode::ConstantSpeed?(settings_.world_units_per_second>0?elapsed*settings_.world_units_per_second:elapsed/duration*total_length()):settings_.time_remap.evaluate(std::clamp(elapsed/duration,0.,1.))*total_length();
    distance=std::clamp(distance,0.,total_length());auto hi=std::upper_bound(prefixes_.begin(),prefixes_.end(),distance);pi=std::min<std::size_t>(std::max<std::size_t>(hi-prefixes_.begin(),1)-1,geometry_.size()-1);
    pu=geometry_[pi].arc.distance_to_parameter(distance-prefixes_[pi]);
   }
   out.position=geometry_[pi].curve.evaluate(pu);
   if(settings_.timing==TimingMode::KeyframeTime&&time>=keys_[p.back()].time_ns)out.position=keys_[p.back()].state.position;
   info.segment=pi;info.parameter=pu;info.distance=prefixes_[pi]+geometry_[pi].arc.parameter_to_distance(pu);info.interpolation=keys_[p[pi]].outgoing;
  }
  const auto&r=channels_[1];
  if(!r.empty()){
   if(r.size()==1||time<=keys_[r.front()].time_ns)out.orientation=keys_[r.front()].state.orientation;
   else if(time>=keys_[r.back()].time_ns)out.orientation=keys_[r.back()].state.orientation;
   else{auto i=segment(r,time);const auto&a=keys_[r[i]],&b=keys_[r[i+1]];double u=a.rotation_ease.evaluate(fraction(a,b,time));
    out.orientation=a.rotation_interpolation==RotationInterpolation::Step?a.state.orientation:a.rotation_interpolation==RotationInterpolation::Squad&&r.size()>2?squad(a.state.orientation,squad_out_[i],squad_in_[i+1],b.state.orientation,u):slerp(a.state.orientation,b.state.orientation,u);}
  }
  out.fov_degrees=std::clamp(scalar(time,2,&State::fov_degrees,out.fov_degrees),.001,179.999);
  out.roll_degrees=scalar(time,3,&State::roll_degrees,0);out.focus_distance=std::max(0.,scalar(time,4,&State::focus_distance,0));
  std::optional<Vec> target;
  if(settings_.target==TargetType::World)target=settings_.target_position;
  else if(settings_.target==TargetType::Keyframed){const auto&t=channels_[5];if(!t.empty()){if(t.size()==1||time<=keys_[t.front()].time_ns)target=keys_[t.front()].state.look_at_target;else if(time>=keys_[t.back()].time_ns)target=keys_[t.back()].state.look_at_target;else{auto i=segment(t,time);target=mix(keys_[t[i]].state.look_at_target,keys_[t[i+1]].state.look_at_target,keys_[t[i]].scalar_ease.evaluate(fraction(keys_[t[i]],keys_[t[i+1]],time)));}}}
  else if(resolver)target=resolver(settings_.target,settings_.target_actor_id,time);
  if(target&&finite(*target))target=add(*target,settings_.target_offset);else target.reset();
  if(settings_.rotation==RotationMode::LookAlongPath&&!geometry_.empty()){
   const auto&g=geometry_[pi];double v=pu*settings_.arc_samples;auto i=std::min<std::size_t>(std::size_t(v),g.ups.size()-2);auto up=direction(mix(g.ups[i],g.ups[i+1],v-i));
   out.orientation=look_rotation(g.curve.derivative(pu),up,out.orientation);
  }else if((settings_.rotation==RotationMode::LookAt||settings_.rotation==RotationMode::LookAtRoll)&&target){out.orientation=look_rotation(sub(*target,out.position),{0,1,0},out.orientation);out.look_at_target=*target;out.has_look_at_target=true;info.target_resolved=true;}
  if(settings_.rotation!=RotationMode::LookAt&&out.roll_degrees!=0)out.orientation=*normalized(compose(out.orientation,qexp({0,0,out.roll_degrees*3.141592653589793/360})));
  if(debug)*debug=info;return valid(out)?std::optional<State>(out):std::nullopt;
 }
};
}

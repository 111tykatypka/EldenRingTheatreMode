#include "../shared/CameraProject.h"
#include "../shared/CameraLiveDamping.h"
#include <iostream>
#include <stdexcept>
using namespace cinematic;
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
bool near(double a,double b,double e=1e-7){return std::abs(a-b)<e;}
double angle(Quat a,Quat b){return 2*std::acos(std::clamp(std::abs(qdot(a,b)),0.,1.));}
Key key(unsigned id,double seconds,Vec pos){Key k;k.id=id;k.time_ns=std::uint64_t(seconds*1e9);k.state.position=pos;return k;}
int main(){try{
 for(int i=0;i<100;++i){auto q=*mouse_look({0,0,0,1},i*.07,i*.006,i*.1);auto [base,roll]=split_roll(q);auto restored=compose(base,qexp({0,0,roll*3.141592653589793/360}));require(angle(q,restored)<1e-7,"independent roll capture round trip");}
 CriticalSpring spring30,spring120;for(int i=0;i<30;++i)require(spring30.advance({1,2,3},1./30,8),"spring step");for(int i=0;i<120;++i)require(spring120.advance({1,2,3},1./120,8),"spring step");require(length(sub(spring30.position,spring120.position))<1e-12&&length(sub(spring30.velocity,spring120.velocity))<1e-12,"live spring frame-rate independence");require(!spring30.advance({NAN,0,0},.01,8),"invalid spring target");
 Track path;require(!path.evaluate(0),"empty track");
 auto a=key(1,0,{0,0,0}),b=key(2,3,{2,1,0}),c=key(3,6,{4,0,3}),d=key(4,10,{8,2,4});
 a.state.orientation=*mouse_look(a.state.orientation,0,.1,0);b.state.orientation=*mouse_look(b.state.orientation,.7,.2,0);c.state.orientation=*mouse_look(c.state.orientation,1.3,.4,.1);d.state.orientation=*mouse_look(d.state.orientation,2,.1,.2);
 require(path.replace({a}),"single replace");require(path.evaluate(999)->position==a.state.position,"single pose");
 require(path.replace({a,b}),"two replace");require(valid(*path.evaluate(1000000000)),"two finite");
 require(path.replace({d,b,a,c}),"sort keys");
 auto first=*path.evaluate(5000000000);for(auto t:{1ULL,20000000000ULL,5000000000ULL,7000000000ULL,5000000000ULL})require(valid(*path.evaluate(t)),"random seek finite");
 auto again=*path.evaluate(5000000000);require(first.position==again.position&&first.orientation==again.orientation&&first.fov_degrees==again.fov_degrees,"history independence");
 require(path.evaluate_seconds(5)->position==first.position&&!path.evaluate_seconds(NAN)&&!path.evaluate_seconds(-1),"seconds API");
 for(auto k:path.keys()){auto at=*path.evaluate(k.time_ns);require(length(sub(at.position,k.state.position))<1e-8,"exact spatial key");require(angle(at.orientation,k.state.orientation)<1e-7,"exact rotation key");}
 for(auto t:{b.time_ns,c.time_ns}){auto left=*path.evaluate(t-1000),right=*path.evaluate(t+1000);require(length(sub(left.position,right.position))<1e-4&&angle(left.orientation,right.orientation)<1e-4,"boundary continuity");}
 // Nonuniform-time SQUAD derivative must match on either side of a key.
 auto left=qlog(compose(conjugate(path.evaluate(b.time_ns-100000)->orientation),path.evaluate(b.time_ns)->orientation));
 auto right=qlog(compose(conjugate(path.evaluate(b.time_ns)->orientation),path.evaluate(b.time_ns+100000)->orientation));
 require(length(sub(left,right))<2e-8,"SQUAD angular velocity continuity");
 TrackSettings speed;speed.timing=TimingMode::ConstantSpeed;speed.arc_samples=1024;
 require(path.replace({a,b,c,d},speed),"global constant speed");double min=1e9,max=0;auto prev=path.evaluate(0)->position;
 for(unsigned i=1;i<=2000;++i){auto p=path.evaluate(i*5000000ULL)->position;double dist=length(sub(p,prev));min=std::min(min,dist);max=std::max(max,dist);prev=p;}
 require(max/min<1.02,"approximately uniform world speed");
 auto arc=path.arc(1);for(int i=0;i<=100;++i){double u=i/100.;require(near(arc->distance_to_parameter(arc->parameter_to_distance(u)),u,1e-7),"arc round trip");}
 speed.world_units_per_second=1;require(path.replace({a,b,c,d},speed),"explicit speed");EvaluationDebug dbg;path.evaluate(2000000000,{},&dbg);require(near(dbg.distance,2,1e-6),"world units per second");
 EaseCurve ease;for(auto mode:{Easing::Linear,Easing::EaseIn,Easing::EaseOut,Easing::EaseInOut,Easing::Smoothstep,Easing::Smootherstep,Easing::CubicBezier}){ease.mode=mode;require(ease.evaluate(0)==0&&ease.evaluate(1)==1,"ease endpoints");double previous=0;for(int i=0;i<=100;++i){auto value=ease.evaluate(i/100.);require(value>=previous&&value<=1,"ease monotonic");previous=value;}}
 ease.mode=Easing::CubicBezier;ease.x1=.1;ease.y1=0;ease.x2=.2;ease.y2=1;require(ease.evaluate(.5)>.65,"Bezier time inversion");
 speed.timing=TimingMode::TimeRemap;speed.time_remap=ease;require(path.replace({a,b,c,d},speed),"time remap");path.evaluate(5000000000,{},&dbg);require(near(dbg.distance,ease.evaluate(.5)*path.total_length(),1e-6),"remapped distance");
 // Position and scalar keys use different times; disjoint same-time keys are legal.
 a.channels=b.channels=c.channels=d.channels=Position|Rotation;auto f=key(5,1,{99,99,99});f.channels=Fov;f.state.fov_degrees=35;auto f2=f;f2.id=6;f2.time_ns=4000000000;f2.state.fov_degrees=70;
 auto roll=key(7,0,{});roll.channels=Roll;roll.state.roll_degrees=0;auto roll2=roll;roll2.id=8;roll2.time_ns=10000000000;roll2.state.roll_degrees=25;
 require(path.replace({a,b,c,d,f,f2,roll,roll2},{}),"independent channels");require(near(path.evaluate(1000000000)->fov_degrees,35),"independent FOV key");require(near(path.evaluate(4000000000)->fov_degrees,70),"second FOV key");require(near(path.evaluate(10000000000)->roll_degrees,25),"roll channel");
 auto before=path.evaluate(2500000000)->position;auto cache_generation=path.geometry_generation();f.state.fov_degrees=45;require(path.replace({a,b,c,d,f,f2,roll,roll2},{}),"scalar edit");require(path.evaluate(2500000000)->position==before,"FOV does not affect path");require(path.geometry_generation()==cache_generation,"FOV edit reuses geometry cache");
 auto duplicate=a;duplicate.id=99;require(!path.replace({a,duplicate}),"reject duplicate channel time");require(path.evaluate(2500000000)->position==before,"transactional failure");
 TrackSettings look;look.rotation=RotationMode::LookAtRoll;look.target_position={10,4,10};require(path.replace({a,b,c,d,roll,roll2},look),"world target");require(path.evaluate(2000000000)->has_look_at_target,"look target state");
 look.target=TargetType::RecordedPlayer;require(path.replace({a,b,c,d},look),"recorded target config");std::uint64_t requested=0;auto target=[&](TargetType type,std::uint64_t,std::uint64_t t)->std::optional<Vec>{requested=t;require(type==TargetType::RecordedPlayer,"target type");return Vec{double(t)/1e9,2,8};};
 require(path.evaluate(123456789,target)->has_look_at_target&&requested==123456789,"target same timestamp");require(!path.evaluate(123456789)->has_look_at_target,"missing target no fake data");
 look.rotation=RotationMode::LookAlongPath;auto vertical0=key(11,0,{0,0,0}),vertical1=key(12,1,{0,1,0}),vertical2=key(13,2,{.1,2,0});require(path.replace({vertical0,vertical1,vertical2},look),"vertical path");Quat q=path.evaluate(0)->orientation;for(int i=1;i<=200;++i){auto s=*path.evaluate(i*10000000ULL);require(valid(s)&&angle(q,s.orientation)<.1,"parallel transport stability");q=s.orientation;}
 auto zero=a;zero.state.position=b.state.position;require(path.replace({zero,b,c}),"duplicate positions");for(auto t:{0ULL,1ULL,1000000ULL,2000000000ULL,3000000000ULL})require(valid(*path.evaluate(t)),"zero segment finite");
 auto tiny=b;tiny.time_ns=1;tiny.state.position={1e-12,0,0};require(path.replace({a,tiny}),"tiny segment");require(valid(*path.evaluate(1)),"tiny finite");
 b.state.orientation={0,1,0,0};a.state.orientation={0,0,0,-1};require(path.replace({a,b,c}),"180 degree transition");for(int i=0;i<100;++i)require(valid(*path.evaluate(i*30000000ULL)),"180 finite");
 a.outgoing=Interpolation::Bezier;a.tangent_mode=TangentMode::Auto;b.tangent_mode=TangentMode::Auto;require(path.replace({a,b,c}),"auto handles");require(path.evaluate(1500000000)->position!=mix(a.state.position,b.state.position,.5),"curved auto Bezier");
 a.outgoing=Interpolation::Step;require(path.replace({a,b}),"step");require(path.evaluate(b.time_ns-1)->position==a.state.position,"intentional cut");require(!path.replace({a,b},speed),"do not smooth across cut");
 std::string identity="Unicode \\xD0\\x91.erplay";a.outgoing=Interpolation::Spline;require(path.replace({a,b,c,f,f2,roll,roll2},look),"serial track");auto text=save_project(identity,path.keys(),look);std::vector<Key> restored;TrackSettings settings;require(load_project(text,identity,restored,&settings),"v2 round trip");Track other;require(other.replace(restored,settings),"rebuild v2");require(other.evaluate(1000000000)->position==path.evaluate(1000000000)->position,"serialized evaluation");
 const std::string legacy="ERTCAM 1\n\"old\"\n1\n1 0 4 0 0 0 0 0 0 0 1 60 0 0 0 0 0 0 0 0\n";require(load_project(legacy,"old",restored,&settings),"v1 migration");require(restored.front().rotation_interpolation==RotationInterpolation::Slerp,"legacy rotation preserved");
 require(!load_project(text+"junk",identity,restored),"trailing garbage rejected");require(!load_project(text,"wrong",restored),"identity mismatch");
 std::cout<<"Deterministic channels, Catmull-Rom, SQUAD, arc length, timing curves, targets, parallel transport, independent FOV/roll, v1/v2 and degenerate-case tests passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

#include "../shared/CinematicCamera.h"
#include "../shared/CameraTelemetry.h"
#include <iostream>
#include <stdexcept>
#include <limits>
void check(bool ok){if(!ok)throw std::runtime_error("cinematic check failed");}
int main(){using namespace cinematic;
 Track track;Key a,b,c;a.id=1;b.id=2;c.id=3;b.time_ns=1000000000;c.time_ns=2000000000;b.state.position={2,0,0};c.state.position={3,1,0};
 check(!track.evaluate(0));check(track.replace({a,b,c}));check(track.evaluate(b.time_ns)->position==b.state.position);
 check(track.evaluate(0)->position==a.state.position);check(track.evaluate(3000000000)->position==c.state.position);
 for(auto mode:{Interpolation::Linear,Interpolation::Smooth,Interpolation::Bezier,Interpolation::Curve,Interpolation::Spline,Interpolation::Step}){
  a.outgoing=mode;a.constant_speed=true;a.tangent_out={0,1,0};b.tangent_in={0,-1,0};check(track.replace({a,b,c}));
  for(unsigned n=0;n<200;++n){auto s=track.evaluate(n*10000000ULL);check(s&&valid(*s));}}
 a.outgoing=Interpolation::Linear;a.constant_speed=false;b.state.orientation={0,0,0,-1};check(track.replace({a,b}));check(std::abs(track.evaluate(500000000)->orientation[3]-1)<1e-6);
 a.outgoing=Interpolation::Step;check(track.replace({a,b}));check(track.evaluate(999999999)->position==a.state.position);check(track.evaluate(1000000000)->position==b.state.position);
 b.id=1;check(!track.replace({a,b}));b.id=2;b.time_ns=0;check(!track.replace({a,b}));b.time_ns=1000000000;b.state.fov_degrees=std::numeric_limits<double>::quiet_NaN();check(!track.replace({a,b}));
 check(finite(spline({0,0,0},{0,0,0},{1,0,0},{1,0,0},.5)));
 check(frame_time(0,10000000000ULL,599,60)==9983333333ULL);check(!frame_time(0,10000000000ULL,600,60));check(!frame_time(0,1,0,0));
 check(frame_time(0,10000000000ULL,1,24000,1001)==41708333ULL);check(!frame_time(0,10000000000ULL,UINT64_MAX,60));
 a.outgoing=Interpolation::Spline;a.constant_speed=true;b.state.orientation={0,0,0,1};b.state.fov_degrees=60;
 check(track.replace({a,b,c}));double shortest=1e9,longest=0;auto previous=track.evaluate(0)->position;
 for(int n=1;n<=20;++n){auto p=track.evaluate(n*50000000ULL)->position;auto d=length(sub(p,previous));shortest=std::min(shortest,d);longest=std::max(longest,d);previous=p;}
 check(longest/shortest<1.05);
 theater_camera::Slot slot;check(!theater_camera::valid(slot));for(int i=0;i<4;++i)slot.matrix[i*4+i]=1;slot.fov=1;slot.aspect=1.77f;slot.near_plane=.1f;slot.far_plane=1000;check(theater_camera::valid(slot));slot.matrix[0]=std::numeric_limits<float>::infinity();check(!theater_camera::valid(slot));
 std::cout<<"Camera path, SLERP, boundary, invalid-data, frame-time and telemetry checks passed\n";
}

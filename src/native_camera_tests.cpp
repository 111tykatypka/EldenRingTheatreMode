#include "../native_ui/CinematicCameraRuntime.h"
#include <stdexcept>
#include <iostream>
void check(bool ok){if(!ok)throw std::runtime_error("native camera check failed");}
int main(){
 theater_camera::Slot c;c.fov=1;c.aspect=1.777f;c.near_plane=.1f;c.far_plane=1000;
 for(double angle:{0.,.01,1.,3.14,4.,6.27}){
  cinematic::State s{{4,5,6},{0,std::sin(angle/2),0,std::cos(angle/2)},57.295779513};
  camera_runtime::encode_pose(s,c.matrix);auto actual=camera_runtime::decode_candidate(c);check(actual.has_value());
  double dot=0;for(int i=0;i<4;++i)dot+=s.orientation[i]*actual->orientation[i];check(std::abs(std::abs(dot)-1)<1e-6);check(actual->position==s.position);
 }
 c.matrix[0]=0;check(!camera_runtime::decode_candidate(c));c.fov=60;check(!camera_runtime::decode_candidate(c));
 c.fov=1;
 for(cinematic::Quat q:std::vector<cinematic::Quat>{{1,0,0,0},{0,1,0,0},{0,0,1,0},{.1,.2,.3,.9}}){
  q=*cinematic::normalized(q);cinematic::State s{{4,5,6},q,57.295779513};camera_runtime::encode_pose(s,c.matrix);
  auto actual=camera_runtime::decode_candidate(c);check(actual.has_value());double dot=0;for(int i=0;i<4;++i)dot+=q[i]*actual->orientation[i];check(std::abs(std::abs(dot)-1)<1e-6);
 }
 camera_runtime::mode(1);camera_runtime::enable(true);check(!camera_runtime::view().enabled);camera_runtime::add_key();check(camera_runtime::view().keys.empty());
 camera_runtime::stop();check(camera_runtime::view().mode==0&&!camera_runtime::owns_input());
 std::cout<<"Native camera conversion and fail-closed control checks passed\n";
}

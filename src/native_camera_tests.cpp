#include "../native_ui/CinematicCameraRuntime.h"
#include "../native_ui/EldenRingTimingAdapter.h"
#include "../native_ui/NativeCameraMemory.h"
#include <windows.h>
#include <stdexcept>
#include <iostream>
#include <cstring>
extern "C" {void tm_camera_detour();extern void* tm_camera_original;}
void native_copy_fixture(void*source,void*destination){std::memcpy(static_cast<char*>(destination)+16,static_cast<char*>(source)+16,80);}
void check(bool ok){if(!ok)throw std::runtime_error("native camera check failed");}
int main(){
 game_timing::enable(false);tm_world_timing_tick(0,1);check(!game_timing::enabled());
 game_timing::enable(true);tm_world_timing_tick(1,.5);check(game_timing::status()=="Timing binding unavailable/rejected");
 game_timing::enable(false);tm_world_timing_tick(0,1);check(game_timing::status()=="World timing OFF");
 theater_camera::Slot c;c.fov=1;c.aspect=1.777f;c.near_plane=.1f;c.far_plane=1000;
 {
  auto page=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);check(page!=nullptr);
  float matrix[16]{};cinematic::State pose{{4,5,6},{0,0,0,1},60};camera_runtime::encode_pose(pose,matrix);
  check(camera_local_write(page,matrix,1.f));float result[20]{};check(camera_local_read(page,result));check(std::memcmp(result,matrix,64)==0&&result[16]==1.f);
  DWORD old=0;check(VirtualProtect(page,4096,PAGE_READONLY,&old)!=0);check(!camera_local_write(page,matrix,2.f));check(camera_local_read(page,result)&&result[16]==1.f);
  check(VirtualProtect(page,4096,PAGE_NOACCESS,&old)!=0);check(!camera_local_read(page,result));check(!camera_local_write(page,matrix,1.f));check(VirtualFree(page,0,MEM_RELEASE)!=0);
  check(!camera_local_read(nullptr,result));check(!camera_local_write(nullptr,matrix,1.f));
 }
 {
  alignas(16) unsigned char source[96]{},destination[96]{};
  cinematic::State pose{{4,5,6},{0,0,0,1},57.295779513};camera_runtime::encode_pose(pose,c.matrix);std::memcpy(source+16,&c,80);
  tm_camera_original=reinterpret_cast<void*>(native_copy_fixture);
  // Offline ABI/pass-through fixture; no game hooks or native game writes.
  reinterpret_cast<void(*)(void*,void*)>(tm_camera_detour)(source,destination);
  check(std::memcmp(source+16,destination+16,80)==0);tm_camera_original=nullptr;
 }
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
 cinematic::State root{{10,20,30},{0,0,0,1},60};float matrix[16];camera_runtime::encode_pose(root,matrix);float qs[12]={1,2,3,0,0,0,0,1,1,1,1,0};
 auto attached=camera_runtime::bone_world(matrix,qs);check(attached&&attached->position==cinematic::Vec({11,22,33}));check(!camera_runtime::bone_world(nullptr,qs));qs[4]=NAN;check(!camera_runtime::bone_world(matrix,qs));
 qs[4]=0;root.orientation={0,std::sqrt(.5),0,std::sqrt(.5)};camera_runtime::encode_pose(root,matrix);attached=camera_runtime::bone_world(matrix,qs);check(attached&&cinematic::length(cinematic::sub(attached->position,{13,22,29}))<1e-5);
 std::cout<<"Native camera conversion and fail-closed control checks passed\n";
}

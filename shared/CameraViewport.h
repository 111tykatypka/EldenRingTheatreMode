#pragma once
#include "CinematicCamera.h"
namespace cinematic::viewport {
struct Screen {double x,y,depth;};
inline Vec basis(Quat q,int axis){auto unit=normalized(q);if(!unit)return {};q=*unit;const double x=q[0],y=q[1],z=q[2],w=q[3];
 if(axis==0)return {1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w)};
 if(axis==1)return {2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w)};
 return {2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y)};
}
// UI angles only (yaw Y, pitch X, roll Z); replay interpolation stays quaternion-based.
inline Vec angles(Quat q){auto forward=basis(q,2),right=basis(q,0),up=basis(q,1);
 return {std::asin(std::clamp(-forward[1],-1.,1.)),std::atan2(forward[0],forward[2]),std::atan2(right[1],up[1])};
}
inline double dot(Vec a,Vec b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
// Camera-to-world right/up/+Z-forward convention used by the native adapter.
// Editor projection assumes vertical FOV and the drawable window aspect; verify in game.
inline std::optional<Screen> project(const State& camera,Vec point,double width,double height){
 if(!valid(camera)||!finite(point)||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return {};
 Vec delta=sub(point,camera.position);double z=dot(delta,basis(camera.orientation,2));if(z<=.05)return {};
 double focal=height/(2*std::tan(camera.fov_degrees*3.141592653589793/360));
 Screen p{width*.5+dot(delta,basis(camera.orientation,0))*focal/z,height*.5-dot(delta,basis(camera.orientation,1))*focal/z,z};
 if(!std::isfinite(p.x)||!std::isfinite(p.y)||std::abs(p.x)>1e7||std::abs(p.y)>1e7)return {};return p;
}
inline std::optional<double> plane_angle(const State& camera,double x,double y,double width,double height,Vec center,int axis){
 if(!valid(camera)||!finite(center)||axis<0||axis>2||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return {};
 double focal=height/(2*std::tan(camera.fov_degrees*3.141592653589793/360));
 Vec ray=add(add(mul(basis(camera.orientation,0),(x-width*.5)/focal),mul(basis(camera.orientation,1),-(y-height*.5)/focal)),basis(camera.orientation,2));
 if(std::abs(ray[axis])<1e-6)return {};double distance=(center[axis]-camera.position[axis])/ray[axis];if(distance<=0)return {};
 Vec hit=sub(add(camera.position,mul(ray,distance)),center);
 const double a=hit[(axis+1)%3],b=hit[(axis+2)%3];if(!std::isfinite(a)||!std::isfinite(b)||std::hypot(a,b)<1e-6)return {};
 return std::atan2(b,a);
}
inline double segment_distance(double x,double y,Screen a,Screen b){
 const double dx=b.x-a.x,dy=b.y-a.y,length=dx*dx+dy*dy;
 const double t=length>1e-9?std::clamp(((x-a.x)*dx+(y-a.y)*dy)/length,0.,1.):0.;
 return std::hypot(x-a.x-t*dx,y-a.y-t*dy);
}
inline std::optional<Quat> rotate_world(Quat q,int axis,double radians){
 if(axis<0||axis>2||!normalized(q)||!std::isfinite(radians))return {};
 Quat turn{0,0,0,std::cos(radians/2)};turn[axis]=std::sin(radians/2);return normalized(compose(turn,q));
}
}

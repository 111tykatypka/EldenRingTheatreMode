#pragma once
#include "CameraMath.h"
#include <utility>
namespace cinematic {
inline double qdot(Quat a,Quat b){double v=0;for(int i=0;i<4;++i)v+=a[i]*b[i];return v;}
inline Quat hemisphere(Quat q,Quat reference){if(qdot(q,reference)<0)for(auto&v:q)v=-v;return q;}
inline Quat conjugate(Quat q){return {-q[0],-q[1],-q[2],q[3]};}
inline Vec qlog(Quat q){q=*normalized(q);double n=std::hypot(q[0],q[1],q[2]);double weight=n>1e-12?std::atan2(n,q[3])/n:1;return {q[0]*weight,q[1]*weight,q[2]*weight};}
inline Quat qexp(Vec v){double angle=length(v),weight=angle>1e-12?std::sin(angle)/angle:1;return {v[0]*weight,v[1]*weight,v[2]*weight,std::cos(angle)};}
inline Quat squad(Quat p,Quat a,Quat b,Quat q,double u){return slerp(slerp(p,q,u),slerp(a,b,u),2*u*(1-u));}
// Capture stores local roll independently. The remaining quaternion stays a
// quaternion channel; UI angles are not interpolated. At a vertical pole roll
// is ambiguous, so keep it in the base quaternion rather than invent an angle.
inline std::pair<Quat,double> split_roll(Quat q){
 q=normalized(q).value_or(Quat{0,0,0,1});
 double right_y=2*(q[0]*q[1]+q[3]*q[2]),up_y=1-2*(q[0]*q[0]+q[2]*q[2]);
 double roll=std::hypot(right_y,up_y)>1e-9?std::atan2(right_y,up_y):0;
 return {*normalized(compose(q,qexp({0,0,-roll*.5}))),roll*180/3.141592653589793};
}
inline Vec cross(Vec a,Vec b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline double dot(Vec a,Vec b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline Vec direction(Vec v,Vec fallback={0,0,1}){double n=length(v);return n>1e-12?mul(v,1/n):fallback;}
inline Quat basis_rotation(Vec right,Vec up,Vec forward){
 const double trace=right[0]+up[1]+forward[2];Quat q;
 if(trace>0){double s=2*std::sqrt(trace+1);q={(up[2]-forward[1])/s,(forward[0]-right[2])/s,(right[1]-up[0])/s,s/4};}
 else if(right[0]>up[1]&&right[0]>forward[2]){double s=2*std::sqrt(1+right[0]-up[1]-forward[2]);q={s/4,(up[0]+right[1])/s,(forward[0]+right[2])/s,(up[2]-forward[1])/s};}
 else if(up[1]>forward[2]){double s=2*std::sqrt(1+up[1]-right[0]-forward[2]);q={(up[0]+right[1])/s,s/4,(forward[1]+up[2])/s,(forward[0]-right[2])/s};}
 else{double s=2*std::sqrt(1+forward[2]-right[0]-up[1]);q={(forward[0]+right[2])/s,(forward[1]+up[2])/s,s/4,(right[1]-up[0])/s};}
 return normalized(q).value_or(Quat{0,0,0,1});
}
inline Quat look_rotation(Vec forward,Vec up,Quat fallback){
 if(length(forward)<1e-9)return fallback;forward=direction(forward);
 auto right=cross(up,forward);
 // Near a vertical view, project the authored right instead of switching
 // arbitrary world axes. No previous-frame orientation is consulted.
 if(length(right)<1e-5){auto q=fallback;Vec authored{1-2*(q[1]*q[1]+q[2]*q[2]),2*(q[0]*q[1]+q[3]*q[2]),2*(q[0]*q[2]-q[3]*q[1])};right=sub(authored,mul(forward,dot(authored,forward)));}
 right=direction(right,{1,0,0});return basis_rotation(right,cross(forward,right),forward);
}
}

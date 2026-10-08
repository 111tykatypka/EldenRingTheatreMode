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
// Filter angular input, not whole camera quaternions: world-up yaw remains level.
inline Vec take_smoothed_angles(Vec& pending,double dt,double seconds){
 const double weight=seconds>0?-std::expm1(-std::max(0.,dt)/seconds):1.;
 Vec applied=mul(pending,weight);pending=sub(pending,applied);return applied;
}
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
}

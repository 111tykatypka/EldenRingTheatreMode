//! UNKNOWN map never permits a normal arbitrary relocation.
use crate::transform_probe::Transform;
pub fn near(live:Transform,target:Transform)->bool {
 live.valid()&&target.valid()&&
 (f64::from(target.position[0])-f64::from(live.position[0])).hypot(f64::from(target.position[2])-f64::from(live.position[2]))<=1.0&&
 (f64::from(target.position[1])-f64::from(live.position[1])).abs()<=0.25
}
#[cfg(test)] mod tests {use super::*;
 #[test] fn refuses_unknown_scene_relocation(){let a=Transform{position:[0.;3],quaternion:[0.,0.,0.,1.]};
 for xyz in [[1.01,0.,0.],[0.,0.251,0.],[500.,0.,0.],[0.,f32::NAN,0.]]{assert!(!near(a,Transform{position:xyz,..a}));}
 assert!(near(a,Transform{position:[0.5,0.25,0.5],..a}));}
}

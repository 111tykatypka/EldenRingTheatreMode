//! Where the player's weapon models sit (hand, sheath, hidden): the part of `CSChrActionFlagModule` that animation
//! events set (TAE event 712 "OverrideWeaponModelLocations"). While a flask is drunk the game moves the weapon
//! away through this state; a replay that only drives bones leaves the live weapon in the hand, so the recorded
//! state is written back each frame. SDK-pinned struct, read as raw bytes, written only when it reads plausible.
use eldenring::cs::{ChrIns,CSChrActionFlagModule};
use std::mem::offset_of;

pub const BYTES:usize=17; // 4 left + 4 right models x (absorb-position condition, change type) + overridden flag
macro_rules! pairs{($($c:ident,$t:ident);*)=>{[$((offset_of!(CSChrActionFlagModule,$c),offset_of!(CSChrActionFlagModule,$t))),*]}}
fn pair_offsets()->[(usize,usize);8]{pairs!(lh_model0_absorp_pos_param_condition,lh_model0_change_type;lh_model1_absorp_pos_param_condition,lh_model1_change_type;lh_model2_absorp_pos_param_condition,lh_model2_change_type;lh_model3_absorp_pos_param_condition,lh_model3_change_type;
 rh_model0_absorp_pos_param_condition,rh_model0_change_type;rh_model1_absorp_pos_param_condition,rh_model1_change_type;rh_model2_absorp_pos_param_condition,rh_model2_change_type;rh_model3_absorp_pos_param_condition,rh_model3_change_type)}
pub(crate) fn module(chr:usize)->Option<usize>{
 let c=unsafe{&*(chr as *const ChrIns)};let m=c.modules.action_flag.as_ptr() as usize;(m>0x10000).then_some(m)}
/// Change types the SDK enum knows: -1..=6.
pub fn plausible(d:&[u8;BYTES])->bool{(0..8).all(|i|(-1..=6).contains(&(d[i*2+1] as i8)))&&d[16]<=1}
pub fn read(chr:usize)->Option<[u8;BYTES]>{
 let m=module(chr)?;let mut d=[0u8;BYTES];
 for (i,(c,t)) in pair_offsets().into_iter().enumerate(){
  let mut b=[0u8];if !crate::companions::copy(m+c,&mut b){return None;}d[i*2]=b[0];
  if !crate::companions::copy(m+t,&mut b){return None;}d[i*2+1]=b[0];}
 let mut b=[0u8];if !crate::companions::copy(m+offset_of!(CSChrActionFlagModule,weapon_model_location_overridden),&mut b){return None;}d[16]=b[0];
 plausible(&d).then_some(d)}
pub fn write(chr:usize,d:&[u8;BYTES])->bool{
 if !plausible(d){return false;}let Some(m)=module(chr) else {return false};if read(chr).is_none(){return false;}
 unsafe{for (i,(c,t)) in pair_offsets().into_iter().enumerate(){std::ptr::write_volatile((m+c) as *mut u8,d[i*2]);std::ptr::write_volatile((m+t) as *mut u8,d[i*2+1]);}
  std::ptr::write_volatile((m+offset_of!(CSChrActionFlagModule,weapon_model_location_overridden)) as *mut u8,d[16]);}true}
#[cfg(test)]mod tests{
 use super::*;
 #[test]fn implausible_states_are_refused(){let mut d=[0u8;BYTES];assert!(plausible(&d));d[1]=3;assert!(plausible(&d));d[1]=100;assert!(!plausible(&d));d[1]=0xFF;assert!(plausible(&d));d[16]=2;assert!(!plausible(&d));}
 #[test]fn offsets_are_inside_the_module(){for (c,t) in pair_offsets(){assert!(c<0x258&&t<0x258&&c!=t);}}
}

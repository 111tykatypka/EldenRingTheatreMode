//! Where the player's weapon models sit (hand, sheath, hidden): the part of `CSChrActionFlagModule` that animation
//! events set (TAE event 712 "OverrideWeaponModelLocations"). While a flask is drunk the game moves the weapon
//! away through this state; a replay that only drives bones leaves the live weapon in the hand, so the recorded
//! state is written back each frame. SDK-pinned struct, read as raw bytes, written only when it reads plausible.
use eldenring::cs::{ChrIns,CSChrActionFlagModule};
use std::mem::offset_of;

/// 4 left + 4 right models x (absorb-position condition, change type) + overridden flag = 17 bytes, then the param id
/// (i32 LE) of the active "Hide Weapon" special effect (state info 184, MEASURED in the CheatEngine table's stateInfo list), 0 for none.
pub const MODULE_BYTES:usize=17;
/// Then the item-in-hand state: 1 byte at PlayerIns.chr_asm_model_ins + 0x2D0 (0, or 2 while a consumable is held) and the
/// u32 at CSChrActionFlagModule + 0x48 (0, or 2). MEASURED in the \"flask sip\" recording: both go 0 -> 2 together
/// when the flask is taken into the hand (1.20 s, 0.2 s after the use animation 50110 starts) and back at 3.31 s.
pub const BYTES:usize=26;
pub const ASM_AT:usize=21;
pub const ITEM_AT:usize=22;
const ASM_STATE_OFFSET:usize=0x2D0;
const ITEM_STATE_OFFSET:usize=0x48;
fn asm_object(chr:usize)->Option<usize>{crate::companions::word(chr+offset_of!(eldenring::cs::PlayerIns,chr_asm)+0x10).filter(|p|*p>0x10000)}
pub fn asm_state(d:&[u8;BYTES])->u8{d[ASM_AT]}
pub fn item_state(d:&[u8;BYTES])->u32{u32::from_le_bytes([d[ITEM_AT],d[ITEM_AT+1],d[ITEM_AT+2],d[ITEM_AT+3]])}
macro_rules! pairs{($($c:ident,$t:ident);*)=>{[$((offset_of!(CSChrActionFlagModule,$c),offset_of!(CSChrActionFlagModule,$t))),*]}}
fn pair_offsets()->[(usize,usize);8]{pairs!(lh_model0_absorp_pos_param_condition,lh_model0_change_type;lh_model1_absorp_pos_param_condition,lh_model1_change_type;lh_model2_absorp_pos_param_condition,lh_model2_change_type;lh_model3_absorp_pos_param_condition,lh_model3_change_type;
 rh_model0_absorp_pos_param_condition,rh_model0_change_type;rh_model1_absorp_pos_param_condition,rh_model1_change_type;rh_model2_absorp_pos_param_condition,rh_model2_change_type;rh_model3_absorp_pos_param_condition,rh_model3_change_type)}
pub(crate) fn module(chr:usize)->Option<usize>{
 let c=unsafe{&*(chr as *const ChrIns)};let m=c.modules.action_flag.as_ptr() as usize;(m>0x10000).then_some(m)}
/// Change types the SDK enum knows: -1..=6.
pub fn plausible(d:&[u8;BYTES])->bool{(0..8).all(|i|(-1..=6).contains(&(d[i*2+1] as i8)))&&d[16]<=1&&(0..=10_000_000).contains(&hide_id(d))&&matches!(asm_state(d),0|2)&&matches!(item_state(d),0|2)}
pub fn hide_id(d:&[u8;BYTES])->i32{i32::from_le_bytes([d[17],d[18],d[19],d[20]])}
pub fn read(chr:usize)->Option<[u8;BYTES]>{
 let m=module(chr)?;let mut d=[0u8;BYTES];
 for (i,(c,t)) in pair_offsets().into_iter().enumerate(){
  let mut b=[0u8];if !crate::companions::copy(m+c,&mut b){return None;}d[i*2]=b[0];
  if !crate::companions::copy(m+t,&mut b){return None;}d[i*2+1]=b[0];}
 let mut b=[0u8];if !crate::companions::copy(m+offset_of!(CSChrActionFlagModule,weapon_model_location_overridden),&mut b){return None;}d[16]=b[0];
 d[17..21].copy_from_slice(&crate::item_probe::active_hide(chr).to_le_bytes());
 if let Some(a)=asm_object(chr){let mut b=[0u8];if crate::companions::copy(a+ASM_STATE_OFFSET,&mut b){d[ASM_AT]=b[0];}}
 if let Some(v)=crate::companions::dword(m+ITEM_STATE_OFFSET){d[ITEM_AT..ITEM_AT+4].copy_from_slice(&v.to_le_bytes());}
 plausible(&d).then_some(d)}
pub fn write(chr:usize,d:&[u8;BYTES])->bool{
 if !plausible(d){return false;}let Some(m)=module(chr) else {return false};if read(chr).is_none(){return false;}
 // Hide-weapon effect: make the live player match the recorded one through the game's own special effect.
 crate::item_probe::set_hide(chr,hide_id(d));
 unsafe{for (i,(c,t)) in pair_offsets().into_iter().enumerate(){std::ptr::write_volatile((m+c) as *mut u8,d[i*2]);std::ptr::write_volatile((m+t) as *mut u8,d[i*2+1]);}
  std::ptr::write_volatile((m+offset_of!(CSChrActionFlagModule,weapon_model_location_overridden)) as *mut u8,d[16]);
  // Item in hand (flask): the same two values the game sets itself.
  if let Some(a)=asm_object(chr){let mut b=[0u8];if crate::companions::copy(a+ASM_STATE_OFFSET,&mut b){std::ptr::write_volatile((a+ASM_STATE_OFFSET) as *mut u8,d[ASM_AT]);}}
  if crate::companions::dword(m+ITEM_STATE_OFFSET).is_some(){std::ptr::write_volatile((m+ITEM_STATE_OFFSET) as *mut u32,item_state(d));}}
 true}
#[cfg(test)]mod tests{
 use super::*;
 #[test]fn implausible_states_are_refused(){let mut d=[0u8;BYTES];assert!(plausible(&d));d[17..21].copy_from_slice(&5i32.to_le_bytes());assert!(plausible(&d));d[17..21].copy_from_slice(&(-3i32).to_le_bytes());assert!(!plausible(&d));d[17..21].copy_from_slice(&0i32.to_le_bytes());d[ASM_AT]=2;assert!(plausible(&d));d[ASM_AT]=7;assert!(!plausible(&d));d[ASM_AT]=0;d[ITEM_AT]=9;assert!(!plausible(&d));d[ITEM_AT]=0;d[1]=3;assert!(plausible(&d));d[1]=100;assert!(!plausible(&d));d[1]=0xFF;assert!(plausible(&d));d[16]=2;assert!(!plausible(&d));}
 #[test]fn offsets_are_inside_the_module(){for (c,t) in pair_offsets(){assert!(c<0x258&&t<0x258&&c!=t);}}
}

//! Update-LOD ("omission") override for recorded and replayed characters (STEP A).
//!
//! The game updates far or off-screen characters less often (every 5, 20 or 30 frames). A character that
//! is not updated has an unchanged pose, so a recording of it contains repeated poses (stutter on replay)
//! and a puppet that is only updated sometimes flickers or lags. While Theater records or plays actors:
//! - the global debug budget (WorldChrManDbg) is forced to "Normal" with a very large number of characters
//!   allowed full updates per frame (originals saved and restored exactly), and
//! - each tracked body gets the SDK-documented per-character `force_update` bit every frame.
//! Offsets come from the pinned SDK structs (WorldChrManDbg singleton, ChrInsFlags1c4, OmissionMode);
//! nothing is written unless every value read back is plausible, because the SDK's ChrIns layout was
//! already wrong once for this game version (debug flags). All values are in memory only: nothing is
//! saved to disk, so a crash cannot leave a changed setting behind; the next start has the defaults.
use eldenring::cs::{ChrIns,WorldChrManDbg};
use fromsoftware_shared::FromStatic;
use std::sync::Mutex;

/// Everything changed in WorldChrManDbg, to put back exactly.
#[derive(Clone,Copy,Debug,PartialEq,Eq)]
struct Saved{override_type:i32,near:[i32;3],far:[i32;3]}
static SAVED:Mutex<Option<Saved>>=Mutex::new(None);
/// "All characters": more than the game ever loads at once.
const BUDGET:i32=4096;
const NORMAL:i32=0;

fn dword(a:usize)->Option<i32>{crate::companions::dword(a).map(|v|v as i32)}
fn write_i32(a:usize,v:i32){unsafe{std::ptr::write_volatile(a as *mut i32,v)}}
/// Addresses of the fields we touch: override type, near[3], far[3].
fn fields(dbg:&WorldChrManDbg)->(usize,[usize;3],[usize;3]){
 let o=&dbg.omission_update_num_type_override as *const _ as usize;
 let n=&dbg.omission_update_num_near as *const _ as usize;let f=&dbg.omission_update_num_far as *const _ as usize;
 (o,[n,n+4,n+8],[f,f+4,f+8])}
/// A value an `OmissionUpdateNumType` can have (-1 none, 0 normal, 1 overload, 2 emergency).
pub fn plausible_type(v:i32)->bool{(-1..=2).contains(&v)}
/// A per-frame budget the game could have configured.
pub fn plausible_budget(v:i32)->bool{(0..=100_000).contains(&v)}
/// Values `OmissionMode` can have: NoUpdate -2, Normal 0, 1, 5, 20, 30 frames.
pub fn plausible_mode(v:i32)->bool{matches!(v,-2|0|1|5|20|30)}

pub fn engaged()->bool{SAVED.lock().unwrap().is_some()}
/// Forces full updates for all characters; idempotent. Err = nothing was changed.
pub fn engage()->Result<(),String>{
 let mut saved=SAVED.lock().unwrap();if saved.is_some(){return Ok(());}
 let dbg=unsafe{WorldChrManDbg::instance_mut()}.map_err(|_|"WorldChrManDbg not available".to_string())?;
 let (o,near,far)=fields(dbg);
 let read=|a:usize|dword(a).ok_or_else(||"WorldChrManDbg unreadable".to_string());
 let s=Saved{override_type:read(o)?,near:[read(near[0])?,read(near[1])?,read(near[2])?],far:[read(far[0])?,read(far[1])?,read(far[2])?]};
 if !plausible_type(s.override_type)||!s.near.iter().chain(&s.far).all(|v|plausible_budget(*v)){
  return Err(format!("WorldChrManDbg values are not plausible (override={}, near={:?}, far={:?}); layout mismatch, nothing written",s.override_type,s.near,s.far));}
 write_i32(o,NORMAL);for a in near.into_iter().chain(far){write_i32(a,BUDGET);}
 crate::log_game(&format!("OMISSION: forced full updates for all characters (saved override={} near={:?} far={:?}; budgets set to {BUDGET})",s.override_type,s.near,s.far));
 *saved=Some(s);Ok(())}
/// Puts the original settings back (only if nothing else changed them meanwhile).
pub fn release(){
 let mut saved=SAVED.lock().unwrap();let Some(s)=saved.take() else {return};
 let Ok(dbg)=(unsafe{WorldChrManDbg::instance_mut()}) else {crate::log_game("OMISSION: WorldChrManDbg gone; nothing to restore");return};
 let (o,near,far)=fields(dbg);
 write_i32(o,s.override_type);for (a,v) in near.into_iter().zip(s.near).chain(far.into_iter().zip(s.far)){write_i32(a,v);}
 crate::log_game("OMISSION: original update settings restored");}

/// Current per-character update interval (frames between updates), when it reads as a valid mode.
pub fn mode_of(chr:usize)->Option<i32>{
 let c=chr as *const ChrIns;let a=unsafe{&raw const (*c).omission_mode as usize};
 dword(a).filter(|v|plausible_mode(*v))}
/// Sets the documented `force_update` bit (ChrInsFlags1c4 bit 1; the game clears it every frame) on a body
/// whose omission fields read as valid. Returns the omission mode seen, or None when the layout check fails.
pub fn force_update(chr:usize)->Option<i32>{
 let mode=mode_of(chr)?;
 let c=chr as *const ChrIns;let a=unsafe{&raw const (*c).chr_flags1c4 as usize};
 if !crate::companions::copy(a,&mut [0u8]){return None;}
 unsafe{let v=std::ptr::read_volatile(a as *const u8);std::ptr::write_volatile(a as *mut u8,v|2);}
 Some(mode)}
/// Plain-language level for logs.
pub fn describe(mode:Option<i32>)->&'static str{match mode{Some(0)=>"every frame",Some(1)=>"1",Some(5)=>"every 5th",Some(20)=>"every 20th",Some(30)=>"every 30th",Some(-2)=>"not updated",Some(_)=>"?",None=>"unreadable"}}
/// Histogram of omission modes over a set of bodies, for the recording log.
#[derive(Default,Clone,Copy,Debug,PartialEq,Eq)]
pub struct Histogram{pub every_frame:u32,pub slow:u32,pub not_updated:u32,pub unreadable:u32}
impl Histogram{
 pub fn add(&mut self,mode:Option<i32>){match mode{Some(0)=>self.every_frame+=1,Some(-2)=>self.not_updated+=1,Some(_)=>self.slow+=1,None=>self.unreadable+=1}}
}
#[cfg(test)]mod tests{
 use super::*;
 use crate::game_profile as profile;
 use std::mem::offset_of;
 #[test]fn plausibility_rejects_garbage(){
  for ok in [-1,0,1,2]{assert!(plausible_type(ok));}for bad in [-2,3,0x7FF00000]{assert!(!plausible_type(bad));}
  for ok in [-2,0,1,5,20,30]{assert!(plausible_mode(ok));}for bad in [-1,2,3,4,10,31,1_000_000]{assert!(!plausible_mode(bad));}
  assert!(plausible_budget(0)&&plausible_budget(64)&&!plausible_budget(-1)&&!plausible_budget(1<<20));}
 /// Every offset used here is declared in GameProfile.h and must equal what the pinned SDK declares; a
 /// mismatch fails this test instead of silently reading the wrong field.
 #[test]fn game_profile_offsets_match_the_sdk_layout(){
  assert_eq!(offset_of!(ChrIns,omission_mode),profile::OFF_CHRINS_OMISSION_MODE);
  assert_eq!(offset_of!(ChrIns,chr_flags1c4),profile::OFF_CHRINS_FLAGS_1C4);
  assert_eq!(offset_of!(WorldChrManDbg,omission_update_num_type_override),profile::OFF_WCMDBG_OMISSION_OVERRIDE);
  assert_eq!(offset_of!(WorldChrManDbg,omission_update_num_near),profile::OFF_WCMDBG_OMISSION_NEAR);
  assert_eq!(offset_of!(WorldChrManDbg,omission_update_num_far),profile::OFF_WCMDBG_OMISSION_FAR);}
 #[test]fn histogram_counts_levels(){let mut h=Histogram::default();for m in [Some(0),Some(0),Some(5),Some(30),Some(-2),None]{h.add(m);}assert_eq!(h,Histogram{every_frame:2,slow:2,not_updated:1,unreadable:1});}
 #[test]fn descriptions(){assert_eq!(describe(Some(30)),"every 30th");assert_eq!(describe(None),"unreadable");}
}

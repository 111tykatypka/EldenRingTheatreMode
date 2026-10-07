//! World state for replays (Phase 2.1): in-game clock and event flags.
//!
//! - Time of day: WorldAreaTime.clock (FILETIME + packed date), sampled once a second. Playback
//!   override is disabled pending save-isolation proof; read-only clock data remains available.
//! - Event flags (doors, fog walls, bosses, graces, everything the game tracks): the whole flag memory
//!   is copied once at the start of a recording and compared once a second; changes become events.
//!   Playback can rebuild the flags at any time T (start copy + changes up to T). Writing them changes
//!   save-relevant state. Replay flag writes are disabled until autosave isolation is verified;
//!   the track is read-only even when an older settings file has its flag-override bit set.
//! - Weather: not recorded or restored yet. The weather controller is not described by the pinned SDK;
//!   reported as "not possible yet" until it is researched.
use eldenring::cs::{CSEventFlagMan,WorldAreaTime};
use fromsoftware_shared::FromStatic;

pub const FLAG_BLOCK:usize=125; // bytes per group of 1000 flags (FlagBlock)

/// Clock as raw words (FILETIME, packed date), so it round-trips exactly.
#[derive(Clone,Copy,Debug,Default,PartialEq)]pub struct Clock{pub time64:u64,pub date:u64,pub multiplier:f32}
pub fn read_clock()->Option<Clock>{
 let w=unsafe{WorldAreaTime::instance()}.ok()?;
 let base=w as *const WorldAreaTime as usize;
 Some(Clock{time64:unsafe{std::ptr::read_volatile(base as *const u64)},date:unsafe{std::ptr::read_volatile((base+8) as *const u64)},multiplier:w.time_passage_multiplier})}
/// Sets clock and previous-tick clock to `c` (so the game sees no jump to blend), and the multiplier.
pub fn write_clock(c:&Clock){
 let Ok(w)=(unsafe{WorldAreaTime::instance_mut()}) else {return};
 if !(c.multiplier.is_finite()&&(0.0..=1000.0).contains(&c.multiplier)){return;}
 let base=w as *mut WorldAreaTime as usize;
 unsafe{for o in [0usize,16]{std::ptr::write_volatile((base+o) as *mut u64,c.time64);std::ptr::write_volatile((base+o+8) as *mut u64,c.date);}}
 w.time_passage_multiplier=c.multiplier;}

/// Every flag group currently in memory: (group id, 125 bytes). Group g holds flags g*1000..g*1000+999.
pub fn read_flags()->Vec<(u32,[u8;FLAG_BLOCK])>{
 let Ok(man)=(unsafe{CSEventFlagMan::instance()}) else {return Vec::new()};
 let vm=&man.virtual_memory_flag;let base=vm.flag_blocks as usize;let mut out=Vec::new();
 if base==0{return out;}
 for pair in vm.flag_block_descriptors.iter(){
  // FlagBlockDescriptor: u32 location_mode, then a union at +8 (holder index or block pointer).
  let d=&pair.second as *const _ as usize;
  let mode=unsafe{std::ptr::read_volatile(d as *const u32)};
  let block=match mode{1=>base+unsafe{std::ptr::read_volatile((d+8) as *const u32)} as usize*FLAG_BLOCK,2=>unsafe{std::ptr::read_volatile((d+8) as *const usize)},_=>continue};
  if block<0x10000{continue;}
  out.push((pair.first,unsafe{std::ptr::read_volatile(block as *const [u8;FLAG_BLOCK])}));}
 out}
/// Changed flags between two copies: (flag id, new state).
pub fn diff(old:&[(u32,[u8;FLAG_BLOCK])],new:&[(u32,[u8;FLAG_BLOCK])])->Vec<(u32,bool)>{
 let mut out=Vec::new();let lookup:std::collections::HashMap<u32,&[u8;FLAG_BLOCK]>=old.iter().map(|(g,b)|(*g,b)).collect();
 for (g,b) in new{let prev=lookup.get(g);for i in 0..FLAG_BLOCK{let a=prev.map(|p|p[i]).unwrap_or(0);if a!=b[i]{for bit in 0..8{let m=0x80u8>>bit;if (a^b[i])&m!=0{out.push((g*1000+(i*8+bit) as u32,b[i]&m!=0));}}}}}
 out}
/// Sets one flag (only flags whose group exists in memory).
pub fn write_flag(flag:u32,state:bool){if let Ok(man)=unsafe{CSEventFlagMan::instance_mut()}{man.virtual_memory_flag.set_flag(flag,state);}}
pub fn read_flag(flag:u32)->bool{unsafe{CSEventFlagMan::instance()}.map(|m|m.virtual_memory_flag.get_flag(flag)).unwrap_or(false)}

#[cfg(test)]mod tests{
 use super::*;
 #[test]fn diff_reports_flag_ids(){
  let mut a=[0u8;FLAG_BLOCK];let mut b=a;b[0]=0x80;b[2]=0x01;a[3]=0x40;
  let d=diff(&[(7,a)],&[(7,b)]);assert_eq!(d,vec![(7000,true),(7023,true),(7025,false)]);}
}

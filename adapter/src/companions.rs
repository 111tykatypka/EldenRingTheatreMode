//! Phase 2.3: read-only companion discovery. No spawning or ride-state writes.
//! Public fields come from the locked SDK, not the 2.7.1.0 Cheat Engine table.
use eldenring::cs::{ChrIns, ChrLoadStatus, ChrSetEntry, CSChrRideModule, CSRideNode, WorldChrMan};
use std::ffi::c_void;

pub const BUDDY_SET:u32=1;
pub const RIDDEN_BODY:u32=2;
pub const NPC_SUMMON:u32=4;
pub const WHITE_PHANTOM:u32=8;
pub const RIDE_VALID:u32=1;
pub const MOUNTING:u32=2;
pub const MOUNTED:u32=4;
pub const RIDE_CHARACTER:u32=8;

#[link(name="kernel32")]
unsafe extern "system" {
 fn ReadProcessMemory(process:*mut c_void,base:*const c_void,out:*mut c_void,size:usize,read:*mut usize)->i32;
}

// Copy scalars as bytes: malformed bools/enums are never materialized as Rust values.
fn copy(address:usize,out:&mut [u8])->bool {
 if address<0x10000||address.checked_add(out.len()).is_none(){return false;}
 let mut read=0;
 unsafe{ReadProcessMemory(-1isize as *mut c_void,address as *const c_void,out.as_mut_ptr().cast(),out.len(),&mut read)!=0&&read==out.len()}
}
pub(crate) fn word(address:usize)->Option<usize>{let mut b=[0;8];copy(address,&mut b).then(||usize::from_le_bytes(b))}
fn byte(address:usize)->Option<u8>{let mut b=[0];copy(address,&mut b).then_some(b[0])}
pub(crate) fn dword(address:usize)->Option<u32>{let mut b=[0;4];copy(address,&mut b).then(||u32::from_le_bytes(b))}
fn bit(address:usize)->Option<bool>{match byte(address)?{0=>Some(false),1=>Some(true),_=>None}}

fn readable_body(chr:usize)->bool{
 let c=chr as *const ChrIns;
 let Some(modules)=word(unsafe{&raw const (*c).modules as usize}) else{return false;};if modules<0x10000{return false;}
 let m=modules as *const eldenring::cs::ChrInsModuleContainer;
 let Some(physics)=word(unsafe{&raw const (*m).physics as usize}) else{return false;};if physics<0x10000{return false;}
 let p=physics as *const eldenring::cs::CSChrPhysicsModule;
 word(unsafe{&raw const (*p).owner as usize})==Some(chr)
}

#[derive(Clone,Copy,Debug,Default,PartialEq,Eq)]
pub struct Ride {pub flags:u32,pub state:u32,pub param:i32,pub counter_party:u64}
pub fn ride(chr:usize)->Option<Ride>{
 let c=chr as *const ChrIns;
 let modules=word(unsafe{&raw const (*c).modules as usize})?;
 if modules<0x10000{return None;}
 let module=word(unsafe{&raw const (*(modules as *const eldenring::cs::ChrInsModuleContainer)).ride as usize})?;
 if module<0x10000{return None;}
 let r=module as *const CSChrRideModule;
 if word(unsafe{&raw const (*r).owner as usize})?!=chr{return None;}
 let mounting=bit(unsafe{&raw const (*r).is_mounting as usize})?;
 let mounted=bit(unsafe{&raw const (*r).is_mounted as usize})?;
 let is_ride=bit(unsafe{&raw const (*r).is_ride_character as usize})?;
 let node=word(unsafe{&raw const (*r).ride_node as usize})?;
 if node<0x10000{return None;}
 let n=node as *const CSRideNode;
 if word(unsafe{&raw const (*n).pair_anim_node.owner as usize})?!=chr{return None;}
 let state=dword(unsafe{&raw const (*n).ride_state as usize})?;
 // The SDK documents these lifecycle states. Anything else remains unavailable.
 if !matches!(state,0|3|5|7){return None;}
 let param=dword(unsafe{&raw const (*n).ride_param_id as usize})? as i32;
 let counter_party=word(unsafe{&raw const (*n).pair_anim_node.counter_party as usize})? as u64;
 Some(Ride{flags:RIDE_VALID|if mounting{MOUNTING}else{0}|if mounted{MOUNTED}else{0}|if is_ride{RIDE_CHARACTER}else{0},state,param,counter_party})
}

/// Active buddy bodies may be missing from the distance-sorted list. Read their public ChrSet.
/// The upper scan bound is a corruption/resource guard, centralized in GameProfile.
pub fn buddies(world:&WorldChrMan,diagnostic:bool)->Result<Vec<usize>,&'static str>{
 let set=&raw const world.summon_buddy_chr_set;
 let capacity=dword(unsafe{&raw const (*set).capacity as usize}).ok_or("unreadable capacity")? as usize;
 if capacity>crate::game_profile::VAL_COMPANION_SCAN_GUARD{return Err("capacity exceeds corruption guard");}
 if capacity==0{if diagnostic{crate::log_game("COMPANION_SCAN: capacity=0");}return Ok(Vec::new());}
 let entries=word(unsafe{&raw const (*set).entries as usize}).ok_or("unreadable entries")?;
 let stride=std::mem::size_of::<ChrSetEntry<ChrIns>>();
 let mut bytes=vec![0;capacity.checked_mul(stride).ok_or("capacity overflow")?];
 if !copy(entries,&mut bytes){return Err("unreadable entry array");}
 let pointer=std::mem::offset_of!(ChrSetEntry<ChrIns>,chr_ins);
 let status=std::mem::offset_of!(ChrSetEntry<ChrIns>,chr_load_status);
 let mut statuses=[0usize;256];let mut active=0;let mut rejected=0;
 let bodies=bytes.chunks_exact(stride).filter_map(|e|{
  statuses[e[status] as usize]+=1;
  // The mounted test stayed in ReadyForActivation (4), not Active (2).
  // Include this status for read-only capture discovery; owner and downstream full
  // skeleton checks still apply. Unloading/initializing bodies remain excluded.
  if !capturable_status(e[status]){return None;}active+=1;
  let p=usize::from_le_bytes(e[pointer..pointer+8].try_into().ok()?);
  if p>=0x10000&&p%8==0&&readable_body(p){Some(p)}else{rejected+=1;None}
 }).collect::<Vec<_>>();
 if diagnostic{let histogram:Vec<_>=statuses.iter().enumerate().filter(|(_,n)|**n>0).map(|(s,n)|format!("{s}:{n}")).collect();crate::log_game(&format!("COMPANION_SCAN: capacity={capacity} statuses=[{}] active={active} owner_rejected={rejected} accepted={}",histogram.join(","),bodies.len()));}
 Ok(bodies)
}

fn capturable_status(status:u8)->bool{status==ChrLoadStatus::Active as u8||status==ChrLoadStatus::ReadyForActivation as u8}

pub fn category(chr:usize,in_buddy_set:bool,r:Option<Ride>)->u32{
 let kind=dword(unsafe{&raw const (*(chr as *const ChrIns)).chr_type as usize});
 (if in_buddy_set{BUDDY_SET}else{0})|
 (if r.is_some_and(|r|r.flags&RIDE_CHARACTER!=0){RIDDEN_BODY}else{0})|
 (if kind==Some(eldenring::cs::ChrType::WhiteSummonNpc as u32){NPC_SUMMON}else{0})|
 (if kind==Some(eldenring::cs::ChrType::WhitePhantom as u32){WHITE_PHANTOM}else{0})
}

pub fn mount_compatible(recorded:u32,live:Option<Ride>)->bool{
 // Old files/unsupported observations cannot establish a mount requirement.
 if recorded&RIDE_VALID==0{return true;}
 let Some(live)=live else{return recorded&MOUNTED==0;};
 (recorded&MOUNTED!=0)==(live.flags&MOUNTED!=0)
}

#[cfg(test)]mod tests{
 use super::*;
 #[test]fn discovery_excludes_unloading_and_unknown_status(){assert!(capturable_status(2));assert!(capturable_status(4));for s in [0,1,3,5,255]{assert!(!capturable_status(s));}}
 #[test]fn unreadable_ride_is_unavailable(){assert!(ride(0).is_none());assert!(ride(0x10000).is_none());}
 #[test]fn read_scalars_rejects_bad_bool(){let b=2u8;assert_eq!(bit(&b as *const _ as usize),None);}
 #[test]fn mounted_state_is_never_fabricated(){assert!(!mount_compatible(RIDE_VALID|MOUNTED,None));assert!(!mount_compatible(RIDE_VALID|MOUNTED,Some(Ride{flags:RIDE_VALID,..Default::default()})));assert!(mount_compatible(RIDE_VALID|MOUNTED,Some(Ride{flags:RIDE_VALID|MOUNTED,..Default::default()})));assert!(!mount_compatible(RIDE_VALID,Some(Ride{flags:RIDE_VALID|MOUNTED,..Default::default()})));assert!(mount_compatible(0,None));}
}

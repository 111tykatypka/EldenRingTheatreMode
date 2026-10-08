//! C23: opt-in transient character update priority, not a mesh-LOD or streaming patch.
//! Uses the pinned SDK's force_update bit. Native code clears it each frame.
//! No retained native pointer, omission-mode override, AI flag or persistent patch.
use eldenring::cs::{ChrIns, WorldChrMan};
use fromsoftware_shared::FromStatic;
use std::sync::{OnceLock,atomic::{AtomicBool,AtomicUsize,Ordering}};
unsafe extern "C" {fn tm_camera_quality_requested()->i32;}
static VERIFIED:OnceLock<bool>=OnceLock::new();
static ACTIVE:AtomicBool=AtomicBool::new(false);
static COUNT:AtomicUsize=AtomicUsize::new(0);

fn verified()->bool {
 *VERIFIED.get_or_init(||{
  let base=unsafe{crate::GetModuleHandleW(std::ptr::null())} as usize;
  let mut gate=[0u8;24];let mut mode=[0u8;16];
  let ok=base!=0&&std::mem::offset_of!(ChrIns,chr_flags1c4)==0x1c4
   &&crate::companions::copy(base+crate::game_profile::VAL_CHARACTER_UPDATE_GATE_RVA,&mut gate)
   &&crate::companions::copy(base+crate::game_profile::VAL_CHARACTER_UPDATE_MODE_RVA,&mut mode)
   &&gate==[0xf6,0x87,0xc4,0x01,0x00,0x00,0x02,0x48,0x8b,0xcf,0x74,0x0c,0x33,0xd2,0xe8,0x6b,0x74,0xee,0xff,0xe9,0x7e,0x01,0x00,0x00]
   &&mode==[0xf6,0x81,0xc4,0x01,0x00,0x00,0x01,0x75,0x06,0x89,0x91,0xb4,0x00,0x00,0x00,0xc3];
  crate::log_game(if ok{"CAMERA_QUALITY signature=PASS SDK force_update bit; runtime visual validation required"}else{"CAMERA_QUALITY signature=FAIL; all quality writes disabled"});ok
 })
}

pub fn tick(allowed:bool){
 let requested=allowed&&unsafe{tm_camera_quality_requested()}==1;
 let mut count=0;
 if requested&&verified(){
  // Same known PostPhysics collection as recording. Obtain a new mutable world
  // and fresh entries every callback. Never retain a body across loading/despawn.
  if let Ok(world)=unsafe{WorldChrMan::instance_mut()}{
   if world.main_player.is_some(){
    for entry in world.chr_inses_by_distance.iter_mut(){
     let chr=unsafe{entry.chr_ins.as_mut()};
     if chr.field_ins_handle.is_empty(){continue;}
     // Preserve explicit engine omission locks and disabled/dead bodies.
     if chr.chr_flags1c4.skip_omission_mode_updates()||!chr.chr_flags1c5.enable_render()||chr.chr_flags1c5.death_flag(){continue;}
     chr.chr_flags1c4.set_force_update(true);count+=1;
    }
   }
  }
 }
 let active=count!=0;
 if ACTIVE.swap(active,Ordering::Relaxed)!=active{
  crate::log_game(&format!("CAMERA_QUALITY active={active} requested={requested} loaded_characters={count}; transient requests only; no mesh/streaming override"));
 }
 COUNT.store(count,Ordering::Relaxed);
 // On OFF, focus loss, disconnect, player loss or loading: no writes at all.
 // Do not clear a bit that another native owner might also have requested.
 // The game consumes/clears the final request in its normal update cycle.
}

//! Developer-only, reversible two-second experiments on ONE existing non-player actor.
//! These flags are not verified native AI ownership. No manipulator, health or capsule writes.
use crate::control_protocol::Packet;
use eldenring::cs::{WorldChrMan,FieldInsHandle,FieldInsSelector,BlockId,ChrIns};
use fromsoftware_shared::FromStatic;
use std::sync::{Mutex,atomic::{AtomicU64,Ordering}};
static GENERATION:AtomicU64=AtomicU64::new(0);
static PENDING:Mutex<Option<(Packet,u64)>>=Mutex::new(None);
pub fn stop(){GENERATION.fetch_add(1,Ordering::AcqRel);}
pub fn receive(p:Packet){if !mode_supported(p.flags){crate::log_game("OWNERSHIP_PROBE_REJECTED=MODE_NOT_STATICALLY_VERIFIED noUpdate blocked; no native write");return;}if let Ok(mut q)=PENDING.lock(){*q=Some((p,GENERATION.load(Ordering::Acquire)));}}
fn handle(p:Packet)->FieldInsHandle{FieldInsHandle{selector:FieldInsSelector(p.applied_sequence as u32),block_id:BlockId::from((p.applied_sequence>>32)as i32)}}
fn same(c:&ChrIns,p:Packet,address:usize)->bool{c as *const _ as usize==address&&c.field_ins_handle==handle(p)&&c.event_entity_id==p.detail&&c.npc_param_id==p.replay_detail as i32&&c.chr_type as u32==p.state}
fn mode_supported(mode:u32)->bool{(1..=3).contains(&mode)||mode==5&&std::mem::offset_of!(eldenring::cs::CSChrBehaviorModule,animation_speed)==0x17c8}
fn mask(mode:u32)->u32{match mode{1=>1<<5,2=>1<<4,3=>(1<<4)|(1<<5),4=>1<<8,_=>0}}
#[cfg(test)]fn restore_bits(current:u32,original:u32,owned:u32)->u32{(current&!owned)|(original&owned)}
struct Lease{packet:Packet,generation:u64,address:usize,behavior_address:usize,flags:u32,speed:f32,end:u64}
#[derive(Default)]pub struct Probe{lease:Option<Lease>}
impl Probe{
 pub fn tick(&mut self,now:u64){
  let generation=GENERATION.load(Ordering::Acquire);
  let ready=crate::PROFILE.load(Ordering::Acquire)==crate::STATE_WAITING&&crate::INIT_STATE.load(Ordering::Acquire)==crate::STATE_READY&&crate::PRESENT.load(Ordering::Acquire)!=0;
  let request=PENDING.try_lock().ok().and_then(|mut q|q.take()).filter(|(_,g)|*g==generation);
  let Ok(world)=(unsafe{WorldChrMan::instance_mut()})else{self.lease=None;return;};
  let local=world.main_player.as_ref().map(|p|p.chr_ins.field_ins_handle);
  if let Some(old)=self.lease.as_ref(){
   if let Some(c)=world.chr_ins_by_handle_mut(&handle(old.packet)){if same(c,old.packet,old.address){crate::grounding::actor_event(now,"ownership_probe_observe",Some(c),old.packet);}}

   if !ready||generation!=old.generation||now>=old.end||!crate::replay_runtime::connection_lease(now)||crate::replay_runtime::active(){
    if let Some(c)=world.chr_ins_by_handle_mut(&handle(old.packet)){if same(c,old.packet,old.address)&&&*c.modules.behavior as *const _ as usize==old.behavior_address&&c.modules.behavior.owner.as_ptr()==c as *const _ as *mut _{

     if mask(old.packet.flags)!=0{let _=crate::native_debug_flags::change_owned(c,mask(old.packet.flags),old.flags);}
     if old.packet.flags==5{c.modules.behavior.animation_speed=old.speed;}
     crate::grounding::actor_event(now,"ownership_probe_restored",Some(c),old.packet);
    }}
    self.lease=None;crate::log_game("OWNERSHIP_PROBE=OFF; restore attempted only on exact current object");
   }
  }
  if let Some((p,g))=request{
   if !mode_supported(p.flags)||!ready||self.lease.is_some()||crate::replay_runtime::active()||!crate::replay_runtime::connection_lease(now)||Some(handle(p))==local{return;}
   let Some(c)=world.chr_ins_by_handle_mut(&handle(p))else{crate::grounding::actor_event(now,"ownership_probe_lookup_failed",None,p);return;};
   let address=c as *const _ as usize;
   if !same(c,p,address)||c.modules.behavior.owner.as_ptr()!=c as *const _ as *mut _||!c.modules.behavior.animation_speed.is_finite(){crate::grounding::actor_event(now,"ownership_probe_identity_invalid",Some(c),p);return;}
   if p.flags!=5&&!crate::native_debug_flags::permitted(c){crate::log_game("OWNERSHIP_PROBE_REJECTED=LIVE_LAYOUT_CHECK; +530 callback or ChrCtrl owner mismatch; no write");return;}
   crate::grounding::actor_event(now,"ownership_probe_before",Some(c),p);
   if g!=GENERATION.load(Ordering::Acquire)||!crate::replay_runtime::connection_lease(now){return;}
   self.lease=Some(Lease{packet:p,generation:g,address,behavior_address:&*c.modules.behavior as *const _ as usize,flags:crate::native_debug_flags::read(c),speed:c.modules.behavior.animation_speed,end:now.saturating_add(2_000_000_000)});
   if p.flags==5{c.modules.behavior.animation_speed=0.;}else if !crate::native_debug_flags::change_owned(c,mask(p.flags),mask(p.flags)){self.lease=None;return;}

   crate::grounding::actor_event(now,"ownership_probe_after",Some(c),p);
   crate::log_game("OWNERSHIP_PROBE=ACTIVE; one existing NPC; automatic restore after 2s; native ownership UNVERIFIED");
  }
 }
}
#[cfg(test)]mod tests{use super::*;#[test]fn isolated_modes_and_restore(){assert!(mode_supported(1));assert!(mode_supported(2));assert!(mode_supported(3));assert!(!mode_supported(4));assert!(mode_supported(5));assert_eq!(mask(1),1<<5);assert_eq!(mask(2),1<<4);assert_eq!(mask(3),48);assert_eq!(mask(4),256);assert_eq!(mask(5),0);assert_eq!(restore_bits(0xfeedffff,0x12340000,48),0xfeedffcf);}}

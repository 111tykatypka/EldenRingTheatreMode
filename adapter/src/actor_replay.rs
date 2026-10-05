//! Experimental existing-actor transform playback. Never spawn, resurrect, cache references,
//! replace manipulators or invoke guessed native ABIs. All writes are on game callbacks.
use crate::{control_protocol::Packet,transform_probe::Transform,transform_replay::{Playback,Request,Action}};
use eldenring::{cs::{WorldChrMan,FieldInsHandle,FieldInsSelector,BlockId,ChrIns},rotation::Quaternion};
use fromsoftware_shared::FromStatic;
use std::sync::{Mutex,atomic::{AtomicU64,AtomicUsize,Ordering}};
static BUDGET:AtomicUsize=AtomicUsize::new(1024);
static QUEUE_DROPS:AtomicU64=AtomicU64::new(0);
pub fn configure_budget(n:usize){BUDGET.store(n.clamp(1,16384),Ordering::Release);}
const MASK:u32=(1<<3)|(1<<4)|(1<<5);
const ACTION_MASK:u64=(1u64<<35)-1;
static GENERATION:AtomicU64=AtomicU64::new(1);
static PENDING:Mutex<Vec<(Packet,u64,u64)>>=Mutex::new(Vec::new());
static OWNED:Mutex<Vec<(Packet,usize,u64)>>=Mutex::new(Vec::new());
pub fn stop(){GENERATION.fetch_add(1,Ordering::AcqRel);}
pub fn receive(packet:Packet,now:u64){
    let (phase,_,session,_,_)=crate::replay_runtime::status();
    if !matches!(phase,1|2)||packet.session!=session{return;}
    if let Ok(mut pending)=PENDING.lock(){
        let generation=GENERATION.load(Ordering::Acquire);
        pending.retain(|r|r.2==generation);
        if let Some(r)=pending.iter_mut().find(|r|r.0.applied_sequence==packet.applied_sequence){*r=(packet,now,generation);}
        else if pending.len()<BUDGET.load(Ordering::Acquire){pending.push((packet,now,generation));}
        else{QUEUE_DROPS.fetch_add(1,Ordering::Relaxed);}
    }
}
fn handle(raw:u64)->FieldInsHandle{FieldInsHandle{selector:FieldInsSelector(raw as u32),block_id:BlockId::from((raw>>32) as i32)}}
fn matches(chr:&ChrIns,p:Packet)->bool{chr.field_ins_handle==handle(p.applied_sequence)&&chr.event_entity_id==p.detail&&chr.npc_param_id==p.replay_detail as i32&&chr.chr_type as u32==p.state}
struct Lease { packet:Packet, generation:u64, address:usize, flags:u32, actions:u64, origin:u64, playback:Playback }
pub struct Actors { owned:Vec<Lease>, incoming:Vec<(Packet,u64,u64)>, next_log:u64,applied:u64,rejected:u64,max_correction:f64 }
impl Actors {
 pub fn new()->Self{let budget=BUDGET.load(Ordering::Acquire);if let Ok(mut owned)=OWNED.lock(){owned.reserve(budget);}if let Ok(mut pending)=PENDING.lock(){pending.reserve(budget);}Self{owned:Vec::with_capacity(budget),incoming:Vec::with_capacity(budget),next_log:0,applied:0,rejected:0,max_correction:0.0}}
 fn restore(lease:&Lease,world:&mut WorldChrMan){
  if let Some(chr)=world.chr_ins_by_handle_mut(&handle(lease.packet.applied_sequence)){
   if matches(chr,lease.packet)&&chr as *mut _ as usize==lease.address{
    chr.debug_flags.0=(chr.debug_flags.0&!MASK)|(lease.flags&MASK);
    let bits=&mut chr.modules.action_request.disabled_action_inputs.0;*bits=(*bits&!ACTION_MASK)|(lease.actions&ACTION_MASK);
   }
  }
 }
 pub fn tick(&mut self,now:u64){
  let generation=GENERATION.load(Ordering::Acquire);
  if let Ok(mut pending)=PENDING.try_lock(){std::mem::swap(&mut *pending,&mut self.incoming);}
  let enabled=crate::replay_runtime::input_owned(now)&&crate::INIT_STATE.load(Ordering::Acquire)==crate::STATE_READY;
  let Ok(world)=(unsafe{WorldChrMan::instance_mut()})else{self.incoming.clear();stop();return;};
  let local=world.main_player.as_ref().map(|p|p.chr_ins.field_ins_handle);
  for (packet,received,g) in self.incoming.drain(..){
   if !enabled||g!=generation||Some(handle(packet.applied_sequence))==local{continue;}
   let Some(chr)=world.chr_ins_by_handle_mut(&handle(packet.applied_sequence))else{self.rejected+=1;continue;};
   if !matches(chr,packet){self.rejected+=1;continue;}
   let physics=&chr.modules.physics;
   let live=Transform{position:[physics.position.0,physics.position.1,physics.position.2],quaternion:[physics.orientation.0,physics.orientation.1,physics.orientation.2,physics.orientation.3]};
   if !live.valid(){self.rejected+=1;continue;}
   let target=Transform{position:packet.position,quaternion:packet.quaternion};
   let index=if let Some(index)=self.owned.iter().position(|r|r.packet.applied_sequence==packet.applied_sequence){index}else{
    // Prototype may only acquire matching existing actors near their recorded placement.
    if self.owned.len()>=BUDGET.load(Ordering::Acquire) || crate::transform_probe::distance(live,target)>20.0{self.rejected+=1;continue;}
    crate::log_game(&format!("ACTOR_ACQUIRE session={} handle={:016X} entity={} npc={} start_distance={:.3}; ownership EXPERIMENTAL",packet.session,packet.applied_sequence,packet.detail,packet.replay_detail as i32,crate::transform_probe::distance(live,target)));
    self.owned.push(Lease{packet,generation,address:chr as *mut _ as usize,flags:chr.debug_flags.0,actions:chr.modules.action_request.disabled_action_inputs.0,origin:packet.replay_timestamp_ns,playback:Playback::default()});self.owned.len()-1
   };
   let lease=&mut self.owned[index];
   if lease.address!=chr as *mut _ as usize||lease.generation!=generation{self.rejected+=1;continue;}
   let action=if lease.playback.phase==0{Action::Begin}else{Action::Apply};
   let request=Request{player_action:packet.player_action,animation_enabled:false,action,session:packet.session,sequence:packet.sequence,generation,received_ns:packet.timestamp_ns.min(received),replay_ns:packet.replay_timestamp_ns.saturating_sub(lease.origin),paused:packet.replay_state==2,target};
   if lease.playback.ingest(request,live).is_err(){lease.playback.cancel();self.rejected+=1;}else{lease.packet=packet;}
  }
  let mut i=0;
  while i<self.owned.len(){
   let lease=&mut self.owned[i];
   let frame=if enabled&&lease.generation==generation{lease.playback.frame(generation,now)}else{None};
   let chr=world.chr_ins_by_handle_mut(&handle(lease.packet.applied_sequence));
   let valid=chr.as_ref().is_some_and(|c|matches(c,lease.packet)&&*c as *const _ as usize==lease.address);
   if frame.is_none()||!valid{crate::log_game(&format!("ACTOR_RELEASE handle={:016X} state={} detail={} identity_match={valid}; owned flags restored only on exact current object",lease.packet.applied_sequence,lease.playback.phase,lease.playback.detail));Self::restore(lease,world);self.owned.swap_remove(i);continue;}
   let chr=chr.unwrap();let r=frame.unwrap();
   if generation!=GENERATION.load(Ordering::Acquire){break;}
   let p=&chr.modules.physics;
   let actual=Transform{position:[p.position.0,p.position.1,p.position.2],quaternion:[p.orientation.0,p.orientation.1,p.orientation.2,p.orientation.3]};
   if !actual.valid(){crate::replay_runtime::stop(4);break;}
   chr.debug_flags.0|=MASK;chr.modules.action_request.disabled_action_inputs.0|=ACTION_MASK;
   self.max_correction=self.max_correction.max(crate::transform_probe::distance(actual,r.target));
   let p=&mut chr.modules.physics;
   p.position.0=r.target.position[0];p.position.1=r.target.position[1];p.position.2=r.target.position[2];
   p.orientation=Quaternion(r.target.quaternion[0],r.target.quaternion[1],r.target.quaternion[2],r.target.quaternion[3]);
   self.applied+=1;i+=1;
  }
  if now>=self.next_log && (!self.owned.is_empty()||self.applied!=0||self.rejected!=0){
   crate::log_game(&format!("ACTOR_REPLAY experimental active={} applied_callbacks={} rejected={} actor_budget={} queue_drops={} max_prewrite_correction={:.4}; normalized movement/actions masked, AI ownership UNVERIFIED; no spawn/health/inventory/animation writes",self.owned.len(),self.applied,self.rejected,BUDGET.load(Ordering::Acquire),QUEUE_DROPS.load(Ordering::Relaxed),self.max_correction));self.next_log=now+1_000_000_000;self.applied=0;self.rejected=0;self.max_correction=0.0;
  }
  if let Ok(mut owned)=OWNED.try_lock(){owned.clear();for r in &self.owned{owned.push((r.packet,r.address,r.generation));}}
 }
}
pub fn early_tick(now:u64){
 if !crate::replay_runtime::input_owned(now){return;}
 let Ok(owned)=OWNED.try_lock()else{return;};if owned.is_empty(){return;}
 let Ok(world)=(unsafe{WorldChrMan::instance_mut()})else{return;};
 let generation=GENERATION.load(Ordering::Acquire);
 for (packet,address,g) in owned.iter(){
  if *g!=generation||now<packet.timestamp_ns||now-packet.timestamp_ns>250_000_000{continue;}
  if let Some(chr)=world.chr_ins_by_handle_mut(&handle(packet.applied_sequence)){
   if matches(chr,*packet)&&chr as *mut _ as usize==*address{
    if !crate::local_input::neutralize(chr){crate::replay_runtime::stop(6);return;}
   }
  }
 }
}

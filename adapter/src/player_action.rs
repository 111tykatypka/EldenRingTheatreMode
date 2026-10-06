//! Observed pinned TAE state; semantic IDs are never guessed from velocity.
use eldenring::cs::{PlayerIns,FieldInsHandle};
#[repr(C)]#[derive(Clone,Copy,Debug,PartialEq)]
pub struct State {pub action:u32,pub flags:u32,pub raw_action_bits:u64,pub animation_id:i32,pub animation_time:f32,pub animation_length:f32,pub playback_rate:f32}
impl Default for State {fn default()->Self{Self{action:0,flags:0,raw_action_bits:0,animation_id:-1,animation_time:0.0,animation_length:0.0,playback_rate:0.0}}}
const _:()=assert!(std::mem::size_of::<State>()==32);
impl State {
 pub fn valid(self)->bool {self.action<=10&&self.flags<=15&&((self.flags&1!=0&&self.animation_id>=0)||(self.flags&1==0&&self.animation_id==-1))&&[self.animation_time,self.animation_length,self.playback_rate].iter().all(|v|v.is_finite())&&(self.flags&2==0||(self.flags&1!=0&&self.animation_time>=0.0&&self.animation_length>0.0&&self.animation_time<=self.animation_length+1.0))&&(self.flags&4==0||(0.0..=10.0).contains(&self.playback_rate))}
 pub fn encode(self)->[u8;32]{let mut b=[0;32];b[0..4].copy_from_slice(&self.action.to_le_bytes());b[4..8].copy_from_slice(&self.flags.to_le_bytes());b[8..16].copy_from_slice(&self.raw_action_bits.to_le_bytes());b[16..20].copy_from_slice(&self.animation_id.to_le_bytes());for(i,v)in[self.animation_time,self.animation_length,self.playback_rate].iter().enumerate(){b[20+i*4..24+i*4].copy_from_slice(&v.to_le_bytes());}b}
 pub fn decode(b:&[u8])->Self{Self{action:u32::from_le_bytes(b[0..4].try_into().unwrap()),flags:u32::from_le_bytes(b[4..8].try_into().unwrap()),raw_action_bits:u64::from_le_bytes(b[8..16].try_into().unwrap()),animation_id:i32::from_le_bytes(b[16..20].try_into().unwrap()),animation_time:f32::from_le_bytes(b[20..24].try_into().unwrap()),animation_length:f32::from_le_bytes(b[24..28].try_into().unwrap()),playback_rate:f32::from_le_bytes(b[28..32].try_into().unwrap())}}
}
pub fn observe(player:&PlayerIns)->State {observe_chr(&player.chr_ins)}
pub fn observe_chr(chr:&eldenring::cs::ChrIns)->State {
 let modules=&chr.modules;let mut s=State{raw_action_bits:modules.action_request.action_requests.0,..Default::default()};
 let tae=&modules.time_act;if let Some(anim)=tae.anim_queue.get(tae.read_idx as usize){
  if anim.anim_id>=0{s.animation_id=anim.anim_id;s.flags|=1;
   if anim.play_time.is_finite()&&anim.anim_length.is_finite()&&anim.play_time>=0.0&&anim.anim_length>0.0&&anim.play_time<=anim.anim_length+1.0{s.animation_time=anim.play_time;s.animation_length=anim.anim_length;s.flags|=2;}
   if anim.anim_id==modules.event.idle_anim_id{s.action=1;s.flags|=8;}
  }
 }
 let rate=modules.behavior.animation_speed;if rate.is_finite()&&(0.0..=10.0).contains(&rate){s.playback_rate=rate;s.flags|=4;}s
}
/// Pure recorded-cycle detector. Repeated IDs are not necessarily the same action.
#[derive(Default)]pub struct AnimationCursor{last:Option<State>}
impl AnimationCursor{
 pub fn request(&mut self,state:State)->bool{
  if !state.valid()||state.flags&1==0{self.last=None;return false;}
  let request=self.last.is_none_or(|old|old.animation_id!=state.animation_id||
   (old.flags&2!=0&&state.flags&2!=0&&state.animation_time+0.0001<old.animation_time));
  self.last=Some(state);request
 }
}
/// Experimental public next-frame request. TAE queue/time and pose are NEVER overwritten.
#[derive(Default)]pub struct AnimationLease{
 saved:Option<(FieldInsHandle,i32,i32)>,cursor:AnimationCursor,last_log_ns:u64,requests:u64,
}
impl AnimationLease {
 pub fn apply(&mut self,player:&mut PlayerIns,state:State,enabled:bool,session:u64,replay_ns:u64)->bool{
  if !enabled||state.flags&1==0{self.restore_player(player);return true;}
  if !state.valid(){return false;}
  let owner=std::ptr::from_ref(&player.chr_ins);
  if player.chr_ins.modules.event.owner.as_ptr().cast_const()!=owner||player.chr_ins.modules.time_act.owner.as_ptr().cast_const()!=owner{return false;}
  // Diagnose the previous native update BEFORE issuing this callback's request.
  let now=crate::monotonic_ns();
  if self.saved.is_some()&&now.saturating_sub(self.last_log_ns)>=1_000_000_000 {
   let actual=observe(player);
   crate::log_game(&format!("REPLAY_ANIMATION_COMPARE session={} replay_ns={} requested_id={} requested_time={} observed_id={} observed_time={} id_match={} phase_error={:?} requests={} mode=EXPERIMENTAL; phase NOT forced",
    session,replay_ns,state.animation_id,state.animation_time,actual.animation_id,actual.animation_time,state.animation_id==actual.animation_id,
    if state.flags&2!=0&&actual.flags&2!=0{Some(actual.animation_time-state.animation_time)}else{None},self.requests));
   self.last_log_ns=now;
  }
  if !self.cursor.request(state){return true;}
  if let Some((handle,original,_))=self.saved{if handle!=player.chr_ins.field_ins_handle{return false;}self.saved=Some((handle,original,state.animation_id));}
  else{self.saved=Some((player.chr_ins.field_ins_handle,player.chr_ins.modules.event.request_animation_id,state.animation_id));}
  player.chr_ins.modules.event.request_animation_id=state.animation_id;self.requests+=1;
  crate::log_game(&format!("REPLAY_ANIMATION_REQUEST session={} replay_ns={} id={} recorded_time={} recorded_length={} raw_requests=0x{:X} request_count={} mode=EXPERIMENTAL ID_CHANGE_OR_PHASE_RESET; phase/time/rate NOT forced",
   session,replay_ns,state.animation_id,state.animation_time,state.animation_length,state.raw_action_bits,self.requests));true
 }
 pub fn restore_player(&mut self,player:&mut PlayerIns){
  if let Some((handle,original,owned))=self.saved.take(){
   if handle==player.chr_ins.field_ins_handle&&player.chr_ins.modules.event.owner.as_ptr().cast_const()==std::ptr::from_ref(&player.chr_ins)&&player.chr_ins.modules.event.request_animation_id==owned{player.chr_ins.modules.event.request_animation_id=original;}
   crate::log_game("REPLAY_ANIMATION_OFF; owned pending request restored if still owned; native animation already accepted is not rewound");
  }
  self.cursor=AnimationCursor::default();self.requests=0;self.last_log_ns=0;
 }
 pub fn restore(&mut self){if self.saved.is_some(){if let Ok(player)=unsafe{PlayerIns::local_player_mut()}{self.restore_player(player);}}else{self.cursor=AnimationCursor::default();}}
 pub fn discard(&mut self){*self=Self::default();}
}
#[cfg(test)]mod tests{use super::*;#[test]fn action_layout_validation(){let s=State{flags:3,animation_id:100,animation_time:0.5,animation_length:1.0,..Default::default()};assert!(s.valid());assert_eq!(State::decode(&s.encode()),s);assert!(!State{action:99,..s}.valid());assert!(!State{animation_time:f32::NAN,..s}.valid());assert!(!State{animation_id:-1,..s}.valid());assert!(!State{animation_time:3.0,..s}.valid());}}

#[cfg(test)]mod cycle_tests{
 use super::*;
 fn sample(time:f32)->State{State{flags:3,animation_id:12345,animation_time:time,animation_length:1.0,..Default::default()}}
 #[test]fn same_id_new_cycle_is_requested_once(){let mut c=AnimationCursor::default();assert!(c.request(sample(0.0)));assert!(!c.request(sample(0.2)));assert!(!c.request(sample(0.2)));assert!(c.request(sample(0.0)));assert!(!c.request(sample(0.1)));}
 #[test]fn invalid_or_unavailable_never_requests(){let mut c=AnimationCursor::default();assert!(!c.request(State::default()));assert!(!c.request(sample(f32::NAN)));assert!(c.request(sample(0.0)));assert!(!c.request(sample(0.00001)));assert!(!c.request(sample(0.0)));assert!(c.request(State{animation_id:54321,..sample(0.0)}));}
}

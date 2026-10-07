//! Observed pinned TAE state; semantic IDs are never guessed from velocity.
use eldenring::cs::PlayerIns;
#[repr(C)]#[derive(Clone,Copy,Debug,PartialEq)]
pub struct State {pub action:u32,pub flags:u32,pub raw_action_bits:u64,pub animation_id:i32,pub animation_time:f32,pub animation_length:f32,pub playback_rate:f32}
impl Default for State {fn default()->Self{Self{action:0,flags:0,raw_action_bits:0,animation_id:-1,animation_time:0.0,animation_length:0.0,playback_rate:0.0}}}
const _:()=assert!(std::mem::size_of::<State>()==32);
impl State {
 #[cfg_attr(not(test),allow(dead_code))]
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
#[cfg(test)]mod tests{use super::*;#[test]fn action_layout_validation(){let s=State{flags:3,animation_id:100,animation_time:0.5,animation_length:1.0,..Default::default()};assert!(s.valid());assert_eq!(State::decode(&s.encode()),s);assert!(!State{action:99,..s}.valid());assert!(!State{animation_time:f32::NAN,..s}.valid());assert!(!State{animation_id:-1,..s}.valid());assert!(!State{animation_time:3.0,..s}.valid());}}

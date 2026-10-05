//! Read-only native observations. No retained game references and no actor writes.
use eldenring::cs::WorldChrMan;
use fromsoftware_shared::FromStatic;
use std::sync::{Mutex,OnceLock,atomic::{AtomicBool,AtomicU64,Ordering}};
use crate::player_action;
#[derive(Clone,Copy,Default)]
struct Observation { id:u64,handle:u64,entity:u32,npc:i32,block:i32,kind:u32,position:[f32;3],rotation:[f32;4],action:player_action::State }
#[derive(Clone)]struct Frame{seq:u64,time:u64,flags:u16,rows:Vec<Observation>,visuals:Vec<crate::visual_capture::Visual>}
static FRAME:OnceLock<Mutex<Frame>>=OnceLock::new();
static CONNECTED:AtomicBool=AtomicBool::new(false);
static DROPPED:AtomicU64=AtomicU64::new(0);
#[derive(Clone,Copy)]struct Identity {handle:u64,address:usize,id:u64,seen:bool,last_seen:u64,entity:u32,npc:i32}
pub struct Capture { radius:f32,interval:u64,budget:usize,next:u64,sequence:u64,next_id:u64,identities:Vec<Identity> }
impl Capture {
 pub fn new()->Self{
  let (mut radius,mut hz,mut budget)=(200f32,60f32,1024usize);
  if let Some(base)=std::env::var_os("LOCALAPPDATA") {if let Ok(text)=std::fs::read_to_string(std::path::PathBuf::from(base).join("EldenRingTheaterMode/Modern.capture.ini")){for line in text.lines(){if let Some((k,v))=line.split_once('='){match k.trim(){"radius"=>{if let Ok(n)=v.trim().parse::<f32>(){if n.is_finite()&&n>0.0{radius=n;}}},"hz"=>{if let Ok(n)=v.trim().parse::<f32>(){if n.is_finite()&&(1.0..=120.0).contains(&n){hz=n;}}},"budget"=>{if let Ok(n)=v.trim().parse::<usize>(){if (1..=16384).contains(&n){budget=n;}}},_=>{}}}}}}
  crate::actor_replay::configure_budget(budget);
  let _=FRAME.set(Mutex::new(Frame{seq:0,time:0,flags:0,rows:Vec::with_capacity(budget),visuals:Vec::with_capacity(budget+1)}));
  crate::log_game(&format!("CHARACTER_CAPTURE configured radius={radius} requested_hz={hz} resource_budget={budget}; read-only EXPERIMENTAL"));
  Self{radius,interval:(1e9/hz as f64)as u64,budget,next:0,sequence:0,next_id:1,identities:Vec::with_capacity(budget)}
 }
 pub fn tick(&mut self,now:u64){
  if !CONNECTED.load(Ordering::Relaxed)||now<self.next{return;}self.next=now+self.interval;
  let Some(slot)=FRAME.get()else{return;};let Ok(mut frame)=slot.try_lock()else{DROPPED.fetch_add(1,Ordering::Relaxed);return;};
  self.sequence+=1;frame.seq=self.sequence;frame.time=now;frame.flags=0;frame.rows.clear();frame.visuals.clear();for v in &mut self.identities{v.seen=false;}
  let Ok(world)=(unsafe{WorldChrMan::instance()})else{frame.flags=4;self.identities.clear();return;};
  let Some(player)=world.main_player.as_ref()else{frame.flags=4;self.identities.clear();return;};
  frame.visuals.push(crate::visual_capture::player(now,player));
  let origin=&player.chr_ins.modules.physics.position;
  let entries=&world.chr_inses_by_distance;
  if entries.len()>65536{frame.flags=2;DROPPED.fetch_add(1,Ordering::Relaxed);return;}
  for entry in entries.iter(){
   // Collection and references are used only in this game's PostPhysics callback.
   let chr=unsafe{entry.chr_ins.as_ref()};if chr.field_ins_handle==player.chr_ins.field_ins_handle||chr.field_ins_handle.is_empty(){continue;}
   let physics=&chr.modules.physics;let p=[physics.position.0,physics.position.1,physics.position.2];let q=[physics.orientation.0,physics.orientation.1,physics.orientation.2,physics.orientation.3];
   let delta=[p[0]-origin.0,p[1]-origin.1,p[2]-origin.2];let distance=delta.iter().map(|v|v*v).sum::<f32>();
   let handle=chr.field_ins_handle.selector.0 as u64|((i32::from(chr.field_ins_handle.block_id)as u32 as u64)<<32);let address=entry.chr_ins.as_ptr()as usize;
   let existing=self.identities.iter().position(|v|v.handle==handle&&v.entity==chr.event_entity_id&&v.npc==chr.npc_param_id&&(v.address==address||v.entity!=0));
   let range=if existing.is_some(){self.radius*1.2}else{self.radius};
   let norm=q.iter().map(|v|v*v).sum::<f32>();if !distance.is_finite()||distance>range*range||!p.iter().chain(q.iter()).all(|v|v.is_finite())||!(0.25..=2.25).contains(&norm){continue;}
   let index=if let Some(i)=existing{i}else{
    if self.identities.len()>=self.budget{frame.flags|=2;continue;}
    let i=self.identities.len();self.identities.push(Identity{handle,address,id:self.next_id,seen:false,last_seen:now,entity:chr.event_entity_id,npc:chr.npc_param_id});self.next_id+=1;i
   };
   if self.identities[index].seen{continue;}self.identities[index].seen=true;self.identities[index].last_seen=now;self.identities[index].address=address;
   if frame.rows.len()>=self.budget{frame.flags|=2;continue;}
   let mut action=player_action::observe_chr(chr);action.action=0;action.flags&=!8;
   frame.rows.push(Observation{id:self.identities[index].id,handle,entity:chr.event_entity_id,npc:chr.npc_param_id,block:i32::from(chr.block_id),kind:chr.chr_type as u32,position:p,rotation:q,action});
   frame.visuals.push(crate::visual_capture::actor(self.identities[index].id,now,chr));
  }
  // Re-entry becomes a NEW observational lifetime. No claim of engine spawn/despawn.
  self.identities.retain(|v|v.seen||now.saturating_sub(v.last_seen)<2_000_000_000);
 }
}
fn encode_row(o:Observation,t:u64)->Vec<u8>{
 let mut b=Vec::with_capacity(112);b.extend(1u16.to_le_bytes());b.extend(2u16.to_le_bytes());b.extend(0u32.to_le_bytes());b.extend(o.id.to_le_bytes());b.extend(t.to_le_bytes());b.extend(o.handle.to_le_bytes());b.extend(o.entity.to_le_bytes());b.extend(o.npc.to_le_bytes());b.extend(o.block.to_le_bytes());b.extend(o.kind.to_le_bytes());for f in o.position.into_iter().chain(o.rotation){b.extend(f.to_le_bytes());}b.extend(o.action.encode());b.extend(0u32.to_le_bytes());b
}
pub fn worker(){
 use std::{ffi::c_void,os::windows::ffi::OsStrExt,time::Duration};type Handle=*mut c_void;
 #[link(name="kernel32")]unsafe extern "system"{fn CreateFileW(n:*const u16,a:u32,s:u32,sa:*mut c_void,c:u32,f:u32,t:Handle)->Handle;fn WriteFile(h:Handle,b:*const c_void,n:u32,w:*mut u32,o:*mut c_void)->i32;fn CloseHandle(h:Handle)->i32;}
 let name=std::ffi::OsStr::new(r"\\.\pipe\EldenRingTheaterMode_1_17_Characters").encode_wide().chain(Some(0)).collect::<Vec<_>>();
 loop{let h=unsafe{CreateFileW(name.as_ptr(),0x40000000,0,std::ptr::null_mut(),3,0,std::ptr::null_mut())};if h==(-1isize as Handle){std::thread::sleep(Duration::from_millis(500));continue;}CONNECTED.store(true,Ordering::Release);let mut last=0;
  loop{let frame=FRAME.get().and_then(|f|f.lock().ok().filter(|f|f.seq!=last).map(|f|f.clone()));if let Some(f)=frame{last=f.seq;let mut b=Vec::with_capacity(40+112*f.rows.len());b.extend(0x54524843u32.to_le_bytes());b.extend(2u16.to_le_bytes());b.extend(f.flags.to_le_bytes());b.extend((f.rows.len()as u32).to_le_bytes());b.extend((f.visuals.len()as u32).to_le_bytes());b.extend(f.seq.to_le_bytes());b.extend(f.time.to_le_bytes());b.extend(DROPPED.load(Ordering::Relaxed).to_le_bytes());for o in f.rows{b.extend(encode_row(o,f.time));}for v in f.visuals{b.extend(v.encode());}let mut n=0;if unsafe{WriteFile(h,b.as_ptr().cast(),b.len()as u32,&mut n,std::ptr::null_mut())}==0||n as usize!=b.len(){break;}}std::thread::sleep(Duration::from_millis(5));}
  CONNECTED.store(false,Ordering::Release);unsafe{CloseHandle(h)};
 }
}
#[cfg(test)]mod tests{use super::*;#[test]fn observation_wire_has_no_pointer(){let b=encode_row(Observation{id:42,handle:123,..Default::default()},99);assert_eq!(b.len(),112);assert_eq!(u64::from_le_bytes(b[8..16].try_into().unwrap()),42);assert_eq!(u64::from_le_bytes(b[16..24].try_into().unwrap()),99);}}

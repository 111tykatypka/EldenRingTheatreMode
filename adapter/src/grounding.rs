//! Explicit ten-second differential trace. Callbacks copy POD; a worker owns formatting/I/O.
//! No guessed vertical correction, collision, gravity, proxy or model writes.
use eldenring::cs::{ChrIns,PlayerIns};
use std::sync::{OnceLock,mpsc::{sync_channel,SyncSender},atomic::{AtomicU64,Ordering}};
use crate::transform_probe::Transform;
const WINDOW_NS:u64=10_000_000_000;
static DEADLINE:AtomicU64=AtomicU64::new(0);
static RUN:AtomicU64=AtomicU64::new(0);
static DROPPED:AtomicU64=AtomicU64::new(0);
static SENDER:OnceLock<SyncSender<Row>>=OnceLock::new();
#[derive(Clone,Copy,Debug,Default)]struct Row {
 mode:u32,run:u64,now:u64,stage:&'static str,handle:u64,session:u64,replay_ns:u64,sequence:u64,requested_handle:u64,requested_entity:u32,requested_npc:i32,requested_type:u32,
 lua_present:Option<bool>,lua_warp:Option<i32>,lua_reentry:Option<bool>,lua_load:Option<bool>,position:[f32;3],orientation:[f32;4],last:[f32;3],physics_model:[f32;3],model:[f32;3],
 entity:u32,npc:i32,chr_type:u32,has_transform:bool,animation_speed:f32,root_motion:[f32;4],fall_timer:f32,proxy_request:bool,vertical_offset:f32,ground:bool,touching:bool,falling:bool,proxy:u32,debug_flags:u32,action_bits:u64,block:i32,target:Option<Transform>,
}
pub fn start(now:u64){RUN.fetch_add(1,Ordering::AcqRel);DROPPED.store(0,Ordering::Release);DEADLINE.store(now.saturating_add(WINDOW_NS),Ordering::Release);}
pub fn stop(){DEADLINE.store(0,Ordering::Release);}
pub fn active(now:u64)->bool{let end=DEADLINE.load(Ordering::Acquire);end!=0&&now<end}
pub fn initialize(){
 let(tx,rx)=sync_channel::<Row>(8192);if SENDER.set(tx).is_err(){return;}
 let _=std::thread::Builder::new().name("TheaterMode.RuntimeTrace".into()).spawn(move||{
  use std::{fs::OpenOptions,io::{BufWriter,Write},time::Duration};
  let path=std::env::temp_dir().join("TheaterModeRuntimeTrace.jsonl");
  let Ok(file)=OpenOptions::new().create(true).append(true).open(path)else{stop();crate::log_game("RUNTIME_TRACE_ERROR=OPEN_FAILED");return;};
  let mut file=BufWriter::new(file);let mut flushed=0;
  loop{let r=match rx.recv_timeout(Duration::from_millis(500)){Ok(r)=>r,Err(std::sync::mpsc::RecvTimeoutError::Timeout)=>{let _=file.flush();continue;},Err(_)=>break};
   let json=render(r);
   if writeln!(file,"{json}").is_err(){stop();crate::log_game("RUNTIME_TRACE_ERROR=WRITE_FAILED");break;}
   if r.now.saturating_sub(flushed)>=500_000_000{let _=file.flush();flushed=r.now;}
  }
 });
}
fn render(r:Row)->String{
   let target=r.target.map(|t|format!("{{\"position\":{:?},\"orientation\":{:?}}}",t.position,t.quaternion)).unwrap_or("null".into());
   let nullable=|v:Option<bool>|v.map(|b|b.to_string()).unwrap_or("null".into());
   let warp=r.lua_warp.map(|v|v.to_string()).unwrap_or("null".into());
   let json=format!("{{\"schema\":1,\"run\":{},\"time_ns\":{},\"stage\":\"{}\",\"handle\":\"{:016X}\",\"session\":{},\"replay_ns\":{},\"sequence\":{},\"requested_handle\":\"{:016X}\",\"requested_entity\":{},\"requested_npc\":{},\"requested_type\":{},\"position\":{:?},\"orientation\":{:?},\"last_update_position\":{:?},\"physics_model_position\":{:?},\"model_position\":{:?},\"has_transform\":{},\"entity\":{},\"npc\":{},\"chr_type\":{},\"animation_speed\":{},\"behavior_root_motion\":{:?},\"fall_timer\":{},\"proxy_update_requested\":{},\"vertical_offset\":{},\"solid_ground\":{},\"touching_ground\":{},\"falling\":{},\"proxy_flags\":{},\"debug_flags\":{},\"debug_flags_offset\":\"0x538\",\"debug_flags_verified\":false,\"action_bits\":{},\"block\":{},\"target\":{},\"queue_drops\":{},\"lua_event_man_present\":{},\"lua_warp_bonfire_id\":{},\"lua_wait_reentry\":{},\"lua_load_wait\":{},\"proxy_position\":null,\"ground_height\":null}}",r.run,r.now,r.stage,r.handle,r.session,r.replay_ns,r.sequence,r.requested_handle,r.requested_entity,r.requested_npc,r.requested_type,r.position,r.orientation,r.last,r.physics_model,r.model,r.has_transform,r.entity,r.npc,r.chr_type,r.animation_speed,r.root_motion,r.fall_timer,r.proxy_request,r.vertical_offset,r.ground,r.touching,r.falling,r.proxy,r.debug_flags,r.action_bits,r.block,target,DROPPED.load(Ordering::Relaxed),nullable(r.lua_present),warp,nullable(r.lua_reentry),nullable(r.lua_load));
   let json=json.replacen("{",&format!("{{\"transform_mode\":{},",r.mode),1);
   let json=json.replace("-inf","null").replace("inf","null").replace("NaN","null");
   json
}
fn snapshot(now:u64,stage:&'static str,chr:&ChrIns,target:Option<Transform>)->Row{
 let p=&chr.modules.physics;let c=&chr.chr_ctrl;let (_,_,session,replay_ns,sequence)=crate::replay_runtime::status();
 let row=Row{mode:if crate::replay_runtime::xz_only(){4}else{0},run:RUN.load(Ordering::Acquire),now,stage,handle:chr.field_ins_handle.selector.0 as u64|((i32::from(chr.field_ins_handle.block_id)as u32 as u64)<<32),session,replay_ns,sequence,requested_handle:0,requested_entity:0,requested_npc:0,requested_type:0,lua_present:None,lua_warp:None,lua_reentry:None,lua_load:None,
 position:[p.position.0,p.position.1,p.position.2],orientation:[p.orientation.0,p.orientation.1,p.orientation.2,p.orientation.3],last:[p.last_update_position.0,p.last_update_position.1,p.last_update_position.2],
 physics_model:[c.physics_model_matrix.3.0,c.physics_model_matrix.3.1,c.physics_model_matrix.3.2],model:[c.model_matrix.3.0,c.model_matrix.3.1,c.model_matrix.3.2],entity:chr.event_entity_id,npc:chr.npc_param_id,chr_type:chr.chr_type as u32,has_transform:true,animation_speed:chr.modules.behavior.animation_speed,root_motion:[chr.modules.behavior.root_motion.0,chr.modules.behavior.root_motion.1,chr.modules.behavior.root_motion.2,chr.modules.behavior.root_motion.3],fall_timer:chr.modules.fall.fall_timer,proxy_request:p.chr_proxy_pos_update_requested,vertical_offset:c.vertical_position_offset,ground:p.standing_on_solid_ground,touching:p.touching_solid_ground,falling:p.is_falling,proxy:c.chr_proxy_flags.0,debug_flags:crate::native_debug_flags::read(chr),action_bits:chr.modules.action_request.disabled_action_inputs.0,block:i32::from(chr.block_id),target};
 row
}
fn enqueue(row:Row){if let Some(tx)=SENDER.get(){if tx.try_send(row).is_err(){DROPPED.fetch_add(1,Ordering::Relaxed);}}}
pub fn capture(now:u64,stage:&'static str,chr:&ChrIns,target:Option<Transform>){if active(now){enqueue(snapshot(now,stage,chr,target));}}
pub fn request(now:u64,stage:&'static str,chr:&ChrIns,r:crate::transform_replay::Request){if active(now){let mut row=snapshot(now,stage,chr,Some(r.target));row.sequence=r.sequence;row.replay_ns=r.replay_ns;row.session=r.session;enqueue(row);}}
pub fn actor_event(now:u64,stage:&'static str,chr:Option<&ChrIns>,packet:crate::control_protocol::Packet){
 if !active(now){return;}
 let target=Some(Transform{position:packet.position,quaternion:packet.quaternion});
 if let Some(chr)=chr{let mut row=snapshot(now,stage,chr,target);row.sequence=packet.sequence;row.replay_ns=packet.replay_timestamp_ns;row.session=packet.session;row.requested_handle=packet.applied_sequence;row.requested_entity=packet.detail;row.requested_npc=packet.replay_detail as i32;row.requested_type=packet.state;enqueue(row);}
 else {let row=Row{mode:if crate::replay_runtime::xz_only(){4}else{0},run:RUN.load(Ordering::Acquire),now,stage,handle:packet.applied_sequence,session:packet.session,replay_ns:packet.replay_timestamp_ns,sequence:packet.sequence,requested_handle:packet.applied_sequence,requested_entity:packet.detail,requested_npc:packet.replay_detail as i32,requested_type:packet.state,target,..Default::default()};enqueue(row);}
}
pub fn world(now:u64,present:bool,warp:Option<i32>,reentry:Option<bool>,load:Option<bool>){if active(now){enqueue(Row{run:RUN.load(Ordering::Acquire),now,stage:"world_lua_readonly",lua_present:Some(present),lua_warp:warp,lua_reentry:reentry,lua_load:load,..Default::default()});}}
pub fn player(now:u64,stage:&'static str){if active(now){if let Ok(p)=unsafe{PlayerIns::local_player()}{capture(now,stage,&p.chr_ins,None);}}}
#[cfg(test)]mod tests{use super::*;#[test]fn nonfinite_json_is_explicit_null(){let text=render(Row{position:[f32::NAN,f32::INFINITY,f32::NEG_INFINITY],..Default::default()});assert!(text.contains("\"position\":[null, null, null]"));assert!(text.contains("\"has_transform\":false"));assert!(!text.contains("NaN"));}#[test]fn trace_is_explicit_and_bounded(){stop();assert!(!active(1));start(100);assert!(active(100));assert!(!active(100+WINDOW_NS));stop();assert!(!active(101));}}

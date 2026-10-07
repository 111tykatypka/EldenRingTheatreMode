//! The replay's world recording: "<replay>.erplay.world" (version 2, legacy version 1 readable).
//!
//! Layout (little endian): "ERWORLD1", u32 version, u32 reserved, then chunks until the end:
//!   u32 'CHNK', u32 track, u32 kind, u32 count, u64 first time, u64 last time,
//!   u32 packed bytes, u32 raw bytes, u32 crc32(packed), packed payload.
//! A chunk holds about one second of one track. Its first record is a keyframe (encoded against
//! zero), so any chunk decodes on its own: seeking needs one chunk, never the whole file.
//! Payloads are codec.rs streams (varint deltas, zero runs packed). Unknown tracks/kinds are skipped
//! by readers, so new categories can be added without breaking old builds.
//! Times are the game's monotonic clock in ns (the same source clock the host recorder stores).
//!
//! The game thread only copies data into a channel; a writer thread encodes and appends chunks to a
//! .tmp file and renames it when the recording stops (so a crash leaves no half file under the
//! real name; the .tmp can still be inspected).
use crate::codec;
use std::collections::HashMap;
use std::io::Write;
use std::path::{Path,PathBuf};
use std::sync::Arc;
use std::sync::mpsc::{Receiver,SyncSender};

pub const MAGIC:&[u8;8]=b"ERWORLD1";
pub const VERSION:u32=2; // v2 preserves all pose float words; v1 remains readable.
const CHUNK:u32=u32::from_le_bytes(*b"CHNK");
const CHUNK_HEADER:usize=4*4+8*2+4*3;
const CHUNK_NS:u64=1_000_000_000;
pub const TRACK_PLAYER:u32=1;
pub const KIND_PLAYER:u32=1;
pub const TRACK_WORLD:u32=2;
pub const KIND_WORLD:u32=2;       // clock samples (Phase 2.1)
pub const TRACK_FLAGS:u32=3;
pub const KIND_FLAGS_FULL:u32=3;  // all event flag groups at the start of the recording
pub const KIND_FLAG_EVENTS:u32=4; // flag changes
pub const TRACK_ACTORS:u32=4;
pub const KIND_ACTOR_INFO:u32=5;   // who each recorded actor is (Phase 2.2)
pub const TRACK_ACTOR_BASE:u32=0x1000; // + actor id: that actor's frames
pub const KIND_ACTOR:u32=6;
pub const TRACK_COMPANIONS:u32=5;
pub const KIND_COMPANION:u32=7;
pub const TRACK_SKELETON:u32=6;
pub const KIND_SKELETON:u32=8;
fn encode_skeleton(d:&crate::skeleton::Definition)->Vec<u8>{
 let mut out=Vec::new();for v in [d.actor_id,d.model_id,d.fingerprint,d.parents.len() as u32]{codec::put_varint(&mut out,v as u64);}
 for p in &d.parents{codec::put_signed(&mut out,*p as i64);}out
}
fn decode_skeleton(raw:&[u8])->Option<crate::skeleton::Definition>{
 let mut at=0;let mut words=[0u32;4];for v in &mut words{*v=u32::try_from(codec::get_varint(raw,&mut at)?).ok()?;}
 let n=words[3] as usize;if n==0||n>4096||n>raw.len().saturating_sub(at){return None;}
 let mut parents=Vec::with_capacity(n);for _ in 0..n{parents.push(i16::try_from(codec::get_signed(raw,&mut at)?).ok()?);}
 if at!=raw.len()||!crate::skeleton::valid_parents(&parents)||crate::skeleton::fingerprint(words[1],&parents)!=words[2]{return None;}
 Some(crate::skeleton::Definition{actor_id:words[0],model_id:words[1],fingerprint:words[2],parents})
}

#[cfg(test)]
#[test]
#[ignore = "requires THEATER_INSPECT_WORLD pointing to a real recording"]
fn inspect_real_root_track(){
 let path=std::env::var("THEATER_INSPECT_WORLD").expect("recording path");
 validate_saved_world(Path::new(&path)).expect("streaming legacy integrity validation");
 let mut file=open(Path::new(&path)).expect("valid recording");
 let mut min=[[f32::INFINITY;3];2];let mut max=[[f32::NEG_INFINITY;3];2];
 for i in 0..file.player.len(){let f=file.player.get(i).unwrap();
  for (j,v) in [f.transform[2],f.place.global].iter().enumerate(){for k in 0..3{min[j][k]=min[j][k].min(v[k]);max[j][k]=max[j][k].max(v[k]);}}
 }
 println!("REAL_ROOT_INSPECTION samples={} physics_min={:?} physics_max={:?} chunk_min={:?} chunk_max={:?}",file.player.len(),min[0],max[0],min[1],max[1]);
}

/// Pointer-free role/ride observation. id=0 is the player; mount_id=0 means unknown/unmounted.
/// All other ids refer to ActorInfo. A buddy-set member is not automatically a spirit ash.
#[derive(Clone,Copy,Debug,Default,PartialEq,Eq)]
pub struct EntityContext{pub time:u64,pub id:u32,pub category:u32,pub ride_flags:u32,pub ride_state:u32,pub ride_param:i32,pub mount_id:u32}

/// One recorded frame of the player (see bone_replay.rs for the meaning of each field).
#[derive(Clone)]
pub struct PlayerFrame{pub time:u64,pub transform:[[f32;4];3],pub matrix:[f32;16],pub local:Vec<u8>,pub model:Vec<u8>,pub place:crate::arrival::Place,pub equip:crate::equipment::Equip}

/// A recorded enemy/NPC/boss: who it is (no pointers; matched to live characters on playback).
#[derive(Clone,Copy,Debug,Default,PartialEq)]
pub struct ActorInfo{pub id:u32,pub handle:u64,pub entity:u32,pub npc_param:i32,pub chr_type:u32,pub first_seen:u64}
/// One frame of an actor: the same body data as the player (equipment unused) plus HP.
#[derive(Clone)]
pub struct ActorFrame{pub body:PlayerFrame,pub hp:i32,pub max_hp:i32}
/// World state sampled once a second (see world_state.rs).
#[derive(Clone,Copy,Debug,Default,PartialEq)]
pub struct WorldSample{pub time:u64,pub clock:crate::world_state::Clock}
#[derive(Clone,Copy,Debug,PartialEq)]
pub struct FlagEvent{pub time:u64,pub flag:u32,pub state:bool}
pub type FlagGroups=Vec<(u32,[u8;crate::world_state::FLAG_BLOCK])>;

// ---------------------------------------------------------------------------------------- encoding
fn encode_world(v:&[WorldSample])->Vec<u8>{let mut o=Vec::new();let (mut t,mut a,mut b,mut m)=(0u64,0u64,0u64,0u32);
 for s in v{codec::put_varint(&mut o,s.time.wrapping_sub(t));t=s.time;codec::put_varint(&mut o,s.clock.time64^a);a=s.clock.time64;codec::put_varint(&mut o,s.clock.date^b);b=s.clock.date;let mb=s.clock.multiplier.to_bits();codec::put_varint(&mut o,(mb^m) as u64);m=mb;}o}
fn decode_world(r:&[u8],count:usize)->Option<Vec<WorldSample>>{if count>r.len()/4{return None;}let mut at=0;let (mut t,mut a,mut b,mut m)=(0u64,0u64,0u64,0u32);let mut out=Vec::with_capacity(count);
 for _ in 0..count{t=t.checked_add(codec::get_varint(r,&mut at)?)?;a^=codec::get_varint(r,&mut at)?;b^=codec::get_varint(r,&mut at)?;m^=u32::try_from(codec::get_varint(r,&mut at)?).ok()?;
  out.push(WorldSample{time:t,clock:crate::world_state::Clock{time64:a,date:b,multiplier:f32::from_bits(m)}});}(at==r.len()).then_some(out)}
fn encode_flags(groups:&FlagGroups)->Vec<u8>{let mut o=Vec::new();codec::put_varint(&mut o,groups.len() as u64);let mut g=0u32;
 for (id,b) in groups{codec::put_varint(&mut o,id.wrapping_sub(g) as u64);g=*id;o.extend_from_slice(b);}o}
fn decode_flags(r:&[u8])->Option<FlagGroups>{let mut at=0;let n=usize::try_from(codec::get_varint(r,&mut at)?).ok()?;if n>r.len().saturating_sub(at)/(crate::world_state::FLAG_BLOCK+1){return None;}let mut g=0u32;let mut out=Vec::with_capacity(n);
 for _ in 0..n{g=g.checked_add(u32::try_from(codec::get_varint(r,&mut at)?).ok()?)?;let b:[u8;crate::world_state::FLAG_BLOCK]=r.get(at..at+crate::world_state::FLAG_BLOCK)?.try_into().ok()?;at+=crate::world_state::FLAG_BLOCK;out.push((g,b));}
 (at==r.len()).then_some(out)}
fn encode_events(v:&[FlagEvent])->Vec<u8>{let mut o=Vec::new();let mut t=0u64;for e in v{codec::put_varint(&mut o,e.time.wrapping_sub(t));t=e.time;codec::put_varint(&mut o,e.flag as u64);o.push(e.state as u8);}o}
fn decode_events(r:&[u8],count:usize)->Option<Vec<FlagEvent>>{if count>r.len()/3{return None;}let mut at=0;let mut t=0u64;let mut out=Vec::with_capacity(count);
 for _ in 0..count{t=t.checked_add(codec::get_varint(r,&mut at)?)?;let flag=u32::try_from(codec::get_varint(r,&mut at)?).ok()?;let s=*r.get(at)?;if s>1{return None;}at+=1;out.push(FlagEvent{time:t,flag,state:s!=0});}(at==r.len()).then_some(out)}
#[derive(Default)]struct PlayerEncoder{bones:Vec<u32>,model:Vec<u32>,floats:Vec<u32>,ints:Vec<u32>,time:u64}
fn equip_ints(e:&crate::equipment::Equip)->Vec<u32>{let mut v=vec![e.arm_style];v.extend(e.slots);v.extend(e.handles);v.extend(e.params.iter().map(|p|*p as u32));v}
impl PlayerEncoder{
 fn encode(&mut self,f:&PlayerFrame,out:&mut Vec<u8>){
  codec::put_varint(out,f.time.wrapping_sub(self.time));self.time=f.time;
  let mut floats:Vec<f32>=f.transform.iter().flatten().copied().collect();floats.extend(f.matrix);floats.extend(f.place.global);
  codec::encode_f32s(&floats,&mut self.floats,out);
  let mut ints=vec![f.place.block as u32,f.place.origin as u32];ints.extend(equip_ints(&f.equip));
  if self.ints.len()!=ints.len(){self.ints=vec![0;ints.len()];}
  for (v,p) in ints.iter().zip(self.ints.iter_mut()){codec::put_varint(out,(v^*p) as u64);*p=*v;}
  codec::put_varint(out,(f.local.len()/codec::QS) as u64);
  codec::encode_pose_exact(&f.local,&mut self.bones,out);codec::encode_pose_exact(&f.model,&mut self.model,out);}
}
fn decode_player(raw:&[u8],count:usize)->Option<Vec<PlayerFrame>>{let mut at=0;decode_player_at(raw,&mut at,count,None,true)}
fn decode_player_v1(raw:&[u8],count:usize)->Option<Vec<PlayerFrame>>{let mut at=0;decode_player_at(raw,&mut at,count,None,false)}
// Decodes `count` frames; with `hp`, an HP pair follows each frame (actor chunks).
fn decode_player_at(raw:&[u8],at:&mut usize,count:usize,mut hp:Option<&mut Vec<(i32,i32)>>,exact:bool)->Option<Vec<PlayerFrame>>{
 if count>raw.len(){return None;}
 let mut hp_prev=(0u32,0u32);let at_ref=at;let mut at=*at_ref;let (mut bones,mut model,mut floats,mut ints,mut time)=(Vec::new(),Vec::new(),Vec::new(),vec![0u32;2+crate::equipment::BYTES/4],0u64);
 let(mut exact_local,mut exact_model)=(Vec::new(),Vec::new());
 let mut out=Vec::with_capacity(count);
 for _ in 0..count{
  time=time.checked_add(codec::get_varint(raw,&mut at)?)?;
  let mut f=[0f32;12+16+4];codec::decode_f32s(raw,&mut at,&mut floats,&mut f)?;
  for p in ints.iter_mut(){*p^=u32::try_from(codec::get_varint(raw,&mut at)?).ok()?;}
  let n=codec::get_varint(raw,&mut at)? as usize;if n==0||n>4096{return None;}
  let mut local=vec![0u8;n*codec::QS];let mut model_pose=vec![0u8;n*codec::QS];
  if exact{codec::decode_pose_exact(raw,&mut at,n,&mut exact_local,&mut local)?;codec::decode_pose_exact(raw,&mut at,n,&mut exact_model,&mut model_pose)?;}
  else{codec::decode_pose(raw,&mut at,n,&mut bones,&mut local)?;codec::decode_pose(raw,&mut at,n,&mut model,&mut model_pose)?;}
  let e=&ints[2..];let equip=crate::equipment::Equip{arm_style:e[0],slots:std::array::from_fn(|k|e[1+k]),handles:std::array::from_fn(|k|e[7+k]),params:std::array::from_fn(|k|e[7+crate::equipment::SLOTS+k] as i32)};
  out.push(PlayerFrame{time,transform:std::array::from_fn(|r|std::array::from_fn(|c|f[r*4+c])),matrix:std::array::from_fn(|i|f[12+i]),local,model:model_pose,
   place:crate::arrival::Place{block:ints[0] as i32,origin:ints[1] as i32,global:[f[28],f[29],f[30],f[31]]},equip});
  if let Some(h)=hp.as_deref_mut(){hp_prev.0^=u32::try_from(codec::get_varint(raw,&mut at)?).ok()?;hp_prev.1^=u32::try_from(codec::get_varint(raw,&mut at)?).ok()?;h.push((hp_prev.0 as i32,hp_prev.1 as i32));}}
 *at_ref=at;(at==raw.len()).then_some(out)}
fn encode_actor(frames:&[ActorFrame])->Vec<u8>{let mut enc=PlayerEncoder::default();let mut raw=Vec::new();let mut hp=(0u32,0u32);
 for f in frames{enc.encode(&f.body,&mut raw);codec::put_varint(&mut raw,((f.hp as u32)^hp.0) as u64);codec::put_varint(&mut raw,((f.max_hp as u32)^hp.1) as u64);hp=(f.hp as u32,f.max_hp as u32);}raw}
fn decode_actor(raw:&[u8],count:usize)->Option<Vec<ActorFrame>>{if count>raw.len(){return None;}let mut at=0;let mut hp=Vec::with_capacity(count);
 let bodies=decode_player_at(raw,&mut at,count,Some(&mut hp),true)?;Some(bodies.into_iter().zip(hp).map(|(body,(hp,max_hp))|ActorFrame{body,hp,max_hp}).collect())}
fn decode_actor_v1(raw:&[u8],count:usize)->Option<Vec<ActorFrame>>{let mut at=0;let mut hp=Vec::new();let bodies=decode_player_at(raw,&mut at,count,Some(&mut hp),false)?;Some(bodies.into_iter().zip(hp).map(|(body,(hp,max_hp))|ActorFrame{body,hp,max_hp}).collect())}
fn encode_infos(v:&[ActorInfo])->Vec<u8>{let mut o=Vec::new();for a in v{for x in [a.id as u64,a.handle,a.entity as u64,a.npc_param as u32 as u64,a.chr_type as u64,a.first_seen]{codec::put_varint(&mut o,x);}}o}
fn decode_infos(r:&[u8],count:usize)->Option<Vec<ActorInfo>>{if count>r.len()/6{return None;}let mut at=0;let mut out=Vec::with_capacity(count);
 for _ in 0..count{let mut g=||codec::get_varint(r,&mut at);let (id,handle,entity,npc,kind,first)=(g()?,g()?,g()?,g()?,g()?,g()?);
  out.push(ActorInfo{id:u32::try_from(id).ok()?,handle,entity:u32::try_from(entity).ok()?,npc_param:u32::try_from(npc).ok()? as i32,chr_type:u32::try_from(kind).ok()?,first_seen:first});}(at==r.len()).then_some(out)}

fn encode_context(v:&[EntityContext])->Vec<u8>{let mut out=Vec::new();for c in v{for x in [c.time,c.id as u64,c.category as u64,c.ride_flags as u64,c.ride_state as u64,c.ride_param as u32 as u64,c.mount_id as u64]{codec::put_varint(&mut out,x);}}out}
fn decode_context(raw:&[u8],count:usize)->Option<Vec<EntityContext>>{
 if count>raw.len()/7{return None;}let mut at=0;let mut out=Vec::with_capacity(count);
 for _ in 0..count{let time=codec::get_varint(raw,&mut at)?;let mut v=[0u32;6];for x in &mut v{*x=u32::try_from(codec::get_varint(raw,&mut at)?).ok()?;}
  out.push(EntityContext{time,id:v[0],category:v[1],ride_flags:v[2],ride_state:v[3],ride_param:v[4] as i32,mount_id:v[5]});}
 (at==raw.len()).then_some(out)
}
pub fn context_at(v:&[EntityContext],id:u32,time:u64)->Option<EntityContext>{
 let end=v.partition_point(|c|c.time<=time);v[..end].iter().rev().find(|c|c.id==id).copied()
}

fn write_chunk(w:&mut impl Write,track:u32,kind:u32,count:u32,first:u64,last:u64,raw:&[u8])->std::io::Result<u64>{
 let packed=codec::pack_zeros(raw);
 let packed_len=u32::try_from(packed.len()).map_err(|_|std::io::Error::other("packed chunk exceeds 32-bit format size; split the chunk"))?;
 let raw_len=u32::try_from(raw.len()).map_err(|_|std::io::Error::other("raw chunk exceeds 32-bit format size; split the chunk"))?;
 let mut h=Vec::with_capacity(CHUNK_HEADER);
 for v in [CHUNK,track,kind,count]{h.extend_from_slice(&v.to_le_bytes());}h.extend_from_slice(&first.to_le_bytes());h.extend_from_slice(&last.to_le_bytes());
 for v in [packed_len,raw_len,codec::crc32(&packed)]{h.extend_from_slice(&v.to_le_bytes());}
 w.write_all(&h)?;w.write_all(&packed)?;Ok((h.len()+packed.len()) as u64)}

// ------------------------------------------------------------------------------------------ writer
pub enum Message{Player(PlayerFrame),PlayerPose(PlayerFrame,Option<crate::skeleton::Definition>),World(WorldSample),Flags(u64,FlagGroups),FlagEvents(Vec<FlagEvent>),Actors(Vec<(u32,ActorFrame)>),
 ActorBatch{infos:Vec<ActorInfo>,frames:Vec<(u32,ActorFrame)>,context:Vec<EntityContext>,skeletons:Vec<crate::skeleton::Definition>},
 // Explicit termination is used by data tests; runtime closes the sender to drain without blocking.
 #[allow(dead_code)]Finish}
/// Starts a writer thread for `final_path`; returns the sender the game thread uses.
pub fn start_writer(final_path:PathBuf)->std::io::Result<SyncSender<Message>>{
 let tmp=PathBuf::from(format!("{}.tmp",final_path.display()));
 // ~2 s of frames can queue; if the disk stalls longer, frames are dropped and counted (no game stall).
 let (tx,rx)=std::sync::mpsc::sync_channel::<Message>(240);
 std::thread::Builder::new().name("TheaterMode.WorldWriter".into()).spawn(move||{
  let create=||->std::io::Result<_>{let mut file=std::io::BufWriter::new(std::fs::File::create(&tmp)?);
   file.write_all(MAGIC)?;file.write_all(&VERSION.to_le_bytes())?;file.write_all(&0u32.to_le_bytes())?;Ok(file)};
  match create(){Ok(file)=>writer(rx,file,tmp,final_path),Err(e)=>{
   crate::log_game(&format!("WORLD_FILE_ERROR: cannot start writer for {}: {e}",tmp.display()));
   crate::bone_replay::status("CAPTURE FAILED: world writer could not open storage (see log).");
  }}
 })?;
 Ok(tx)}
fn writer(rx:Receiver<Message>,mut file:std::io::BufWriter<std::fs::File>,tmp:PathBuf,final_path:PathBuf){
 let mut player:Vec<PlayerFrame>=Vec::new();let mut bytes=16u64;let mut frames=0u64;let mut failed=None;
 let mut first_player=None;let mut last_player=0;
 let (mut world,mut events):(Vec<WorldSample>,Vec<FlagEvent>)=(Vec::new(),Vec::new());let mut flag_changes=0u64;
 let mut actors:HashMap<u32,Vec<ActorFrame>>=HashMap::new();let mut infos:Vec<ActorInfo>=Vec::new();let mut actor_frames=0u64;
 let mut context=Vec::<EntityContext>::new();
 let mut durable_at=std::time::Instant::now();
 let flush_actor=|id:u32,frames:&mut Vec<ActorFrame>,file:&mut std::io::BufWriter<std::fs::File>|->std::io::Result<u64>{
  if frames.is_empty(){return Ok(0);}let n=write_chunk(file,TRACK_ACTOR_BASE+id,KIND_ACTOR,frames.len() as u32,frames[0].body.time,frames.last().unwrap().body.time,&encode_actor(frames))?;frames.clear();Ok(n)};
 let flush=|frames_buf:&mut Vec<PlayerFrame>,file:&mut std::io::BufWriter<std::fs::File>|->std::io::Result<u64>{
  if frames_buf.is_empty(){return Ok(0);}
  let mut enc=PlayerEncoder::default();let mut raw=Vec::new();for f in frames_buf.iter(){enc.encode(f,&mut raw);}
  let n=write_chunk(file,TRACK_PLAYER,KIND_PLAYER,frames_buf.len() as u32,frames_buf[0].time,frames_buf.last().unwrap().time,&raw)?;frames_buf.clear();Ok(n)};
 // Small tracks (world samples, flag changes) are written in chunks of up to a minute.
 let flush_small=|world:&mut Vec<WorldSample>,events:&mut Vec<FlagEvent>,file:&mut std::io::BufWriter<std::fs::File>|->std::io::Result<u64>{
  let mut n=0;
  if !world.is_empty(){n+=write_chunk(file,TRACK_WORLD,KIND_WORLD,world.len() as u32,world[0].time,world.last().unwrap().time,&encode_world(world))?;world.clear();}
  if !events.is_empty(){n+=write_chunk(file,TRACK_FLAGS,KIND_FLAG_EVENTS,events.len() as u32,events[0].time,events.last().unwrap().time,&encode_events(events))?;events.clear();}
  Ok(n)};
 for m in rx{
  // The catalog, poses and context enter the queue atomically: losing an announcement can no
  // longer produce a permanently orphaned actor track. Encoding remains on this worker.
  let m=if let Message::PlayerPose(frame,definition)=m{
   if let Some(d)=definition{match write_chunk(&mut file,TRACK_SKELETON,KIND_SKELETON,1,frame.time,frame.time,&encode_skeleton(&d)){Ok(n)=>bytes+=n,Err(e)=>{failed.get_or_insert(e.to_string());}}}
   Message::Player(frame)
  }else{m};
  let m=if let Message::ActorBatch{infos:new_infos,frames,context:new_context,skeletons}=m{
   for d in skeletons{match write_chunk(&mut file,TRACK_SKELETON,KIND_SKELETON,1,0,0,&encode_skeleton(&d)){Ok(n)=>bytes+=n,Err(e)=>{failed.get_or_insert(e.to_string());}}}
   infos.extend(new_infos);context.extend(new_context);
   if infos.len()>=64{match write_chunk(&mut file,TRACK_ACTORS,KIND_ACTOR_INFO,infos.len() as u32,infos[0].first_seen,infos.last().unwrap().first_seen,&encode_infos(&infos)){Ok(n)=>bytes+=n,Err(e)=>{failed.get_or_insert(e.to_string());}}infos.clear();}
   if context.len()>=4096{match write_chunk(&mut file,TRACK_COMPANIONS,KIND_COMPANION,context.len() as u32,context[0].time,context.last().unwrap().time,&encode_context(&context)){Ok(n)=>bytes+=n,Err(e)=>{failed.get_or_insert(e.to_string());}}context.clear();}
   Message::Actors(frames)
  }else{m};
  let r=match m{
   Message::Player(f)=>{frames+=1;first_player.get_or_insert(f.time);last_player=f.time;let r=if player.first().is_some_and(|p|f.time.saturating_sub(p.time)>=CHUNK_NS){flush(&mut player,&mut file)}else{Ok(0)};player.push(f);r}
   Message::World(w)=>{world.push(w);if world.len()>=60{flush_small(&mut world,&mut events,&mut file)}else{Ok(0)}}
   Message::Flags(t,g)=>write_chunk(&mut file,TRACK_FLAGS,KIND_FLAGS_FULL,g.len() as u32,t,t,&encode_flags(&g)),
   Message::FlagEvents(e)=>{flag_changes+=e.len() as u64;events.extend(e);if events.len()>=4096{flush_small(&mut world,&mut events,&mut file)}else{Ok(0)}}
   Message::Actors(list)=>{let mut total=Ok(0);for (id,f) in list{actor_frames+=1;let buf=actors.entry(id).or_default();
     if buf.first().is_some_and(|p|f.body.time.saturating_sub(p.body.time)>=CHUNK_NS){match flush_actor(id,buf,&mut file){Ok(n)=>{if let Ok(t)=&mut total{*t+=n;}},Err(e)=>total=Err(e)}}buf.push(f);}total}
   Message::ActorBatch{..}|Message::PlayerPose(..)=>unreachable!(),Message::Finish=>break};
  match r{Ok(n)=>bytes+=n,Err(e)=>{failed.get_or_insert(e.to_string());}}
  // Durability work remains on this writer; a crash can still lose the active chunk/queue.
  if durable_at.elapsed()>=std::time::Duration::from_secs(5){
   if let Err(e)=file.flush().and_then(|_|file.get_ref().sync_data()){failed.get_or_insert(e.to_string());}
   durable_at=std::time::Instant::now();
  }
 }
 if failed.is_none(){match flush(&mut player,&mut file).and_then(|n|{bytes+=n;bytes+=flush_small(&mut world,&mut events,&mut file)?;
   if !infos.is_empty(){bytes+=write_chunk(&mut file,TRACK_ACTORS,KIND_ACTOR_INFO,infos.len() as u32,infos[0].first_seen,infos[0].first_seen,&encode_infos(&infos))?;}
   for (id,buf) in actors.iter_mut(){bytes+=flush_actor(*id,buf,&mut file)?;}
   if !context.is_empty(){bytes+=write_chunk(&mut file,TRACK_COMPANIONS,KIND_COMPANION,context.len() as u32,context[0].time,context.last().unwrap().time,&encode_context(&context))?;}
   file.flush()?;file.get_ref().sync_all()?;Ok(())}){Ok(())=>{},Err(e)=>failed=Some(e.to_string())}}
 drop(file);
 if frames==0&&failed.is_none(){failed=Some("no validated player poses were captured; partial file not finalized".into());}
 if failed.is_none(){if let Err(e)=validate_saved_world(&tmp){failed=Some(format!("saved world validation failed: {e}"));}
  else{crate::log_game("WORLD_FILE_VALIDATED: header, chunks, decoding, ordering and identity references passed");}}
 match failed{
  None=>match std::fs::rename(&tmp,&final_path){
   Ok(())=>{let minutes=last_player.saturating_sub(first_player.unwrap_or(last_player)) as f64/60e9;let rate=if minutes>0.0{format!("{:.2}",bytes as f64/1_048_576.0/minutes)}else{"unavailable".into()};crate::log_game(&format!("WORLD_FILE: saved {} ({frames} player frames, {} actors / {actor_frames} actor frames, {flag_changes} flag changes, {:.1} MB, {rate} MB per minute)",final_path.display(),actors.len(),bytes as f64/1_048_576.0));crate::bone_replay::status("BONE REPLAY: recording saved");}
   Err(e)=>{crate::log_game(&format!("WORLD_FILE_ERROR: could not rename {}: {e}",tmp.display()));crate::bone_replay::status("BONE REPLAY ERROR: could not save the recording (see log)");}},
  Some(e)=>{crate::log_game(&format!("WORLD_FILE_ERROR: writing {} failed: {e}; the partial file stays as .tmp",tmp.display()));crate::bone_replay::status("BONE REPLAY ERROR: could not save the recording (see log)");}}}

// ------------------------------------------------------------------------------------------ reader
/// Worker-only finalization validation, bounded to one decoded chunk. No whole-file pose cache.
fn validate_saved_world(path:&Path)->Result<(),String>{
 use std::io::Read;
 let mut file=std::fs::File::open(path).map_err(|e|e.to_string())?;
 let size=file.metadata().map_err(|e|e.to_string())?.len();let mut prefix=[0u8;16];
 file.read_exact(&mut prefix).map_err(|e|e.to_string())?;
 if &prefix[..8]!=MAGIC{return Err("invalid magic".into());}
 let version=u32::from_le_bytes(prefix[8..12].try_into().unwrap());if !matches!(version,1|2){return Err("unsupported world version".into());}
 let player_decoder=if version==1{decode_player_v1}else{decode_player};
 let actor_decoder=if version==1{decode_actor_v1}else{decode_actor};
 let mut at=16u64;let mut player_count=0usize;let mut last=HashMap::<(u32,u32),u64>::new();
 let mut ids=std::collections::HashSet::new();let mut references=std::collections::HashSet::new();
 let mut skeletons=HashMap::<u32,crate::skeleton::Definition>::new();let mut bone_counts=HashMap::<u32,usize>::new();
 while at<size{
  if size-at<CHUNK_HEADER as u64{return Err("incomplete trailing chunk".into());}
  let mut h=[0u8;CHUNK_HEADER];file.read_exact(&mut h).map_err(|e|e.to_string())?;
  let word=|i|u32::from_le_bytes(h[i..i+4].try_into().unwrap());
  let(track,kind,count,packed,raw,crc)=(word(4),word(8),word(12)as usize,word(32)as usize,word(36)as usize,word(40));
  if word(0)!=CHUNK{return Err("invalid chunk marker".into());}
  at+=CHUNK_HEADER as u64;if packed as u64>size-at{return Err("cut-off chunk payload".into());}
  let mut payload=vec![0;packed];file.read_exact(&mut payload).map_err(|e|e.to_string())?;at+=packed as u64;
  if codec::crc32(&payload)!=crc{return Err("chunk checksum mismatch".into());}
  let r=codec::unpack_zeros(&payload).filter(|r|r.len()==raw).ok_or("invalid packed/raw chunk size")?;
  let mut times=Vec::new();
  match (track,kind){
   (TRACK_PLAYER,KIND_PLAYER)=>{let v=player_decoder(&r,count).ok_or("invalid player payload")?;player_count+=v.len();
    for f in v{times.push(f.time);let n=f.local.len()/codec::QS;if bone_counts.insert(0,n).is_some_and(|old|old!=n){return Err("player skeleton size changes within one identity".into());}}}
   (TRACK_WORLD,KIND_WORLD)=>times.extend(decode_world(&r,count).ok_or("invalid world payload")?.into_iter().map(|f|f.time)),
   (TRACK_FLAGS,KIND_FLAGS_FULL)=>{let v=decode_flags(&r).ok_or("invalid flag snapshot")?;if v.len()!=count{return Err("flag group count mismatch".into());}},
   (TRACK_FLAGS,KIND_FLAG_EVENTS)=>times.extend(decode_events(&r,count).ok_or("invalid flag event payload")?.into_iter().map(|f|f.time)),
   (TRACK_ACTORS,KIND_ACTOR_INFO)=>for info in decode_infos(&r,count).ok_or("invalid actor catalog")?{if info.id==0||!ids.insert(info.id){return Err("duplicate/zero actor identity".into());}},
   (TRACK_COMPANIONS,KIND_COMPANION)=>for c in decode_context(&r,count).ok_or("invalid companion payload")?{times.push(c.time);if c.id!=0{references.insert(c.id);}if c.mount_id!=0{references.insert(c.mount_id);}},
   (TRACK_SKELETON,KIND_SKELETON)=>{if count!=1{return Err("invalid skeleton record count".into());}let d=decode_skeleton(&r).ok_or("invalid skeleton definition")?;
    if d.actor_id!=0{references.insert(d.actor_id);}if skeletons.insert(d.actor_id,d).is_some(){return Err("duplicate skeleton identity".into());}},
   (id,KIND_ACTOR) if id>=TRACK_ACTOR_BASE=>{let id=id-TRACK_ACTOR_BASE;references.insert(id);let v=actor_decoder(&r,count).ok_or("invalid actor payload")?;
    for f in v{times.push(f.body.time);let n=f.body.local.len()/codec::QS;if bone_counts.insert(id,n).is_some_and(|old|old!=n){return Err("actor skeleton size changes within one identity".into());}}},
   _=>{}, // unknown categories remain forward-compatible, with envelope integrity checked
  }
  for t in times{if last.insert((track,kind),t).is_some_and(|previous|previous>t){return Err("track timestamp regression".into());}}
 }
 if player_count==0{return Err("no player frames".into());}
 if references.iter().any(|id|!ids.contains(id)){return Err("unresolved actor reference".into());}
 if skeletons.iter().any(|(id,d)|bone_counts.get(id).is_some_and(|n|*n!=d.parents.len())){return Err("skeleton definition and pose count differ".into());}
 Ok(())
}

#[derive(Clone,Copy)]struct ChunkRef{offset:usize,packed:usize,raw:usize,count:usize,first_index:usize}
/// Frames of one track: all frame times up front (for seeking), frames decoded one chunk at a time and
/// cached, so playback at any speed decodes each chunk once. The file bytes are shared by all tracks.
pub struct Track<T:Clone>{data:Arc<Vec<u8>>,chunks:Vec<ChunkRef>,pub times:Vec<u64>,cache:Vec<(usize,Vec<T>)>,decode:fn(&[u8],usize)->Option<Vec<T>>}
pub type PlayerTrack=Track<PlayerFrame>;
pub type ActorTrack=Track<ActorFrame>;
impl<T:Clone> Track<T>{
 pub fn len(&self)->usize{self.times.len()}
 pub fn get(&mut self,i:usize)->Option<&T>{
  let c=self.chunks.partition_point(|c|c.first_index<=i).checked_sub(1)?;
  if !self.cache.iter().any(|(k,_)|*k==c){
   let r=self.chunks[c];let packed=&self.data[r.offset..r.offset+r.packed];
   let raw=codec::unpack_zeros(packed)?;if raw.len()!=r.raw{return None;}
   let frames=(self.decode)(&raw,r.count)?;
   if self.cache.len()>=4{self.cache.remove(0);}self.cache.push((c,frames));}
  let (_,frames)=self.cache.iter().find(|(k,_)|*k==c)?;frames.get(i-self.chunks[c].first_index)}
}
/// Everything in a world file. Player frames stay compressed (chunk cache); small tracks are decoded.
pub struct WorldFile{pub player:PlayerTrack,pub world:Vec<WorldSample>,pub flags_start:Option<(u64,FlagGroups)>,pub flag_events:Vec<FlagEvent>,pub actors:Vec<(ActorInfo,ActorTrack)>,pub context:Vec<EntityContext>,pub skeletons:HashMap<u32,crate::skeleton::Definition>}
/// Opens a world file and indexes its chunks (every chunk's integrity is checked once here).
pub fn open(path:&Path)->Result<WorldFile,String>{
 let data=std::fs::read(path).map_err(|e|e.to_string())?;
 if data.len()<16||&data[0..8]!=MAGIC{return Err("not a Theater Mode world file".into());}
 let version=u32::from_le_bytes(data[8..12].try_into().unwrap());if !matches!(version,1|2){return Err(format!("unsupported world file version {version}"));}
 let player_decoder:fn(&[u8],usize)->Option<Vec<PlayerFrame>>=if version==1{decode_player_v1}else{decode_player};
 let actor_decoder:fn(&[u8],usize)->Option<Vec<ActorFrame>>=if version==1{decode_actor_v1}else{decode_actor};
 let (mut at,mut chunks,mut times,mut skipped)=(16usize,Vec::new(),Vec::new(),HashMap::<u32,usize>::new());
 let (mut world,mut flags_start,mut flag_events)=(Vec::new(),None,Vec::new());
 let mut context=Vec::new();
 let mut skeletons=HashMap::new();
 let (mut infos,mut actor_chunks):(Vec<ActorInfo>,HashMap<u32,(Vec<ChunkRef>,Vec<u64>)>)=(Vec::new(),HashMap::new());
 let u32_at=|i:usize|u32::from_le_bytes(data[i..i+4].try_into().unwrap());
 while at+CHUNK_HEADER<=data.len(){
  if u32_at(at)!=CHUNK{return Err(format!("damaged chunk header at byte {at}"));}
  let (track,kind,count)=(u32_at(at+4),u32_at(at+8),u32_at(at+12) as usize);
  let (packed,raw,crc)=(u32_at(at+32) as usize,u32_at(at+36) as usize,u32_at(at+40));
  let body=at+CHUNK_HEADER;if body+packed>data.len(){return Err(format!("chunk at byte {at} is cut off"));}
  if codec::crc32(&data[body..body+packed])!=crc{return Err(format!("chunk at byte {at} is damaged (checksum)"));}
  if track==TRACK_PLAYER&&kind==KIND_PLAYER{
   let first_index=times.len();chunks.push(ChunkRef{offset:body,packed,raw,count,first_index});
   // Times are needed for seeking; decode just the time column cheaply by decoding the chunk once.
   let frames=codec::unpack_zeros(&data[body..body+packed]).filter(|r|r.len()==raw).and_then(|r|player_decoder(&r,count)).ok_or(format!("chunk at byte {at} does not decode/has invalid raw size"))?;
   times.extend(frames.iter().map(|f|f.time));}
  else if matches!((track,kind),(TRACK_WORLD,KIND_WORLD)|(TRACK_FLAGS,KIND_FLAGS_FULL)|(TRACK_FLAGS,KIND_FLAG_EVENTS)){
   let raw=codec::unpack_zeros(&data[body..body+packed]).filter(|r|r.len()==raw).ok_or(format!("chunk at byte {at} does not unpack/has invalid raw size"))?;
   let bad=||format!("chunk at byte {at} does not decode");
   match kind{KIND_WORLD=>world.extend(decode_world(&raw,count).ok_or_else(bad)?),
    KIND_FLAGS_FULL=>{if flags_start.is_none(){flags_start=Some((u64::from_le_bytes(data[at+16..at+24].try_into().unwrap()),decode_flags(&raw).ok_or_else(bad)?));}}
    _=>flag_events.extend(decode_events(&raw,count).ok_or_else(bad)?)}}
  else if track==TRACK_SKELETON&&kind==KIND_SKELETON{
   let unpacked=codec::unpack_zeros(&data[body..body+packed]).ok_or("skeleton chunk does not unpack")?;
   if unpacked.len()!=raw||count!=1{return Err("invalid skeleton chunk size/count".into());}
   let d=decode_skeleton(&unpacked).ok_or("invalid skeleton hierarchy/fingerprint")?;
   if skeletons.insert(d.actor_id,d).is_some(){return Err("duplicate skeleton definition".into());}
  }
  else if track==TRACK_COMPANIONS&&kind==KIND_COMPANION{
   let unpacked=codec::unpack_zeros(&data[body..body+packed]).ok_or(format!("companion chunk at {at} does not unpack"))?;
   if unpacked.len()!=raw{return Err(format!("companion chunk at {at} has invalid raw size"));}
   context.extend(decode_context(&unpacked,count).ok_or(format!("companion chunk at {at} does not decode"))?);}
  else if track==TRACK_ACTORS&&kind==KIND_ACTOR_INFO{
   let raw=codec::unpack_zeros(&data[body..body+packed]).filter(|r|r.len()==raw).ok_or(format!("chunk at byte {at} does not unpack/has invalid raw size"))?;
   infos.extend(decode_infos(&raw,count).ok_or(format!("chunk at byte {at} does not decode"))?);}
  else if track>=TRACK_ACTOR_BASE&&kind==KIND_ACTOR{
   let e=actor_chunks.entry(track-TRACK_ACTOR_BASE).or_default();let first_index=e.1.len();e.0.push(ChunkRef{offset:body,packed,raw,count,first_index});
   let frames=codec::unpack_zeros(&data[body..body+packed]).filter(|r|r.len()==raw).and_then(|r|actor_decoder(&r,count)).ok_or(format!("chunk at byte {at} does not decode/has invalid raw size"))?;
   e.1.extend(frames.iter().map(|f|f.body.time));}
  else{*skipped.entry(track).or_default()+=1;}
  at=body+packed;}
 if times.is_empty(){return Err("the world file has no player frames".into());}
 if at!=data.len(){return Err("incomplete trailing world chunk".into());}
 if times.windows(2).any(|w|w[1]<w[0]){return Err("frame times go backwards".into());}
 if context.windows(2).any(|w|w[1].time<w[0].time){return Err("companion times go backwards".into());}
 if world.windows(2).any(|w|w[1].time<w[0].time)||flag_events.windows(2).any(|w|w[1].time<w[0].time){return Err("world/event times go backwards".into());}
 let mut ids=std::collections::HashSet::new();for a in &infos{if a.id==0||!ids.insert(a.id){return Err("duplicate/zero actor id".into());}}
 if skeletons.keys().any(|id|*id!=0&&!ids.contains(id)){return Err("skeleton references unknown actor".into());}
 if actor_chunks.keys().any(|id|!ids.contains(id)){return Err("pose track references unknown actor".into());}
 if actor_chunks.values().any(|(_,times)|times.windows(2).any(|w|w[1]<w[0])){return Err("actor times go backwards".into());}
 if context.iter().any(|c|(c.id!=0&&!ids.contains(&c.id))||(c.mount_id!=0&&!ids.contains(&c.mount_id))){return Err("companion context references unknown actor".into());}
 if !skipped.is_empty(){crate::log_game(&format!("WORLD_FILE: tracks this build does not use yet: {skipped:?}"));}
 let data=Arc::new(data);
 let mut actors=Vec::new();
 for info in infos{if let Some((chunks,times))=actor_chunks.remove(&info.id){if times.windows(2).all(|w|w[1]>=w[0]){actors.push((info,Track{data:data.clone(),chunks,times,cache:Vec::new(),decode:actor_decoder}));}}}
 Ok(WorldFile{player:Track{data,chunks,times,cache:Vec::new(),decode:player_decoder},world,flags_start,flag_events,actors,context,skeletons})}

#[cfg(test)]mod tests{
 #[test]fn exact_dynamic_bone_count_and_definition_survive_the_file(){
  let dir=std::env::temp_dir().join(format!("tm_skeleton_file_{}",std::process::id()));std::fs::create_dir_all(&dir).unwrap();
  let path=dir.join("dynamic.world");let _=std::fs::remove_file(&path);
  let mut f=frame(0);f.local=f.local[..48].repeat(250);f.model=f.model[..48].repeat(250);
  let parents:Vec<i16>=(0..250).map(|i|i as i16-1).collect();
  let d=crate::skeleton::Definition{actor_id:0,model_id:123,fingerprint:crate::skeleton::fingerprint(123,&parents),parents};
  let tx=start_writer(path.clone()).unwrap();tx.send(Message::PlayerPose(f.clone(),Some(d.clone()))).unwrap();drop(tx);
  for _ in 0..200{if path.exists(){break;}std::thread::sleep(std::time::Duration::from_millis(10));}
  let mut loaded=open(&path).unwrap();assert_eq!(loaded.skeletons.get(&0),Some(&d));
  let decoded=loaded.player.get(0).unwrap();assert_eq!(decoded.local,f.local);assert_eq!(decoded.model,f.model);
  drop(loaded);std::fs::remove_dir_all(dir).ok();
 }
 #[test]fn invalid_counts_events_and_raw_sizes_are_rejected(){
  assert!(decode_world(&[],usize::MAX).is_none());assert!(decode_infos(&[],usize::MAX).is_none());
  assert!(decode_actor(&[],usize::MAX).is_none());assert!(decode_events(&[0,0,2],1).is_none());
  let mut huge=Vec::new();codec::put_varint(&mut huge,u64::MAX);assert!(decode_flags(&huge).is_none());
  let mut enc=PlayerEncoder::default();let mut raw=Vec::new();enc.encode(&frame(0),&mut raw);
  let mut bytes=MAGIC.to_vec();bytes.extend(VERSION.to_le_bytes());bytes.extend(0u32.to_le_bytes());
  write_chunk(&mut bytes,TRACK_PLAYER,KIND_PLAYER,1,1000,1000,&raw).unwrap();
  let path=std::env::temp_dir().join(format!("tm_bad_world_{}.world",std::process::id()));
  bytes[52..56].copy_from_slice(&0u32.to_le_bytes());std::fs::write(&path,&bytes).unwrap();
  assert!(open(&path).err().unwrap().contains("raw size"));std::fs::remove_file(path).ok();
 }
 #[test]fn no_player_pose_does_not_finalize_a_world_file(){
  let path=std::env::temp_dir().join(format!("tm_empty_world_{}.world",std::process::id()));
  let tmp=PathBuf::from(format!("{}.tmp",path.display()));let _=std::fs::remove_file(&path);let _=std::fs::remove_file(&tmp);
  let tx=start_writer(path.clone()).unwrap();drop(tx);
  for _ in 0..100{if tmp.exists(){break;}std::thread::sleep(std::time::Duration::from_millis(10));}
  std::thread::sleep(std::time::Duration::from_millis(20));assert!(!path.exists());assert!(tmp.exists());std::fs::remove_file(tmp).ok();
 }
 use super::*;
 #[test]fn recording_duration_has_no_ten_minute_cutoff(){
  let dir=std::env::temp_dir().join(format!("tm_long_world_{}",std::process::id()));std::fs::create_dir_all(&dir).unwrap();let path=dir.join("long.world");let _=std::fs::remove_file(&path);
  let tx=start_writer(path.clone()).unwrap();for i in 0..=660{let mut f=frame(i);f.time=1_000+i*1_000_000_000;tx.send(Message::Player(f)).unwrap();}drop(tx);
  for _ in 0..500{if path.exists(){break;}std::thread::sleep(std::time::Duration::from_millis(10));}
  let mut w=open(&path).unwrap();assert_eq!(w.player.len(),661);assert_eq!(w.player.get(660).unwrap().time-w.player.get(0).unwrap().time,660_000_000_000);std::fs::remove_dir_all(dir).ok();
 }
 #[test]fn actual_hierarchy_metadata_roundtrip_and_damage(){
  let parents=vec![-1,0,1,0];let d=crate::skeleton::Definition{actor_id:7,model_id:8000,fingerprint:crate::skeleton::fingerprint(8000,&parents),parents};
  let mut raw=encode_skeleton(&d);assert_eq!(decode_skeleton(&raw),Some(d));raw.push(0);assert!(decode_skeleton(&raw).is_none());
 }
 #[test]fn direct_seek_matches_sequential_pose_and_root(){
  let frames:Vec<_>=(0..8).map(frame).collect();let mut enc=PlayerEncoder::default();let mut raw=Vec::new();for f in &frames{enc.encode(f,&mut raw);}
  let decoded=decode_player(&raw,frames.len()).unwrap();
  for target in [7,2,0,6,3]{let direct=decoded.iter().find(|f|f.time==frames[target].time).unwrap();
   assert_eq!(direct.local,frames[target].local);assert_eq!(direct.model,frames[target].model);assert_eq!(direct.transform,frames[target].transform);}
 }
 #[test]fn legacy_quantized_frame_is_still_readable(){
  let f=frame(0);let mut raw=Vec::new();codec::put_varint(&mut raw,f.time);
  let mut floats:Vec<_>=f.transform.iter().flatten().copied().collect();floats.extend(f.matrix);floats.extend(f.place.global);codec::encode_f32s(&floats,&mut Vec::new(),&mut raw);
  let mut ints=vec![f.place.block as u32,f.place.origin as u32];ints.extend(equip_ints(&f.equip));for x in ints{codec::put_varint(&mut raw,x as u64);}
  codec::put_varint(&mut raw,(f.local.len()/codec::QS) as u64);codec::encode_pose(&f.local,&mut Vec::new(),&mut raw);codec::encode_pose(&f.model,&mut Vec::new(),&mut raw);
  let old=decode_player_v1(&raw,1).unwrap();assert_eq!(old[0].transform,f.transform);assert_eq!(old[0].local.len(),f.local.len());assert_eq!(old[0].time,f.time);
 }
 #[test]fn companion_context_roundtrip_seek_and_malformed(){
  let states=vec![EntityContext{time:100,id:0,ride_flags:1,..Default::default()},EntityContext{time:110,id:7,category:3,ride_flags:9,ride_state:5,ride_param:-25,mount_id:0},EntityContext{time:200,id:0,category:0,ride_flags:5,ride_state:5,ride_param:50,mount_id:7},EntityContext{time:300,id:0,ride_flags:1,..Default::default()}];
  let raw=encode_context(&states);assert_eq!(decode_context(&raw,states.len()),Some(states.clone()));
  assert_eq!(context_at(&states,0,99),None);assert_eq!(context_at(&states,0,250),Some(states[2]));assert_eq!(context_at(&states,0,150),Some(states[0]));assert_eq!(context_at(&states,0,300),Some(states[3]));
  assert_eq!(decode_context(&raw[..raw.len()-1],states.len()),None);assert_eq!(decode_context(&raw,usize::MAX),None);
  let mut trailing=raw;trailing.push(0);assert_eq!(decode_context(&trailing,states.len()),None);
 }
 #[test]fn atomic_catalog_and_companion_track_is_backward_compatible(){
  let dir=std::env::temp_dir().join(format!("tm_companion_test_{}",std::process::id()));std::fs::create_dir_all(&dir).unwrap();let path=dir.join("companions.world");let _=std::fs::remove_file(&path);
  let tx=start_writer(path.clone()).unwrap();tx.send(Message::Player(frame(0))).unwrap();
  let context=vec![EntityContext{time:1000,id:0,ride_flags:5,ride_state:5,mount_id:9,..Default::default()},EntityContext{time:1000,id:9,category:3,ride_flags:9,..Default::default()}];
  tx.send(Message::ActorBatch{infos:vec![ActorInfo{id:9,handle:777,npc_param:42,first_seen:1000,..Default::default()}],frames:vec![(9,ActorFrame{body:frame(0),hp:100,max_hp:100})],context:context.clone(),skeletons:Vec::new()}).unwrap();tx.send(Message::Finish).unwrap();drop(tx);
  for _ in 0..200{if path.exists(){break;}std::thread::sleep(std::time::Duration::from_millis(10));}
  let loaded=open(&path).unwrap();assert_eq!(loaded.context,context);assert_eq!(loaded.actors[0].0.id,9);
  // Remove only the optional companion chunks: the remaining old track layout still loads.
  let bytes=std::fs::read(&path).unwrap();let mut old=bytes[..16].to_vec();let mut at=16;while at<bytes.len(){let track=u32::from_le_bytes(bytes[at+4..at+8].try_into().unwrap());let n=u32::from_le_bytes(bytes[at+32..at+36].try_into().unwrap()) as usize;let end=at+CHUNK_HEADER+n;if track!=TRACK_COMPANIONS{old.extend_from_slice(&bytes[at..end]);}at=end;}
  let old_path=dir.join("old.world");std::fs::write(&old_path,&old).unwrap();let old_loaded=open(&old_path).unwrap();assert!(old_loaded.context.is_empty());assert_eq!(old_loaded.actors.len(),1);
  let mut dangling=old.clone();write_chunk(&mut dangling,TRACK_COMPANIONS,KIND_COMPANION,1,1000,1000,&encode_context(&[EntityContext{time:1000,id:123,..Default::default()}])).unwrap();std::fs::write(&old_path,&dangling).unwrap();assert!(open(&old_path).err().unwrap().contains("unknown actor"));
  old.push(1);std::fs::write(&old_path,&old).unwrap();assert!(open(&old_path).err().unwrap().contains("trailing"));
  std::fs::remove_dir_all(dir).ok();
 }
 fn frame(i:u64)->PlayerFrame{
  let pose:Vec<u8>=(0..150).flat_map(|b|{let a=(b as f32*0.01+i as f32*0.02).sin()*0.5;let n=(a*a+1.0).sqrt();
   [0.1f32,b as f32*0.01,i as f32*0.001,0.0,a/n,0.0,0.0,1.0/n,1.0,1.0,1.0,1.0].iter().flat_map(|x|x.to_le_bytes()).collect::<Vec<_>>()}).collect();
  PlayerFrame{time:1_000+i*16_666_667,transform:[[0.0,0.7,0.0,0.7],[0.0,0.7,0.0,0.7],[i as f32*0.1,2.0,3.0,1.0]],matrix:[1.0;16],local:pose.clone(),model:pose,
   place:crate::arrival::Place{block:0x3C2A2400,origin:0x3C2A2400,global:[100.0+i as f32,5.0,7.0,1.0]},equip:crate::equipment::Equip{arm_style:1,slots:[0,1,0,0,0,0],handles:[7;22],params:[i as i32;22]}}}
 #[test]fn write_then_read(){
  let dir=std::env::temp_dir().join(format!("tm_world_test_{}",std::process::id()));std::fs::create_dir_all(&dir).unwrap();
  let path=dir.join("a.erplay.world");
  let tx=start_writer(path.clone()).unwrap();
  let mut g=[0u8;125];g[3]=0x10;tx.send(Message::Flags(5,vec![(7,g),(4000,[0;125])])).unwrap();
  for i in 0..200{tx.send(Message::Player(frame(i))).unwrap();if i%60==0{tx.send(Message::World(WorldSample{time:i,clock:crate::world_state::Clock{time64:1000+i,date:77,multiplier:1.0}})).unwrap();tx.send(Message::FlagEvents(vec![FlagEvent{time:i,flag:7024+i as u32,state:i%2==0}])).unwrap();}}
  tx.send(Message::ActorBatch{infos:vec![ActorInfo{id:3,handle:77,entity:1043360200,npc_param:-5,chr_type:0,first_seen:1000}],frames:Vec::new(),context:Vec::new(),skeletons:Vec::new()}).unwrap();
  tx.send(Message::Actors((0..90).map(|i|(3u32,ActorFrame{body:frame(i),hp:500-i as i32,max_hp:500})).collect())).unwrap();
  tx.send(Message::Finish).unwrap();drop(tx);
  for _ in 0..200{if path.exists(){break;}std::thread::sleep(std::time::Duration::from_millis(10));}
  let w=open(&path).unwrap();assert_eq!(w.world.len(),4);assert_eq!(w.world[2].clock.time64,1120);assert_eq!(w.flag_events.len(),4);assert_eq!(w.flag_events[1].flag,7084);
  let (ft,fg)=w.flags_start.clone().unwrap();assert_eq!(ft,5);assert_eq!(fg[0].0,7);assert_eq!(fg[0].1[3],0x10);assert_eq!(fg[1].0,4000);
  assert_eq!(w.actors.len(),1);let (info,mut at)=(w.actors[0].0,{let mut v=w.actors;v.remove(0).1});assert_eq!(info.entity,1043360200);assert_eq!(info.npc_param,-5);
  assert_eq!(at.len(),90);let f=at.get(70).unwrap().clone();assert_eq!(f.hp,430);assert_eq!(f.max_hp,500);assert_eq!(f.body.time,frame(70).time);
  let mut t=w.player;assert_eq!(t.len(),200);assert!(t.chunks.len()>=3);
  for i in [0usize,59,60,61,150,199]{let f=t.get(i).unwrap().clone();let e=frame(i as u64);
   assert_eq!(f.time,e.time);assert_eq!(f.transform,e.transform);assert_eq!(f.place,e.place);assert_eq!(f.equip,e.equip);
   assert_eq!(f.local,e.local);assert_eq!(f.model,e.model);}
  let size=std::fs::metadata(&path).unwrap().len();println!("LOSSLESS_FIXTURE_STORAGE: {size} bytes for 290 frames (200 player + 90 actor)");
  std::fs::remove_dir_all(&dir).ok();}
}

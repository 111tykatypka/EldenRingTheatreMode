//! The replay's world recording: "<replay>.erplay.world" (Phase 2, format version 1).
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
use crate::codec::{self,QBone};
use std::collections::HashMap;
use std::io::Write;
use std::path::{Path,PathBuf};
use std::sync::Arc;
use std::sync::mpsc::{Receiver,SyncSender};

pub const MAGIC:&[u8;8]=b"ERWORLD1";
pub const VERSION:u32=1;
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
fn decode_world(r:&[u8],count:usize)->Option<Vec<WorldSample>>{let mut at=0;let (mut t,mut a,mut b,mut m)=(0u64,0u64,0u64,0u32);let mut out=Vec::with_capacity(count);
 for _ in 0..count{t=t.wrapping_add(codec::get_varint(r,&mut at)?);a^=codec::get_varint(r,&mut at)?;b^=codec::get_varint(r,&mut at)?;m^=codec::get_varint(r,&mut at)? as u32;
  out.push(WorldSample{time:t,clock:crate::world_state::Clock{time64:a,date:b,multiplier:f32::from_bits(m)}});}(at==r.len()).then_some(out)}
fn encode_flags(groups:&FlagGroups)->Vec<u8>{let mut o=Vec::new();codec::put_varint(&mut o,groups.len() as u64);let mut g=0u32;
 for (id,b) in groups{codec::put_varint(&mut o,id.wrapping_sub(g) as u64);g=*id;o.extend_from_slice(b);}o}
fn decode_flags(r:&[u8])->Option<FlagGroups>{let mut at=0;let n=codec::get_varint(r,&mut at)? as usize;let mut g=0u32;let mut out=Vec::with_capacity(n);
 for _ in 0..n{g=g.wrapping_add(codec::get_varint(r,&mut at)? as u32);let b:[u8;crate::world_state::FLAG_BLOCK]=r.get(at..at+crate::world_state::FLAG_BLOCK)?.try_into().ok()?;at+=crate::world_state::FLAG_BLOCK;out.push((g,b));}
 (at==r.len()).then_some(out)}
fn encode_events(v:&[FlagEvent])->Vec<u8>{let mut o=Vec::new();let mut t=0u64;for e in v{codec::put_varint(&mut o,e.time.wrapping_sub(t));t=e.time;codec::put_varint(&mut o,e.flag as u64);o.push(e.state as u8);}o}
fn decode_events(r:&[u8],count:usize)->Option<Vec<FlagEvent>>{let mut at=0;let mut t=0u64;let mut out=Vec::with_capacity(count);
 for _ in 0..count{t=t.wrapping_add(codec::get_varint(r,&mut at)?);let flag=codec::get_varint(r,&mut at)? as u32;let s=*r.get(at)?;at+=1;out.push(FlagEvent{time:t,flag,state:s!=0});}(at==r.len()).then_some(out)}
#[derive(Default)]struct PlayerEncoder{bones:Vec<QBone>,model:Vec<QBone>,floats:Vec<u32>,ints:Vec<u32>,time:u64}
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
  codec::encode_pose(&f.local,&mut self.bones,out);codec::encode_pose(&f.model,&mut self.model,out);}
}
fn decode_player(raw:&[u8],count:usize)->Option<Vec<PlayerFrame>>{let mut at=0;decode_player_at(raw,&mut at,count,None)}
// Decodes `count` frames; with `hp`, an HP pair follows each frame (actor chunks).
fn decode_player_at(raw:&[u8],at:&mut usize,count:usize,mut hp:Option<&mut Vec<(i32,i32)>>)->Option<Vec<PlayerFrame>>{
 let mut hp_prev=(0u32,0u32);let at_ref=at;let mut at=*at_ref;let (mut bones,mut model,mut floats,mut ints,mut time)=(Vec::new(),Vec::new(),Vec::new(),vec![0u32;2+crate::equipment::BYTES/4],0u64);
 let mut out=Vec::with_capacity(count);
 for _ in 0..count{
  time=time.wrapping_add(codec::get_varint(raw,&mut at)?);
  let mut f=[0f32;12+16+4];codec::decode_f32s(raw,&mut at,&mut floats,&mut f)?;
  for p in ints.iter_mut(){*p^=codec::get_varint(raw,&mut at)? as u32;}
  let n=codec::get_varint(raw,&mut at)? as usize;if n==0||n>4096{return None;}
  let mut local=vec![0u8;n*codec::QS];let mut model_pose=vec![0u8;n*codec::QS];
  codec::decode_pose(raw,&mut at,n,&mut bones,&mut local)?;codec::decode_pose(raw,&mut at,n,&mut model,&mut model_pose)?;
  let e=&ints[2..];let equip=crate::equipment::Equip{arm_style:e[0],slots:std::array::from_fn(|k|e[1+k]),handles:std::array::from_fn(|k|e[7+k]),params:std::array::from_fn(|k|e[7+crate::equipment::SLOTS+k] as i32)};
  out.push(PlayerFrame{time,transform:std::array::from_fn(|r|std::array::from_fn(|c|f[r*4+c])),matrix:std::array::from_fn(|i|f[12+i]),local,model:model_pose,
   place:crate::arrival::Place{block:ints[0] as i32,origin:ints[1] as i32,global:[f[28],f[29],f[30],f[31]]},equip});
  if let Some(h)=hp.as_deref_mut(){hp_prev.0^=codec::get_varint(raw,&mut at)? as u32;hp_prev.1^=codec::get_varint(raw,&mut at)? as u32;h.push((hp_prev.0 as i32,hp_prev.1 as i32));}}
 *at_ref=at;(at==raw.len()).then_some(out)}
fn encode_actor(frames:&[ActorFrame])->Vec<u8>{let mut enc=PlayerEncoder::default();let mut raw=Vec::new();let mut hp=(0u32,0u32);
 for f in frames{enc.encode(&f.body,&mut raw);codec::put_varint(&mut raw,((f.hp as u32)^hp.0) as u64);codec::put_varint(&mut raw,((f.max_hp as u32)^hp.1) as u64);hp=(f.hp as u32,f.max_hp as u32);}raw}
fn decode_actor(raw:&[u8],count:usize)->Option<Vec<ActorFrame>>{let mut at=0;let mut hp=Vec::with_capacity(count);
 let bodies=decode_player_at(raw,&mut at,count,Some(&mut hp))?;Some(bodies.into_iter().zip(hp).map(|(body,(hp,max_hp))|ActorFrame{body,hp,max_hp}).collect())}
fn encode_infos(v:&[ActorInfo])->Vec<u8>{let mut o=Vec::new();for a in v{for x in [a.id as u64,a.handle,a.entity as u64,a.npc_param as u32 as u64,a.chr_type as u64,a.first_seen]{codec::put_varint(&mut o,x);}}o}
fn decode_infos(r:&[u8],count:usize)->Option<Vec<ActorInfo>>{let mut at=0;let mut out=Vec::with_capacity(count);
 for _ in 0..count{let mut g=||codec::get_varint(r,&mut at);let (id,handle,entity,npc,kind,first)=(g()?,g()?,g()?,g()?,g()?,g()?);
  out.push(ActorInfo{id:id as u32,handle,entity:entity as u32,npc_param:npc as u32 as i32,chr_type:kind as u32,first_seen:first});}(at==r.len()).then_some(out)}

fn write_chunk(w:&mut impl Write,track:u32,kind:u32,count:u32,first:u64,last:u64,raw:&[u8])->std::io::Result<u64>{
 let packed=codec::pack_zeros(raw);
 let mut h=Vec::with_capacity(CHUNK_HEADER);
 for v in [CHUNK,track,kind,count]{h.extend_from_slice(&v.to_le_bytes());}h.extend_from_slice(&first.to_le_bytes());h.extend_from_slice(&last.to_le_bytes());
 for v in [packed.len() as u32,raw.len() as u32,codec::crc32(&packed)]{h.extend_from_slice(&v.to_le_bytes());}
 w.write_all(&h)?;w.write_all(&packed)?;Ok((h.len()+packed.len()) as u64)}

// ------------------------------------------------------------------------------------------ writer
pub enum Message{Player(PlayerFrame),World(WorldSample),Flags(u64,FlagGroups),FlagEvents(Vec<FlagEvent>),ActorInfo(ActorInfo),Actors(Vec<(u32,ActorFrame)>),Finish}
/// Starts a writer thread for `final_path`; returns the sender the game thread uses.
pub fn start_writer(final_path:PathBuf)->std::io::Result<SyncSender<Message>>{
 let tmp=PathBuf::from(format!("{}.tmp",final_path.display()));
 let mut file=std::io::BufWriter::new(std::fs::File::create(&tmp)?);
 file.write_all(MAGIC)?;file.write_all(&VERSION.to_le_bytes())?;file.write_all(&0u32.to_le_bytes())?;
 // ~2 s of frames can queue; if the disk stalls longer, frames are dropped and counted (no game stall).
 let (tx,rx)=std::sync::mpsc::sync_channel::<Message>(240);
 std::thread::Builder::new().name("TheaterMode.WorldWriter".into()).spawn(move||writer(rx,file,tmp,final_path))?;
 Ok(tx)}
fn writer(rx:Receiver<Message>,mut file:std::io::BufWriter<std::fs::File>,tmp:PathBuf,final_path:PathBuf){
 let mut player:Vec<PlayerFrame>=Vec::new();let mut bytes=16u64;let mut frames=0u64;let mut failed=None;
 let (mut world,mut events):(Vec<WorldSample>,Vec<FlagEvent>)=(Vec::new(),Vec::new());let mut flag_changes=0u64;
 let mut actors:HashMap<u32,Vec<ActorFrame>>=HashMap::new();let mut infos:Vec<ActorInfo>=Vec::new();let mut actor_frames=0u64;
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
  let r=match m{
   Message::Player(f)=>{frames+=1;let r=if player.first().is_some_and(|p|f.time.saturating_sub(p.time)>=CHUNK_NS){flush(&mut player,&mut file)}else{Ok(0)};player.push(f);r}
   Message::World(w)=>{world.push(w);if world.len()>=60{flush_small(&mut world,&mut events,&mut file)}else{Ok(0)}}
   Message::Flags(t,g)=>write_chunk(&mut file,TRACK_FLAGS,KIND_FLAGS_FULL,g.len() as u32,t,t,&encode_flags(&g)),
   Message::FlagEvents(e)=>{flag_changes+=e.len() as u64;events.extend(e);if events.len()>=4096{flush_small(&mut world,&mut events,&mut file)}else{Ok(0)}}
   Message::ActorInfo(a)=>{infos.push(a);if infos.len()>=64{let r=write_chunk(&mut file,TRACK_ACTORS,KIND_ACTOR_INFO,infos.len() as u32,a.first_seen,a.first_seen,&encode_infos(&infos));infos.clear();r}else{Ok(0)}}
   Message::Actors(list)=>{let mut total=Ok(0);for (id,f) in list{actor_frames+=1;let buf=actors.entry(id).or_default();
     if buf.first().is_some_and(|p|f.body.time.saturating_sub(p.body.time)>=CHUNK_NS){match flush_actor(id,buf,&mut file){Ok(n)=>{if let Ok(t)=&mut total{*t+=n;}},Err(e)=>total=Err(e)}}buf.push(f);}total}
   Message::Finish=>break};
  match r{Ok(n)=>bytes+=n,Err(e)=>{failed.get_or_insert(e.to_string());}}}
 if failed.is_none(){match flush(&mut player,&mut file).and_then(|n|{bytes+=n;bytes+=flush_small(&mut world,&mut events,&mut file)?;
   if !infos.is_empty(){bytes+=write_chunk(&mut file,TRACK_ACTORS,KIND_ACTOR_INFO,infos.len() as u32,infos[0].first_seen,infos[0].first_seen,&encode_infos(&infos))?;}
   for (id,buf) in actors.iter_mut(){bytes+=flush_actor(*id,buf,&mut file)?;}
   file.flush()?;file.get_ref().sync_all()?;Ok(())}){Ok(())=>{},Err(e)=>failed=Some(e.to_string())}}
 drop(file);
 match failed{
  None=>match std::fs::rename(&tmp,&final_path){
   Ok(())=>{crate::log_game(&format!("WORLD_FILE: saved {} ({frames} player frames, {} actors / {actor_frames} actor frames, {flag_changes} flag changes, {:.1} MB, {:.2} MB per minute)",final_path.display(),actors.len(),bytes as f64/1_048_576.0,bytes as f64/1_048_576.0/((frames.max(1)) as f64/3600.0)));crate::bone_replay::status("BONE REPLAY: recording saved");}
   Err(e)=>{crate::log_game(&format!("WORLD_FILE_ERROR: could not rename {}: {e}",tmp.display()));crate::bone_replay::status("BONE REPLAY ERROR: could not save the recording (see log)");}},
  Some(e)=>{crate::log_game(&format!("WORLD_FILE_ERROR: writing {} failed: {e}; the partial file stays as .tmp",tmp.display()));crate::bone_replay::status("BONE REPLAY ERROR: could not save the recording (see log)");}}}

// ------------------------------------------------------------------------------------------ reader
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
pub struct WorldFile{pub player:PlayerTrack,pub world:Vec<WorldSample>,pub flags_start:Option<(u64,FlagGroups)>,pub flag_events:Vec<FlagEvent>,pub actors:Vec<(ActorInfo,ActorTrack)>}
/// Opens a world file and indexes its chunks (every chunk's integrity is checked once here).
pub fn open(path:&Path)->Result<WorldFile,String>{
 let data=std::fs::read(path).map_err(|e|e.to_string())?;
 if data.len()<16||&data[0..8]!=MAGIC{return Err("not a Theater Mode world file".into());}
 let version=u32::from_le_bytes(data[8..12].try_into().unwrap());if version!=VERSION{return Err(format!("unsupported world file version {version}"));}
 let (mut at,mut chunks,mut times,mut skipped)=(16usize,Vec::new(),Vec::new(),HashMap::<u32,usize>::new());
 let (mut world,mut flags_start,mut flag_events)=(Vec::new(),None,Vec::new());
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
   let frames=codec::unpack_zeros(&data[body..body+packed]).and_then(|r|decode_player(&r,count)).ok_or(format!("chunk at byte {at} does not decode"))?;
   times.extend(frames.iter().map(|f|f.time));}
  else if matches!((track,kind),(TRACK_WORLD,KIND_WORLD)|(TRACK_FLAGS,KIND_FLAGS_FULL)|(TRACK_FLAGS,KIND_FLAG_EVENTS)){
   let raw=codec::unpack_zeros(&data[body..body+packed]).ok_or(format!("chunk at byte {at} does not unpack"))?;
   let bad=||format!("chunk at byte {at} does not decode");
   match kind{KIND_WORLD=>world.extend(decode_world(&raw,count).ok_or_else(bad)?),
    KIND_FLAGS_FULL=>{if flags_start.is_none(){flags_start=Some((u64::from_le_bytes(data[at+16..at+24].try_into().unwrap()),decode_flags(&raw).ok_or_else(bad)?));}}
    _=>flag_events.extend(decode_events(&raw,count).ok_or_else(bad)?)}}
  else if track==TRACK_ACTORS&&kind==KIND_ACTOR_INFO{
   let raw=codec::unpack_zeros(&data[body..body+packed]).ok_or(format!("chunk at byte {at} does not unpack"))?;
   infos.extend(decode_infos(&raw,count).ok_or(format!("chunk at byte {at} does not decode"))?);}
  else if track>=TRACK_ACTOR_BASE&&kind==KIND_ACTOR{
   let e=actor_chunks.entry(track-TRACK_ACTOR_BASE).or_default();let first_index=e.1.len();e.0.push(ChunkRef{offset:body,packed,raw,count,first_index});
   let frames=codec::unpack_zeros(&data[body..body+packed]).and_then(|r|decode_actor(&r,count)).ok_or(format!("chunk at byte {at} does not decode"))?;
   e.1.extend(frames.iter().map(|f|f.body.time));}
  else{*skipped.entry(track).or_default()+=1;}
  at=body+packed;}
 if times.is_empty(){return Err("the world file has no player frames".into());}
 if times.windows(2).any(|w|w[1]<w[0]){return Err("frame times go backwards".into());}
 if !skipped.is_empty(){crate::log_game(&format!("WORLD_FILE: tracks this build does not use yet: {skipped:?}"));}
 let data=Arc::new(data);
 let mut actors=Vec::new();
 for info in infos{if let Some((chunks,times))=actor_chunks.remove(&info.id){if times.windows(2).all(|w|w[1]>=w[0]){actors.push((info,Track{data:data.clone(),chunks,times,cache:Vec::new(),decode:decode_actor}));}}}
 Ok(WorldFile{player:Track{data,chunks,times,cache:Vec::new(),decode:decode_player},world,flags_start,flag_events,actors})}

#[cfg(test)]mod tests{
 use super::*;
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
  tx.send(Message::ActorInfo(ActorInfo{id:3,handle:77,entity:1043360200,npc_param:-5,chr_type:0,first_seen:1000})).unwrap();
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
   for (a,b) in f.local.chunks_exact(4).zip(e.local.chunks_exact(4)){let (a,b)=(f32::from_le_bytes(a.try_into().unwrap()),f32::from_le_bytes(b.try_into().unwrap()));assert!((a-b).abs()<2e-5);}}
  let size=std::fs::metadata(&path).unwrap().len();assert!(size<290*3000,"{size} bytes for 290 frames (200 player + 90 actor)");
  std::fs::remove_dir_all(&dir).ok();}
}

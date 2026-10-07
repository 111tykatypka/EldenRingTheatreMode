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
use std::sync::mpsc::{Receiver,SyncSender};

pub const MAGIC:&[u8;8]=b"ERWORLD1";
pub const VERSION:u32=1;
const CHUNK:u32=u32::from_le_bytes(*b"CHNK");
const CHUNK_HEADER:usize=4*4+8*2+4*3;
const CHUNK_NS:u64=1_000_000_000;
pub const TRACK_PLAYER:u32=1;
pub const KIND_PLAYER:u32=1;

/// One recorded frame of the player (see bone_replay.rs for the meaning of each field).
#[derive(Clone)]
pub struct PlayerFrame{pub time:u64,pub transform:[[f32;4];3],pub matrix:[f32;16],pub local:Vec<u8>,pub model:Vec<u8>,pub place:crate::arrival::Place,pub equip:crate::equipment::Equip}

// ---------------------------------------------------------------------------------------- encoding
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
fn decode_player(raw:&[u8],count:usize)->Option<Vec<PlayerFrame>>{
 let mut at=0;let (mut bones,mut model,mut floats,mut ints,mut time)=(Vec::new(),Vec::new(),Vec::new(),vec![0u32;2+crate::equipment::BYTES/4],0u64);
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
   place:crate::arrival::Place{block:ints[0] as i32,origin:ints[1] as i32,global:[f[28],f[29],f[30],f[31]]},equip});}
 (at==raw.len()).then_some(out)}

fn write_chunk(w:&mut impl Write,track:u32,kind:u32,count:u32,first:u64,last:u64,raw:&[u8])->std::io::Result<u64>{
 let packed=codec::pack_zeros(raw);
 let mut h=Vec::with_capacity(CHUNK_HEADER);
 for v in [CHUNK,track,kind,count]{h.extend_from_slice(&v.to_le_bytes());}h.extend_from_slice(&first.to_le_bytes());h.extend_from_slice(&last.to_le_bytes());
 for v in [packed.len() as u32,raw.len() as u32,codec::crc32(&packed)]{h.extend_from_slice(&v.to_le_bytes());}
 w.write_all(&h)?;w.write_all(&packed)?;Ok((h.len()+packed.len()) as u64)}

// ------------------------------------------------------------------------------------------ writer
pub enum Message{Player(PlayerFrame),Finish}
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
 let flush=|frames_buf:&mut Vec<PlayerFrame>,file:&mut std::io::BufWriter<std::fs::File>|->std::io::Result<u64>{
  if frames_buf.is_empty(){return Ok(0);}
  let mut enc=PlayerEncoder::default();let mut raw=Vec::new();for f in frames_buf.iter(){enc.encode(f,&mut raw);}
  let n=write_chunk(file,TRACK_PLAYER,KIND_PLAYER,frames_buf.len() as u32,frames_buf[0].time,frames_buf.last().unwrap().time,&raw)?;frames_buf.clear();Ok(n)};
 for m in rx{
  match m{
   Message::Player(f)=>{frames+=1;if player.first().is_some_and(|p|f.time.saturating_sub(p.time)>=CHUNK_NS){match flush(&mut player,&mut file){Ok(n)=>bytes+=n,Err(e)=>{failed.get_or_insert(e.to_string());}}}player.push(f);}
   Message::Finish=>break}}
 if failed.is_none(){match flush(&mut player,&mut file).and_then(|n|{bytes+=n;file.flush()?;file.get_ref().sync_all()?;Ok(())}){Ok(())=>{},Err(e)=>failed=Some(e.to_string())}}
 drop(file);
 match failed{
  None=>match std::fs::rename(&tmp,&final_path){
   Ok(())=>{crate::log_game(&format!("WORLD_FILE: saved {} ({frames} player frames, {:.1} MB, {:.0} bytes per frame)",final_path.display(),bytes as f64/1_048_576.0,bytes as f64/(frames.max(1)) as f64));crate::bone_replay::status("BONE REPLAY: recording saved");}
   Err(e)=>{crate::log_game(&format!("WORLD_FILE_ERROR: could not rename {}: {e}",tmp.display()));crate::bone_replay::status("BONE REPLAY ERROR: could not save the recording (see log)");}},
  Some(e)=>{crate::log_game(&format!("WORLD_FILE_ERROR: writing {} failed: {e}; the partial file stays as .tmp",tmp.display()));crate::bone_replay::status("BONE REPLAY ERROR: could not save the recording (see log)");}}}

// ------------------------------------------------------------------------------------------ reader
#[derive(Clone,Copy)]struct ChunkRef{offset:usize,packed:usize,raw:usize,count:usize,first_index:usize}
/// Player frames of a world file: all frame times up front (for seeking), frames decoded one chunk at
/// a time and cached, so playback at any speed decodes each chunk once.
pub struct PlayerTrack{data:Vec<u8>,chunks:Vec<ChunkRef>,pub times:Vec<u64>,cache:Vec<(usize,Vec<PlayerFrame>)>}
impl PlayerTrack{
 pub fn len(&self)->usize{self.times.len()}
 pub fn get(&mut self,i:usize)->Option<&PlayerFrame>{
  let c=self.chunks.partition_point(|c|c.first_index<=i).checked_sub(1)?;
  if !self.cache.iter().any(|(k,_)|*k==c){
   let r=self.chunks[c];let packed=&self.data[r.offset..r.offset+r.packed];
   let raw=codec::unpack_zeros(packed)?;if raw.len()!=r.raw{return None;}
   let frames=decode_player(&raw,r.count)?;
   if self.cache.len()>=4{self.cache.remove(0);}self.cache.push((c,frames));}
  let (_,frames)=self.cache.iter().find(|(k,_)|*k==c)?;frames.get(i-self.chunks[c].first_index)}
}
/// Opens a world file and indexes its chunks (every chunk's integrity is checked once here).
pub fn open(path:&Path)->Result<PlayerTrack,String>{
 let data=std::fs::read(path).map_err(|e|e.to_string())?;
 if data.len()<16||&data[0..8]!=MAGIC{return Err("not a Theater Mode world file".into());}
 let version=u32::from_le_bytes(data[8..12].try_into().unwrap());if version!=VERSION{return Err(format!("unsupported world file version {version}"));}
 let (mut at,mut chunks,mut times,mut skipped)=(16usize,Vec::new(),Vec::new(),HashMap::<u32,usize>::new());
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
  else{*skipped.entry(track).or_default()+=1;}
  at=body+packed;}
 if times.is_empty(){return Err("the world file has no player frames".into());}
 if times.windows(2).any(|w|w[1]<w[0]){return Err("frame times go backwards".into());}
 if !skipped.is_empty(){crate::log_game(&format!("WORLD_FILE: tracks this build does not use yet: {skipped:?}"));}
 Ok(PlayerTrack{data,chunks,times,cache:Vec::new()})}

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
  let tx=start_writer(path.clone()).unwrap();for i in 0..200{tx.send(Message::Player(frame(i))).unwrap();}tx.send(Message::Finish).unwrap();drop(tx);
  for _ in 0..200{if path.exists(){break;}std::thread::sleep(std::time::Duration::from_millis(10));}
  let mut t=open(&path).unwrap();assert_eq!(t.len(),200);assert!(t.chunks.len()>=3);
  for i in [0usize,59,60,61,150,199]{let f=t.get(i).unwrap().clone();let e=frame(i as u64);
   assert_eq!(f.time,e.time);assert_eq!(f.transform,e.transform);assert_eq!(f.place,e.place);assert_eq!(f.equip,e.equip);
   for (a,b) in f.local.chunks_exact(4).zip(e.local.chunks_exact(4)){let (a,b)=(f32::from_le_bytes(a.try_into().unwrap()),f32::from_le_bytes(b.try_into().unwrap()));assert!((a-b).abs()<2e-5);}}
  let size=std::fs::metadata(&path).unwrap().len();assert!(size<200*3000,"{size} bytes for 200 frames");
  std::fs::remove_dir_all(&dir).ok();}
}

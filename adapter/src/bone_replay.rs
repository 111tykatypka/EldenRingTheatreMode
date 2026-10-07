// Replay system: record the player's bone transforms and replay them exactly on the player
// character. User decisions (2026-10-07): record only bone transforms, local and world space, no
// animation data (the game's animation is not deterministic); the native ghost is retired; the
// replay body is the player character, with controls locked while the replay owns it.
//
// One record system: F5 (the host recorder) records. While the host records, this module samples
// the bones every drawn frame and, when the recording stops, writes "<replay>.erplay.bones" beside
// the host's file (the library already moves/recycles "<file>.<anything>" sidecars with it). When a
// replay with bones is loaded, the body follows the host timeline: play, pause and scrubbing.
// The host and overlay state arrives through tm_overlay_bone_link (theater_ui snapshot v5).
//
// Where the pose lives (skeleton probe K1, write tests K2-K4): ChrIns+0x398 is the
// CSFD4LocationHkaPoseImporter; +0x50 -> 150 local-space hkQsTransforms, +0x60 -> 150 model-space
// hkQsTransforms (48 bytes each). Writes in ChrIns_PrePhysicsSafe survive until Draw_Pre and are what
// is drawn; writes in ChrIns_BehaviorSafe are overwritten; writes after the importer show nothing.
// World placement comes from CSChrPhysicsModule orientation, interpolated_orientation, position; the
// proxy-move request keeps the physics step from pulling the body back to last frame's spot.
// Crash history: ChrIns::debug_flags has the wrong offset in this SDK pin for game 2.7.0.0 (writing it
// corrupted a pointer). Every byte flag written here is checked to read as a bool first.
//
// Frame flow (task groups registered in lib.rs):
//   0 ChrIns_PostPhysics: link state, ownership changes; while owning, rewrites the transform after physics
//   2 ChrIns_PrePhysicsSafe: while owning, writes bones and transform
//   4 Draw_Pre: while recording, samples what is about to be drawn; while owning, measures accuracy
// Offsets stay in this file until they move into GameProfile.
use std::ffi::{c_char,CStr,CString};
use std::io::{Read,Write};
use std::path::PathBuf;
use std::sync::{Arc,Mutex};
use eldenring::cs::{WorldChrMan,ChrIns,CSChrPhysicsModule};
use fromsoftware_shared::FromStatic;

const POSE_IMPORTER:usize=0x398;
const LOCAL_POSE:usize=0x50;
const MODEL_POSE:usize=0x60;
const BONES:usize=150;
const POSE_BYTES:usize=BONES*48;
const MAX_SECONDS:u64=600; // about 52 MB per minute uncompressed
const KEYS_GROUP:usize=0;
const WRITE_GROUP:usize=2;
const DRAW_GROUP:usize=4;
const RESTORE_FRAMES:u32=10;
const MAGIC:&[u8;8]=b"ERBONES1";
const FILE_VERSION:u32=1;
const FRAME_BYTES:usize=8+48+64+POSE_BYTES*2;
// theater_ui::RecordingState
const RECORD_RECORDING:u32=1;
const RECORD_PAUSED:u32=2;

#[repr(C)]struct Link{linked:u32,recording:u32,loaded:u32,playing:u32,overlay_shown:u32,reserved:u32,timescale:f64,play_source_ns:u64,received_ns:u64,recording_path:[c_char;260],loaded_path:[c_char;260]}
unsafe extern "C"{fn tm_render_native_status(text:*const c_char);fn tm_render_lock_game_input(locked:i32);fn tm_overlay_bone_link(out:*mut Link);}

type Transform=[[f32;4];3]; // orientation, interpolated_orientation, position
struct Frame{time:u64,transform:Transform,matrix:[f32;16],local:Vec<u8>,model:Vec<u8>}
#[derive(Default)]struct Accuracy{frames:u64,exact_bones:u64,max_drawn_cm:f32,sum_drawn_cm:f64}
struct Loaded{path:String,frames:Arc<Vec<Frame>>}
#[derive(PartialEq)]enum Loading{None,Busy(String),Missing(String),Failed(String)}
struct State{
 recording:Option<(String,Vec<Frame>)>,record_paused:bool,last_second:u64,
 loaded:Option<Loaded>,loading:Loading,
 owning:bool,restore_left:u32,saved:Option<Transform>,gravity_saved:Option<bool>,written:Option<usize>,evaluated_root:Option<Transform>,pose_alpha:f64,local_out:[u8;POSE_BYTES],model_out:[u8;POSE_BYTES],accuracy:Accuracy,
}
static STATE:Mutex<State>=Mutex::new(State{recording:None,record_paused:false,last_second:0,loaded:None,loading:Loading::None,owning:false,restore_left:0,saved:None,gravity_saved:None,written:None,evaluated_root:None,pose_alpha:0.0,local_out:[0;POSE_BYTES],model_out:[0;POSE_BYTES],accuracy:Accuracy{frames:0,exact_bones:0,max_drawn_cm:0.0,sum_drawn_cm:0.0}});
// Background load results land here and are adopted on the next tick.
static LOADED:Mutex<Option<Result<Loaded,(String,String)>>>=Mutex::new(None);

pub(crate) fn status(text:&str){if let Ok(c)=CString::new(text){unsafe{tm_render_native_status(c.as_ptr())}}}
fn text(c:&[c_char;260])->String{unsafe{CStr::from_ptr(c.as_ptr())}.to_string_lossy().into_owned()}
fn link()->Link{let mut l:Link=unsafe{std::mem::zeroed()};unsafe{tm_overlay_bone_link(&mut l)};l}
fn read_ptr(address:usize)->usize{if address<0x10000{return 0;}unsafe{std::ptr::read_volatile(address as *const usize)}}
fn array(p:usize)->bool{(0x10000000000..0x800000000000).contains(&p)&&p%16==0}
fn object(p:usize)->bool{(0x10000000000..0x800000000000).contains(&p)&&p%8==0}
fn player_chr()->Option<usize>{let world=unsafe{WorldChrMan::instance()}.ok()?;let player=world.main_player.as_ref()?;Some(&player.chr_ins as *const _ as usize)}
fn physics(chr:usize)->Option<usize>{let p=unsafe{&*(chr as *const ChrIns)}.modules.physics.as_ref() as *const CSChrPhysicsModule as usize;object(p).then_some(p)}
fn transform_fields(p:usize)->[usize;3]{let m=p as *const CSChrPhysicsModule;unsafe{[&raw const (*m).orientation as usize,&raw const (*m).interpolated_orientation as usize,&raw const (*m).position as usize]}}
fn read_transform(chr:usize)->Option<Transform>{let p=physics(chr)?;Some(transform_fields(p).map(|a|unsafe{std::ptr::read_volatile(a as *const [f32;4])}))}
fn write_transform(chr:usize,t:&Transform){if let Some(p)=physics(chr){for(a,v)in transform_fields(p).iter().zip(t){unsafe{std::ptr::write_volatile(*a as *mut [f32;4],*v)}}}}
fn ctrl(chr:usize)->usize{unsafe{&*(*(chr as *const ChrIns)).chr_ctrl as *const eldenring::cs::ChrCtrl as usize}}
fn matrix_address(chr:usize)->usize{let c=ctrl(chr) as *const eldenring::cs::ChrCtrl;unsafe{&raw const (*c).model_matrix as usize}}
fn read_matrix(a:usize)->[f32;16]{unsafe{std::ptr::read_volatile(a as *const [f32;16])}}
// Byte flags in CSChrPhysicsModule, only written when they read as a bool (0 or 1).
fn bool_flag(chr:usize,pick:fn(*const CSChrPhysicsModule)->usize)->Option<usize>{let p=physics(chr)?;let a=pick(p as *const CSChrPhysicsModule);(unsafe{std::ptr::read_volatile(a as *const u8)}<=1).then_some(a)}
fn gravity_flag(chr:usize)->Option<usize>{bool_flag(chr,|m|unsafe{&raw const (*m).gravity_disabled as usize})}
fn proxy_flag(chr:usize)->Option<usize>{bool_flag(chr,|m|unsafe{&raw const (*m).chr_proxy_pos_update_requested as usize})}
fn set_flag(a:Option<usize>,on:bool){if let Some(a)=a{unsafe{std::ptr::write_volatile(a as *mut u8,on as u8)}}}
fn pose_arrays(chr:usize)->Option<(usize,usize)>{
 let importer=read_ptr(chr+POSE_IMPORTER);if !object(importer){return None;}
 let (local,model)=(read_ptr(importer+LOCAL_POSE),read_ptr(importer+MODEL_POSE));
 (array(local)&&array(model)).then_some((local,model))}
fn pose(p:usize)->&'static [u8]{unsafe{std::slice::from_raw_parts(p as *const u8,POSE_BYTES)}}
fn bones_path(replay:&str)->PathBuf{PathBuf::from(format!("{replay}.bones"))}

// File: "ERBONES1", u32 version, u32 bone count, u32 frame bytes, u32 reserved, u64 frame count,
// then per frame: u64 source time (game monotonic ns), 12 f32 transform, 16 f32 model matrix,
// local pose, model pose. Written to .tmp and renamed, so a crash never leaves a half file.
fn save(path:PathBuf,frames:Vec<Frame>)->std::io::Result<u64>{
 let tmp=path.with_extension("bones.tmp");
 {let mut f=std::io::BufWriter::new(std::fs::File::create(&tmp)?);
  f.write_all(MAGIC)?;for v in [FILE_VERSION,BONES as u32,FRAME_BYTES as u32,0]{f.write_all(&v.to_le_bytes())?;}f.write_all(&(frames.len() as u64).to_le_bytes())?;
  for fr in &frames{f.write_all(&fr.time.to_le_bytes())?;for v in fr.transform.iter().flatten().chain(fr.matrix.iter()){f.write_all(&v.to_le_bytes())?;}f.write_all(&fr.local)?;f.write_all(&fr.model)?;}
  f.flush()?;f.get_ref().sync_all()?;}
 std::fs::rename(&tmp,&path)?;Ok(std::fs::metadata(&path)?.len())}
fn load(path:&PathBuf)->Result<Vec<Frame>,String>{
 let mut data=Vec::new();std::fs::File::open(path).and_then(|mut f|f.read_to_end(&mut data)).map_err(|e|e.to_string())?;
 if data.len()<32||&data[0..8]!=MAGIC{return Err("not a bone replay file".into());}
 let u32_at=|i:usize|u32::from_le_bytes(data[i..i+4].try_into().unwrap());
 if u32_at(8)!=FILE_VERSION||u32_at(12)!=BONES as u32||u32_at(16)!=FRAME_BYTES as u32{return Err(format!("unsupported version {} / bones {} / frame size {}",u32_at(8),u32_at(12),u32_at(16)));}
 let count=u64::from_le_bytes(data[24..32].try_into().unwrap()) as usize;
 if count==0||data.len()!=32+count*FRAME_BYTES{return Err(format!("size {} does not match {count} frames",data.len()));}
 let f32_at=|i:usize|f32::from_le_bytes(data[i..i+4].try_into().unwrap());
 let frames=(0..count).map(|k|{let o=32+k*FRAME_BYTES;
  let transform=std::array::from_fn(|r|std::array::from_fn(|c|f32_at(o+8+(r*4+c)*4)));
  let matrix=std::array::from_fn(|i|f32_at(o+56+i*4));
  Frame{time:u64::from_le_bytes(data[o..o+8].try_into().unwrap()),transform,matrix,local:data[o+120..o+120+POSE_BYTES].to_vec(),model:data[o+120+POSE_BYTES..o+FRAME_BYTES].to_vec()}}).collect::<Vec<_>>();
 if frames.windows(2).any(|w|w[1].time<w[0].time){return Err("frame times go backwards".into());}
 Ok(frames)}

fn finish_recording(s:&mut State){
 let Some((replay,frames))=s.recording.take() else {return;};
 let seconds=frames.last().zip(frames.first()).map(|(l,f)|(l.time-f.time) as f64/1e9).unwrap_or(0.0);
 crate::log_game(&format!("BONE_REPLAY: recording stopped; {} frames over {seconds:.2} s; saving {}",frames.len(),bones_path(&replay).display()));
 if frames.is_empty(){status("BONE REPLAY: no bones were recorded (load in first)");return;}
 let _=std::thread::Builder::new().name("TheaterMode.BoneSave".into()).spawn(move||{
  let path=bones_path(&replay);
  match save(path.clone(),frames){
   Ok(bytes)=>{crate::log_game(&format!("BONE_REPLAY: saved {} ({} MB)",path.display(),bytes/1_048_576));status("BONE REPLAY: bones saved with the recording");}
   Err(e)=>{crate::log_game(&format!("BONE_REPLAY_ERROR: could not save {}: {e}",path.display()));status("BONE REPLAY: could not save the bone data (see log)");}}});}

fn adopt_loaded(s:&mut State){
 let Some(result)=LOADED.lock().unwrap().take() else {return;};
 match result{
  Ok(l)=>{let current=matches!(&s.loading,Loading::Busy(p) if *p==l.path);if !current{return;}
   let seconds=l.frames.last().zip(l.frames.first()).map(|(a,b)|(a.time-b.time) as f64/1e9).unwrap_or(0.0);
   crate::log_game(&format!("BONE_REPLAY: loaded {} frames ({seconds:.2} s) for {}",l.frames.len(),l.path));
   status(&format!("BONE REPLAY: bones loaded ({seconds:.1} s). Play or scrub the timeline"));s.loading=Loading::None;s.loaded=Some(l);}
  Err((path,e))=>{if !matches!(&s.loading,Loading::Busy(p) if *p==path){return;}
   crate::log_game(&format!("BONE_REPLAY_ERROR: could not load bones for {path}: {e}"));status("BONE REPLAY: this replay's bone data could not be read (see log)");s.loading=Loading::Failed(path);}}}

// Which loaded replay the host has open; starts a background load when it changes.
fn follow_loaded(s:&mut State,l:&Link){
 let path=if l.loaded!=0{text(&l.loaded_path)}else{String::new()};
 let known=match &s.loading{Loading::Busy(p)|Loading::Missing(p)|Loading::Failed(p)=>Some(p.clone()),Loading::None=>s.loaded.as_ref().map(|x|x.path.clone())};
 if known.as_deref()==Some(path.as_str())||(path.is_empty()&&known.is_none()){return;}
 s.loaded=None;s.loading=Loading::None;s.written=None;s.evaluated_root=None;
 if path.is_empty(){LOADED.lock().unwrap().take();crate::log_game("BONE_REPLAY: unloaded pose data; pending loads invalidated");return;}
 let file=bones_path(&path);
 if !file.is_file(){crate::log_game(&format!("BONE_REPLAY: {path} has no bone data (recorded before bone replays)"));status("BONE REPLAY: this replay has no bone data. Record a new one with F5");s.loading=Loading::Missing(path);return;}
 s.loading=Loading::Busy(path.clone());status("BONE REPLAY: loading bones...");
 let _=std::thread::Builder::new().name("TheaterMode.BoneLoad".into()).spawn(move||{
  let result=load(&file).map(|frames|Loaded{path:path.clone(),frames:Arc::new(frames)}).map_err(|e|(path,e));*LOADED.lock().unwrap()=Some(result);});}

// The timeline position in source time, extrapolated between host snapshots while playing.
fn timeline(l:&Link,now:u64)->u64{
 if l.playing==0||l.received_ns==0{return l.play_source_ns;}
 let since=now.saturating_sub(l.received_ns).min(250_000_000) as f64;
 l.play_source_ns+(since*l.timescale.clamp(0.001,10.0)) as u64}

fn release(s:&mut State,chr:usize,reason:&str){
 if !s.owning{return;}s.owning=false;s.restore_left=RESTORE_FRAMES;
 let a=&s.accuracy;
 crate::log_game(&format!("BONE_REPLAY: released the body ({reason}); accuracy over {} frames: evaluated bones retained {:.1}%, drawn position error max {:.2} cm mean {:.3} cm",
  a.frames,100.0*a.exact_bones as f64/(a.frames.max(1) as f64),a.max_drawn_cm,a.sum_drawn_cm/(a.frames.max(1) as f64)));
 if let Some(t)=s.saved{write_transform(chr,&t);}}

pub fn tick(group:usize,now:u64){
 let mut guard=STATE.lock().unwrap();let s=&mut *guard;
 let chr=player_chr();
 if group==KEYS_GROUP{
  let l=link();
  // Player loss cannot leave the keyboard lock or a return transform armed for a new player.
  if chr.is_none()&&(s.owning||s.restore_left>0){
   s.owning=false;s.restore_left=0;s.saved=None;s.gravity_saved=None;s.written=None;s.evaluated_root=None;
   unsafe{tm_render_lock_game_input(0)};crate::log_game("BONE_REPLAY: player unavailable; ownership and input lock released");}
  // Recording follows the host recorder.
  let recording=l.linked!=0&&(l.recording==RECORD_RECORDING||l.recording==RECORD_PAUSED);
  if recording&&s.recording.is_none(){let path=text(&l.recording_path);
   if !path.is_empty(){crate::log_game(&format!("BONE_REPLAY: recording started for {path}"));s.recording=Some((path,Vec::new()));s.last_second=0;}}
  if !recording&&s.recording.is_some(){finish_recording(s);}
  s.record_paused=l.recording==RECORD_PAUSED;
  if let Some((_,frames))=&s.recording{if let (Some(f),Some(last))=(frames.first(),frames.last()){if last.time-f.time>MAX_SECONDS*1_000_000_000&&s.last_second!=u64::MAX{s.last_second=u64::MAX;crate::log_game("BONE_REPLAY: 10 minute bone limit reached; later frames are not recorded");status("BONE REPLAY: 10 minute limit reached, stop the recording (F6)");}}}
  // Playback follows the loaded replay and the timeline.
  adopt_loaded(s);follow_loaded(s,&l);
  let fresh=l.linked!=0&&l.received_ns!=0&&now.saturating_sub(l.received_ns)<500_000_000;
  let want=fresh&&s.loaded.is_some()&&s.recording.is_none()&&(l.playing!=0||l.overlay_shown!=0);
  if let Some(chr)=chr{
   if want&&!s.owning{
    s.saved=read_transform(chr);if s.saved.is_some(){
     s.owning=true;s.written=None;s.accuracy=Accuracy::default();
     let g=gravity_flag(chr);s.gravity_saved=g.map(|a|unsafe{std::ptr::read_volatile(a as *const u8)}==1);set_flag(g,true);
     unsafe{tm_render_lock_game_input(1)};crate::log_game("BONE_REPLAY: the replay owns the body; controls locked, return spot saved");}}
   else if !want&&s.owning{release(s,chr,if s.loaded.is_none(){"replay unloaded"}else{"timeline idle and overlay closed"});}
   if !s.owning&&s.restore_left>0{
    if let Some(t)=s.saved{write_transform(chr,&t);}set_flag(proxy_flag(chr),true);
    s.restore_left-=1;if s.restore_left==0{if let Some(g)=s.gravity_saved.take(){set_flag(gravity_flag(chr),g);}unsafe{tm_render_lock_game_input(0)};crate::log_game("BONE_REPLAY: returned to the saved spot; controls unlocked");}}
  }
  if s.owning{
   // Pick this game frame's sample once, from the timeline.
   let t=timeline(&l,now);
   if let Some(loaded)=&s.loaded{
    let frames=&loaded.frames;let i=frames.partition_point(|f|f.time<=t).saturating_sub(1).min(frames.len()-1);
    let a=&frames[i];let b=&frames[(i+1).min(frames.len()-1)];let span=b.time.saturating_sub(a.time);
    let alpha=if span==0||span>250_000_000{0.0}else{t.saturating_sub(a.time)as f64/span as f64};
    s.pose_alpha=alpha.clamp(0.0,1.0);s.written=Some(i);s.evaluated_root=crate::replay_interpolation::evaluate(&a.transform,&b.transform,alpha);
    if let (Some(chr),Some(root))=(chr,s.evaluated_root){write_transform(chr,&root);}
    else if let Some(chr)=chr{release(s,chr,"invalid root interpolation");}
   }}
  let fresh=l.linked!=0&&l.received_ns!=0&&now.saturating_sub(l.received_ns)<500_000_000;
  crate::timescale::update(fresh&&l.playing!=0&&s.owning&&chr.is_some()&&s.loaded.is_some()&&s.recording.is_none(),l.timescale,now);
  return;}
 let Some(chr)=chr else {return;};
 if group==DRAW_GROUP&&!s.record_paused&&s.last_second!=u64::MAX{
  if let Some((_,frames))=&mut s.recording{
   let (Some((local,model)),Some(transform))=(pose_arrays(chr),read_transform(chr)) else {return;};
   frames.push(Frame{time:now,transform,matrix:read_matrix(matrix_address(chr)),local:pose(local).to_vec(),model:pose(model).to_vec()});}}
 if !s.owning{return;}
 let (Some(i),Some(loaded))=(s.written,&s.loaded) else {return;};let f=&loaded.frames[i];
 match group{
  WRITE_GROUP=>{
   let Some((local,model))=pose_arrays(chr) else {return;};
   let next=&loaded.frames[(i+1).min(loaded.frames.len()-1)];
   let valid=crate::replay_interpolation::pose_into(&f.local,&next.local,s.pose_alpha,&mut s.local_out).is_some() &&
       crate::replay_interpolation::pose_into(&f.model,&next.model,s.pose_alpha,&mut s.model_out).is_some();
   if !valid{release(s,chr,"invalid bone interpolation");return;}
   unsafe{std::ptr::copy_nonoverlapping(s.local_out.as_ptr(),local as *mut u8,POSE_BYTES);std::ptr::copy_nonoverlapping(s.model_out.as_ptr(),model as *mut u8,POSE_BYTES);}
   if let Some(root)=s.evaluated_root{write_transform(chr,&root);}set_flag(proxy_flag(chr),true);set_flag(gravity_flag(chr),true);}
  DRAW_GROUP=>{
   let Some((local,model))=pose_arrays(chr) else {return;};let drawn=read_matrix(matrix_address(chr));
   let a=&mut s.accuracy;a.frames+=1;if pose(local)==&s.local_out[..]&&pose(model)==&s.model_out[..]{a.exact_bones+=1;}
   let d=(12..15).map(|k|(drawn[k]-f.matrix[k]).powi(2)).sum::<f32>().sqrt()*100.0;a.max_drawn_cm=a.max_drawn_cm.max(d);a.sum_drawn_cm+=d as f64;}
  _=>{}}
}

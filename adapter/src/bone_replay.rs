// Replay system: record the player's bone transforms and replay them exactly on the player
// character. User decisions (2026-10-07): record only bone transforms, local and world space, no
// animation data (the game's animation is not deterministic); the native ghost is retired; the
// replay body is the player character, with controls locked while the replay owns it.
//
// One record system: F5 (the host recorder) records. While the host records, this module samples
// the bones every drawn frame and streams them to "<replay>.erplay.world" (world_file.rs: compact,
// chunked, written by a background thread) beside the host's file (the library already moves and
// recycles "<file>.<anything>" sidecars with it). Older "<replay>.erplay.bones" files still play. When a
// replay with bones is loaded, the body follows the host timeline: play, pause and scrubbing.
// The host and overlay state arrives through tm_overlay_bone_link (theater_ui snapshot v11).
//
// Where the pose lives (skeleton probe K1, write tests K2-K4): ChrIns+0x398 is the
// CSFD4LocationHkaPoseImporter; +0x50 -> local-space hkQsTransforms, +0x60 -> model-space
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
// Offsets belong to GameProfile. New capture validates the runtime skeleton count and hierarchy.
use std::ffi::{c_char,CStr,CString};
use std::io::Read;
use std::path::PathBuf;
use std::sync::{Arc,Mutex};
use crate::weapon_loc;
use crate::effects;
use crate::camera_fade;
use std::sync::mpsc::SyncSender;
use crate::world_file::{self,Message};
use crate::world_state;
use crate::omission;
use eldenring::cs::{WorldChrMan,ChrIns,CSChrPhysicsModule};
use fromsoftware_shared::FromStatic;

// Bone interpolation between recorded frames: on by default (THEATER_POSE_INTERPOLATION=0 turns it off).
// It is pure math on our own buffers and only runs while a replay owns the body, so it cannot affect
// menus or normal play; without it slow motion steps at the recorded 60 Hz.
fn interpolated_pose_enabled()->bool {
 static ENABLED:std::sync::OnceLock<bool>=std::sync::OnceLock::new();
 *ENABLED.get_or_init(||std::env::var("THEATER_POSE_INTERPOLATION").as_deref()!=Ok("0"))
}
use crate::game_profile as profile;
use crate::arrival::{self,Place};
use crate::equipment::{self,Equip};
const POSE_IMPORTER:usize=profile::OFF_CHRINS_POSE_IMPORTER;
const LOCAL_POSE:usize=profile::OFF_POSE_IMPORTER_LOCAL;
const MODEL_POSE:usize=profile::OFF_POSE_IMPORTER_MODEL;
const BONES:usize=profile::VAL_PLAYER_BONES;
const POSE_BYTES:usize=BONES*48;
const KEYS_GROUP:usize=0;
const WRITE_GROUP:usize=2;
const DRAW_GROUP:usize=4;
/// Frames the return position is held (gravity still off) so the ground around the return spot can stream back
/// in before the body is released to physics; 4 s at 60 fps.
const RESTORE_FRAMES:u32=240;
/// Where the player really was before a replay sent them across the map by grace warp (place and body transform). Survives the
/// loading screens, which release the replay's ownership; used to bring the player back when the replay ends.
static ORIGIN:Mutex<Option<(Place,Transform)>>=Mutex::new(None);
static LAST_WARP_NS:std::sync::atomic::AtomicU64=std::sync::atomic::AtomicU64::new(0);
// Older sidecar format, read only: "<replay>.erplay.bones".
const MAGIC:&[u8;8]=b"ERBONES1";
const FRAME_BYTES_V1:usize=8+48+64+POSE_BYTES*2;
// Version 2 adds where the frame was: block id, origin block, global (chunk) position.
const FRAME_BYTES_V2:usize=FRAME_BYTES_V1+24;
// Version 3 adds the render equipment assembly (arm style, active slots, handles, param ids).
const FRAME_BYTES:usize=FRAME_BYTES_V2+equipment::BYTES;
// theater_ui::RecordingState
const RECORD_RECORDING:u32=1;
const RECORD_PAUSED:u32=2;

#[repr(C)]struct Link{linked:u32,recording:u32,loaded:u32,playing:u32,apply_requested:u32,options:u32,timescale:f64,play_source_ns:u64,received_ns:u64,recording_path:[c_char;260],loaded_path:[c_char;260]}
unsafe extern "C"{fn tm_render_event(text:*const c_char);fn tm_render_lock_game_input(locked:i32);fn tm_overlay_bone_link(out:*mut Link);}

type Transform=[[f32;4];3]; // orientation, interpolated_orientation, position
pub fn world_timing_tick(now:u64){
 unsafe extern "C"{fn tm_world_timing_tick(active:i32,speed:f64);}
 let (active,rate)=match STATE.try_lock(){Ok(s)=>(s.owning&&s.host.0&&!s.replay_blocked,s.host.1),Err(_)=>(false,1.0)};
 let mut link:Link=unsafe{std::mem::zeroed()};unsafe{tm_overlay_bone_link(&mut link)};
 let fresh=link.linked!=0&&link.loaded!=0&&link.recording!=RECORD_RECORDING&&link.recording!=RECORD_PAUSED&&now>=link.received_ns&&now-link.received_ns<250_000_000;
 unsafe{tm_world_timing_tick((active&&fresh&&crate::offline_allowed()&&player_chr().is_some()) as i32,rate);}
}
// place.block == -1: recorded before map data existed (bones file version 1); positions are used as recorded.
type Frame=world_file::PlayerFrame;
// Player frames of the loaded replay: the old .bones format is read whole, .world is decoded a chunk at
// a time (cached). Compressed file bytes and the timestamp index still reside in RAM.
enum Store{Memory(Vec<Frame>),Chunked(world_file::PlayerTrack)}
// World state of the loaded replay (Phase 2.1); empty for older files.
#[derive(Default)]struct WorldData{samples:Vec<world_file::WorldSample>,flags_start:Option<(u64,world_file::FlagGroups)>,events:Vec<world_file::FlagEvent>,touched:Vec<u32>,context:Vec<world_file::EntityContext>,skeletons:std::collections::HashMap<u32,crate::skeleton::Definition>,module:Vec<world_file::ModuleSample>,effects:Vec<world_file::EffectEvent>,effect_tracks:std::collections::HashMap<(u64,u32),Vec<(u64,[f32;16])>>}
impl Store{
 fn len(&self)->usize{match self{Store::Memory(v)=>v.len(),Store::Chunked(t)=>t.len()}}
 fn time(&self,i:usize)->u64{match self{Store::Memory(v)=>v[i].time,Store::Chunked(t)=>t.times[i]}}
 fn index_at(&self,t:u64)->usize{let n=self.len();let (mut lo,mut hi)=(0,n);while lo<hi{let m=(lo+hi)/2;if self.time(m)<=t{lo=m+1}else{hi=m}}lo.saturating_sub(1).min(n-1)}
 fn get(&mut self,i:usize)->Option<Frame>{match self{Store::Memory(v)=>v.get(i).cloned(),Store::Chunked(t)=>t.get(i).cloned()}}}
// The game thread hands frames to the world file writer thread; it never waits for the disk.
struct Recorder{path:String,tx:SyncSender<Message>,frames:u64,first:u64,last:u64,dropped:u64,next_world:u64,flags:world_file::FlagGroups,actors:crate::actors::Recorder,skeleton:Option<crate::skeleton::Definition>,skeleton_warned:bool,module:Option<[u8;weapon_loc::BYTES]>,probe:crate::item_probe::Probe}
#[derive(Default)]struct Accuracy{frames:u64,exact_bones:u64,max_drawn_cm:f32,sum_drawn_cm:f64}
struct Loaded{path:String,store:Store,parents:Arc<Vec<i16>>,seconds:f64,world:Arc<WorldData>,actors:Option<crate::actors::Player>,anchors:Arc<crate::replay_interpolation::AnchorTrack>}
#[derive(PartialEq)]enum Loading{None,Busy(String),Missing(String),Failed(String)}
struct State{
 recording:Option<Recorder>,record_paused:bool,last_second:u64,
 // The two recorded frames around this game frame's replay time (chosen once per frame by select()).
 cur:Option<(Frame,Frame)>,
 loaded:Option<Loaded>,loading:Loading,
 owning:bool,restore_left:u32,saved:Option<Transform>,gravity_saved:Option<bool>,written:Option<usize>,evaluated_root:Option<Transform>,pose_alpha:f64,local_out:Vec<u8>,model_out:Vec<u8>,accuracy:Accuracy,
 // Accuracy: where the body should be drawn this frame, and frames to skip after a start or seek.
 expected:[f32;3],settle:u32,last_t:u64,fallback_bones:u64,
 // Timeline as last seen from the host (playing, timescale, source time, when it arrived) and the
 // host monotonic anchor used to evaluate the same master clock between updates.
 host:(bool,f64,u64,u64),
 // Phase 1.4 arrival: how the body gets to the recorded place before the replay plays, and the
 // shift from recorded physics space to today's physics space for this frame.
 arrival:Arrival,warped:bool,shift:[f32;3],expected_root:Option<[f32;3]>,
 // Phase 1.5: the player's own equipment assembly while the replay owns the body, and how often the
 // game replaced the written one before drawing (diagnostic).
 equip_saved:Option<Equip>,equip_written:Option<Equip>,equip_lost:u64,equip_frames:u64,
 // Phase 2.1: the player's clock and (if flags are replayed) their own values of the flags the replay touches.
 clock_saved:Option<world_state::Clock>,flags_saved:Option<Vec<(u32,bool)>>,options:u32,
 // Reserved root-rebase value; remains zero until cross-origin conversion is verified.
 now_offset:[f32;3],
 replay_blocked:bool,
 mount_note:u8,returning:Option<(u64,u32)>,saved_place:Option<Place>,inv_saved:Option<bool>,eval_place:Option<Place>,virt:Option<Virt>,fx_cursor:usize,fx_t:u64,fx_live:Vec<(u64,u32)>,
 module_saved:Option<[u8;weapon_loc::BYTES]>,module_written:Option<[u8;weapon_loc::BYTES]>,module_lost:u64,module_frames:u64,
}
#[derive(Clone,Copy,PartialEq,Debug)]enum Arrival{Ready,Warping{target:Place,since:u64,stable:u32},Placing{tries:u32,frames:u32,good:u32}}
static STATE:Mutex<State>=Mutex::new(State{recording:None,record_paused:false,last_second:0,cur:None,loaded:None,loading:Loading::None,owning:false,restore_left:0,saved:None,gravity_saved:None,written:None,evaluated_root:None,pose_alpha:0.0,local_out:Vec::new(),model_out:Vec::new(),accuracy:Accuracy{frames:0,exact_bones:0,max_drawn_cm:0.0,sum_drawn_cm:0.0},expected:[0.0;3],settle:0,last_t:0,fallback_bones:0,host:(false,1.0,0,0),arrival:Arrival::Ready,warped:false,shift:[0.0;3],expected_root:None,equip_saved:None,equip_written:None,equip_lost:0,equip_frames:0,clock_saved:None,flags_saved:None,options:0,now_offset:[0.0;3],replay_blocked:false,mount_note:0,returning:None,saved_place:None,inv_saved:None,eval_place:None,virt:None,fx_cursor:0,fx_t:0,fx_live:Vec::new(),module_saved:None,module_written:None,module_lost:0,module_frames:0});
// Background load results land here and are adopted on the next tick.
static LOADED:Mutex<Option<Result<Loaded,(String,String)>>>=Mutex::new(None);

pub(crate) fn status(text:&str){if let Ok(c)=CString::new(text){unsafe{tm_render_event(c.as_ptr())}}}
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
fn pose(p:usize,bytes:usize)->&'static [u8]{unsafe{std::slice::from_raw_parts(p as *const u8,bytes)}}
// Read-only publication on Draw_Pre; no retained game pointers cross the FFI.
/// Tells the overlay the names of the player skeleton's bones (once per skeleton) so the bone camera can offer a named list.
fn publish_bone_names(chr:usize){
 static LAST:std::sync::atomic::AtomicUsize=std::sync::atomic::AtomicUsize::new(0);
 unsafe extern "C"{fn tm_camera_bone_names(names:*const *const std::ffi::c_char,count:i32);}
 let importer=read_ptr(chr+profile::OFF_CHRINS_POSE_IMPORTER);if importer<0x10000{return;}
 let skel=read_ptr(importer+profile::OFF_POSE_IMPORTER_SKELETON);if skel<0x10000||LAST.load(std::sync::atomic::Ordering::Relaxed)==skel{return;}
 let count=read_ptr(skel+profile::OFF_HKA_SKELETON_BONE_COUNT) as u32 as usize;let bones=read_ptr(skel+profile::OFF_HKA_SKELETON_BONES);
 if bones<0x10000||count==0||count>1024{return;}
 let mut names:Vec<std::ffi::CString>=Vec::with_capacity(count);
 for i in 0..count{
  let p=read_ptr(bones+i*16)&!1usize;let mut raw=[0u8;64];
  let name=if p>0x10000&&crate::companions::copy(p,&mut raw){let end=raw.iter().position(|b|*b==0).unwrap_or(raw.len());let s=&raw[..end];if !s.is_empty()&&s.iter().all(|b|(0x20..0x7F).contains(b)){String::from_utf8_lossy(s).into_owned()}else{String::new()}}else{String::new()};
  names.push(std::ffi::CString::new(if name.is_empty(){format!("bone {i}")}else{name}).unwrap_or_default());}
 let pointers:Vec<*const std::ffi::c_char>=names.iter().map(|n|n.as_ptr()).collect();
 unsafe{tm_camera_bone_names(pointers.as_ptr(),count as i32)};
 LAST.store(skel,std::sync::atomic::Ordering::Relaxed);
 let preview:Vec<String>=names.iter().take(12).map(|n|n.to_string_lossy().into_owned()).collect();
 crate::log_game(&format!("BONE_CAMERA: {count} bone names published, first: {preview:?}"));
}
pub fn camera_bone_sample(){
 unsafe extern "C"{fn tm_camera_bone_index()->i32;fn tm_camera_bone_publish(root:*const f32,qs:*const f32);}
 if let Some(chr)=player_chr(){publish_bone_names(chr);}
 let index=unsafe{tm_camera_bone_index()};if index<0{return;}
 let sample=(||{let chr=player_chr()?;let count=crate::actors::bone_count(chr)?;if index as usize>=count{return None;}
  let (_,model)=pose_arrays(chr)?;let mut root=[0u8;64];let mut qs=[0u8;48];
  if !crate::companions::copy(matrix_address(chr),&mut root)||!crate::companions::copy(model.checked_add((index as usize).checked_mul(48)?)?,&mut qs){return None;}
  Some((std::array::from_fn::<f32,16,_>(|i|f32::from_le_bytes(root[i*4..i*4+4].try_into().unwrap())),std::array::from_fn::<f32,12,_>(|i|f32::from_le_bytes(qs[i*4..i*4+4].try_into().unwrap()))))})();
 match sample{Some((root,qs))=>unsafe{tm_camera_bone_publish(root.as_ptr(),qs.as_ptr())},None=>unsafe{tm_camera_bone_publish(std::ptr::null(),std::ptr::null())}}
}
fn bones_path(replay:&str)->PathBuf{PathBuf::from(format!("{replay}.bones"))}
fn world_path(replay:&str)->PathBuf{PathBuf::from(format!("{replay}.world"))}

// Old file: "ERBONES1", u32 version (1-3), u32 bone count, u32 frame bytes, u32 reserved, u64 frame
// count, then per frame: u64 source time, 12 f32 transform, 16 f32 model matrix, local pose, model
// pose, (v2) i32 block, i32 origin block, 4 f32 global position, (v3) equipment assembly.
const FILE_VERSION_MAX:u32=3;
fn load(path:&PathBuf)->Result<Vec<Frame>,String>{
 let mut data=Vec::new();std::fs::File::open(path).and_then(|mut f|f.read_to_end(&mut data)).map_err(|e|e.to_string())?;
 if data.len()<32||&data[0..8]!=MAGIC{return Err("not a bone replay file".into());}
 let u32_at=|i:usize|u32::from_le_bytes(data[i..i+4].try_into().unwrap());
 let (version,stride)=(u32_at(8),u32_at(16) as usize);
 if version>FILE_VERSION_MAX||!((version==1&&stride==FRAME_BYTES_V1)||(version==2&&stride==FRAME_BYTES_V2)||(version==3&&stride==FRAME_BYTES))||u32_at(12)!=BONES as u32{return Err(format!("unsupported version {version} / bones {} / frame size {stride}",u32_at(12)));}
 let count=u64::from_le_bytes(data[24..32].try_into().unwrap()) as usize;
 if count==0||data.len()!=32+count*stride{return Err(format!("size {} does not match {count} frames",data.len()));}
 let f32_at=|i:usize|f32::from_le_bytes(data[i..i+4].try_into().unwrap());
 let i32_at=|i:usize|i32::from_le_bytes(data[i..i+4].try_into().unwrap());
 let frames=(0..count).map(|k|{let o=32+k*stride;
  let p=o+FRAME_BYTES_V1;
  let equip=if version>=3{Equip::decode(&data[o+FRAME_BYTES_V2..o+FRAME_BYTES])}else{Equip::default()};
  let place=if version>=2{Place{block:i32_at(p),origin:i32_at(p+4),global:std::array::from_fn(|i|f32_at(p+8+i*4))}}else{Place{block:-1,origin:-1,global:[0.0;4]}};
  let transform=std::array::from_fn(|r|std::array::from_fn(|c|f32_at(o+8+(r*4+c)*4)));
  let matrix=std::array::from_fn(|i|f32_at(o+56+i*4));
  Frame{time:u64::from_le_bytes(data[o..o+8].try_into().unwrap()),transform,matrix,local:data[o+120..o+120+POSE_BYTES].to_vec(),model:data[o+120+POSE_BYTES..o+FRAME_BYTES_V1].to_vec(),place,equip}}).collect::<Vec<_>>();
 if frames.windows(2).any(|w|w[1].time<w[0].time){return Err("frame times go backwards".into());}
 Ok(frames)}

/// Frames and effects both use the game clock (monotonic ns), so effect times are stored unchanged.
fn fx_relative(v:Vec<world_file::EffectEvent>,_first:u64)->Vec<world_file::EffectEvent>{v}
fn finish_recording(s:&mut State){
 let Some(r)=s.recording.take() else {return;};
 // Effects: stop capturing and hand the last ones to the writer before the channel closes.
 effects::capture(false);{let rest=fx_relative(effects::drain(),r.first);{let up=effects::drain_updates();if !up.is_empty(){let _=r.tx.try_send(Message::EffectUpdates(up));}}let (seen,dropped,_)=effects::stats();if !rest.is_empty(){let _=r.tx.try_send(Message::Effects(rest));}
  crate::log_game(&format!("EFFECTS: {seen} effect creations seen so far this session ({dropped} dropped by the queue)"));}
 let seconds=r.last.saturating_sub(r.first) as f64/1e9;
 crate::log_game(&format!("BONE_REPLAY: recording stopped; {} frames over {seconds:.2} s ({} dropped while the disk was busy), {} characters; finishing {}",r.frames,r.dropped,r.actors.count(),world_path(&r.path).display()));
 crate::log_game(&format!("COMPANIONS_SUMMARY: {} companion identities, {} dropped actor samples (no lost catalog announcements)",r.actors.companions(),r.actors.drops()));
 if r.frames==0{status("BONE REPLAY: no bones were recorded (load in first)");}
 drop(r.tx);} // close the producer; writer drains queued data and finalizes without blocking a game task

fn adopt_loaded(s:&mut State){
 let Some(result)=LOADED.lock().unwrap().take() else {return;};
 match result{
  Ok(l)=>{let current=matches!(&s.loading,Loading::Busy(p) if *p==l.path);if !current{return;}
   crate::log_game(&format!("BONE_REPLAY: loaded {} frames ({:.2} s) for {}",l.store.len(),l.seconds,l.path));
   status(&format!("BONE REPLAY: replay loaded ({:.1} s). Play or scrub the timeline",l.seconds));s.loading=Loading::None;s.loaded=Some(l);}
  Err((path,e))=>{if !matches!(&s.loading,Loading::Busy(p) if *p==path){return;}
   crate::log_game(&format!("BONE_REPLAY_ERROR: could not load bones for {path}: {e}"));status("BONE REPLAY: this replay's bone data could not be read (see log)");s.loading=Loading::Failed(path);}}}

// Which loaded replay the host has open; starts a background load when it changes.
fn follow_loaded(s:&mut State,l:&Link){
 let path=if l.loaded!=0{text(&l.loaded_path)}else{String::new()};
 let known=match &s.loading{Loading::Busy(p)|Loading::Missing(p)|Loading::Failed(p)=>Some(p.clone()),Loading::None=>s.loaded.as_ref().map(|x|x.path.clone())};
 if known.as_deref()==Some(path.as_str())||(path.is_empty()&&known.is_none()){return;}
 if let Some(a)=s.loaded.as_mut().and_then(|l|l.actors.as_mut()){a.release();}
 s.loaded=None;s.loading=Loading::None;s.written=None;s.evaluated_root=None;s.cur=None;s.replay_blocked=false;
 if path.is_empty(){LOADED.lock().unwrap().take();crate::log_game("BONE_REPLAY: unloaded pose data; pending loads invalidated");return;}
 let (world,file)=(world_path(&path),bones_path(&path));
 if !world.is_file()&&!file.is_file(){crate::log_game(&format!("BONE_REPLAY: {path} has no bone data (recorded before bone replays)"));status("BONE REPLAY: this replay has no bone data. Record a new one with F5");s.loading=Loading::Missing(path);return;}
 s.loading=Loading::Busy(path.clone());status("BONE REPLAY: loading bones...");
 let _=std::thread::Builder::new().name("TheaterMode.BoneLoad".into()).spawn(move||{
  let opened=if world.is_file(){world_file::open(&world).map(|w|{
    let mut touched:Vec<u32>=w.flag_events.iter().map(|e|e.flag).collect();touched.sort_unstable();touched.dedup();
    let actors=(!w.actors.is_empty()).then(||crate::actors::Player::new(w.actors,&w.context,&w.skeletons,w.actor_lifetime,w.meta));
    (Store::Chunked(w.player),WorldData{samples:w.world,flags_start:w.flags_start,events:w.flag_events,touched,context:w.context,skeletons:w.skeletons,module:w.module,effects:w.effects,effect_tracks:{let mut t:std::collections::HashMap<(u64,u32),Vec<(u64,[f32;16])>>=std::collections::HashMap::new();for u in &w.effect_updates{t.entry((u.created,u.id)).or_default().push((u.time,u.m));}for v in t.values_mut(){v.sort_by_key(|x|x.0);}t}},actors)})}else{load(&file).map(|f|(Store::Memory(f),WorldData::default(),None))};
  let result=opened.map(|(mut store,mut world_data,actors)|{
   // Learn the skeleton hierarchy from a few frames spread over the recording (see replay_interpolation).
   let n=store.len();let picks:Vec<Frame>=(0..5).filter_map(|k|store.get(k*(n-1)/4)).collect();
   let parents=world_data.skeletons.get(&0).map(|d|d.parents.clone()).unwrap_or_else(||crate::replay_interpolation::learn_parents(picks.iter().map(|f|(&f.local[..],&f.model[..]))));
   let known=parents.iter().filter(|p|**p!=crate::replay_interpolation::UNKNOWN_PARENT).count();
   crate::log_game(&format!("BONE_REPLAY: skeleton hierarchy learned for {known} of {} bones; the rest interpolate on their own",parents.len()));
   let seconds=(store.time(n-1)-store.time(0)) as f64/1e9;
   crate::log_game(&format!("BONE_REPLAY: world track: {} clock samples, {} flag changes on {} flags",world_data.samples.len(),world_data.events.len(),world_data.touched.len()));
   if let Some(a)=&actors{crate::log_game(&format!("BONE_REPLAY: {} recorded characters (enemies, NPCs, bosses)",a.len()));}
   // The player's anchor over the recording, for rebasing positions into the live physics frame.
   // Files saved by C19-8 stored effect times relative to the recording start; every real clock reading is far larger than the
   // recording itself, so such a file is recognised and shifted back onto the frame clock.
   if let (Some(first),Some(last))=(store.get(0).map(|f|f.time),world_data.effects.last().map(|e|e.time)){if last<first{for e in world_data.effects.iter_mut(){e.time+=first;}}}
   let mut anchors=crate::replay_interpolation::AnchorTrack::default();
   for i in 0..n{if let Some(f)=store.get(i){if f.place.block!=-1{anchors.push(f.time,f.place.frame(),f.place.global);}}}
   crate::log_game(&format!("BONE_REPLAY: {} physics origin shifts recorded",anchors.len_changes()));
   crate::log_game(&format!("BONE_REPLAY: {} recorded effects in the file; replay of them is {}",world_data.effects.len(),if effects::ready(){"available"}else{"unavailable (hook not installed)"}));
   {let sample:Vec<String>=world_data.effects.iter().take(12).map(|e|format!("id {} t={:.2}s at ({:.1},{:.1},{:.1})",e.id,e.time as f64/1e9,e.pos[0],e.pos[1],e.pos[2])).collect();crate::log_game(&format!("EFFECTS_IN_FILE first events: {sample:?}"));}
   Loaded{path:path.clone(),store,parents:Arc::new(parents),seconds,world:Arc::new(world_data),actors,anchors:Arc::new(anchors)}}).map_err(|e|(path,e));*LOADED.lock().unwrap()=Some(result);});}

// The replay time for this game frame, in source time. The host sends the timeline about 20 times a
// second; between updates the replay clock advances by elapsed game time x speed, so motion is
// continuous at any speed. Every evaluation derives from the authoritative host anchor;
// backward seeks and pauses take effect exactly, without a second clock integrator.
// While paused or scrubbing it follows the host exactly.
fn replay_time(s:&mut State,now:u64)->u64{
 let (playing,speed,source,received)=s.host;let speed=speed.clamp(0.001,10.0);
 // Derive from the single host anchor on the shared unscaled Windows clock.
 // No local integrator, monotonic clamp or error smoothing can delay backward seeks.
 crate::replay_interpolation::source_time(source,received,now,playing,speed)}
// Picks the two recorded frames around this frame's replay time and the blend between them, and
// computes the root transform and where the body should be drawn. Called once per game frame, before
// the writes, so bones and root always come from the same instant.
fn select(s:&mut State,now:u64,live_place:Place)->bool{
 let t=replay_time(s,now);
 let Some(loaded)=&mut s.loaded else {return false;};
 let i=loaded.store.index_at(t);let n=loaded.store.len();
 let (Some(a),Some(b))=(loaded.store.get(i),loaded.store.get((i+1).min(n-1))) else {return false;};
 let span=b.time.saturating_sub(a.time);
 // No blending across a gap in the recording or a teleport (warp, grace travel): hold the earlier frame.
 s.written=Some(i);
 let (a,b)=&*s.cur.insert((a,b));
 // The physics origin moves when the game re-bases the world; each recorded position is carried into the
 // live frame by the change of the anchor (see replay_interpolation::rebase, measured on real recordings).
 let translate=|p:&Place|->Option<[f32;3]>{if p.block==-1{Some([0.0;3])}else{crate::replay_interpolation::rebase(p.frame(),p.global,live_place.frame(),live_place.global)}};
 let (Some(ta),Some(tb))=(translate(&a.place),translate(&b.place)) else {
  s.evaluated_root=None;
  crate::log_game(&format!("ROOT_SPACE_UNAVAILABLE: replay frame {} (origin {}, block {}) vs live frame {} (origin {}, block {}) differ; anchors {:?} / {:?}; no conversion is known between different frames",a.place.frame(),a.place.origin,a.place.block,live_place.frame(),live_place.origin,live_place.block,a.place.global,live_place.global));
  status("REPLAY: the live position is in another map space; waiting for travel to the recorded place.");return false;};
 s.shift=ta;s.now_offset=ta;
 // Interpolate in the live frame, so an origin re-base between two samples is not a 32 m jump.
 let (mut ra,mut rb)=(a.transform,b.transform);for k in 0..3{ra[2][k]+=ta[k];rb[2][k]+=tb[k];}
 let jump=(0..3).map(|k|(rb[2][k]-ra[2][k]).powi(2)).sum::<f32>().sqrt();
 let alpha=if span==0||span>250_000_000||jump>1.5{0.0}else{t.saturating_sub(a.time)as f64/span as f64};
 s.pose_alpha=alpha.clamp(0.0,1.0);s.evaluated_root=crate::replay_interpolation::evaluate(&ra,&rb,s.pose_alpha);
 s.expected_root=s.evaluated_root.map(|r|[r[2][0],r[2][1],r[2][2]]);s.eval_place=Some(live_place);
 // A seek or the first frames after taking the body teleport it; the draw lags one frame there.
 if t<s.last_t||t-s.last_t>200_000_000{s.settle=s.settle.max(3);}s.last_t=t;
 let (shift,w)=(s.shift,s.pose_alpha as f32);s.expected=std::array::from_fn(|k|a.matrix[12+k]+(b.matrix[12+k]-a.matrix[12+k])*w+shift[k]);
 s.evaluated_root.is_some()}

/// The saved return position in today's physics frame: the physics origin may have moved since it was read
/// (the same anchor shift that playback corrects), and writing it unchanged put the body under the map.
/// The physics origin can be re-based by the game between the write point (where the root was evaluated against
/// the anchor of that moment) and a later write: the same recorded point then has a different physics value
/// (32 m in x/z, 8 m in y per measured shift). Carries `evaluated_root` into today's frame; writing it stale put
/// the body (and everything driven with it) a whole shift away, i.e. under the ground.
/// The live physics frame as the replay sees it: the frame and anchor at the start of the replay, advanced by
/// every shift of the physics origin since. Within one frame a shift equals the change of `chunk_position`
/// (measured). When the game changes the origin BLOCK (crossing into another tile) the anchors are relative
/// to a different corner and no conversion is certain, so the shift is MEASURED instead: how far the game
/// moved the body we hold, compared with where we last wrote it.
#[derive(Clone,Copy,Debug)]
struct Virt{frame:i32,anchor:[f32;3],last_frame:i32,last_anchor:[f32;4],written:Option<[f32;3]>}
fn update_live(s:&mut State,chr:usize)->Place{
 let here=arrival::place(chr);if here.block==-1{return here;}
 let phys=read_transform(chr).map(|t|[t[2][0],t[2][1],t[2][2]]);
 let v=match s.virt{
  None=>Virt{frame:here.frame(),anchor:[here.global[0],here.global[1],here.global[2]],last_frame:here.frame(),last_anchor:here.global,written:None},
  Some(mut v)=>{
   if here.frame()==v.last_frame{for k in 0..3{v.anchor[k]+=here.global[k]-v.last_anchor[k];}}
   else if let (Some(w),Some(p))=(v.written,phys){
    let d:[f32;3]=std::array::from_fn(|k|p[k]-w[k]);for k in 0..3{v.anchor[k]+=d[k];}
    crate::log_game(&format!("LIVE_FRAME_CHANGE: live origin {} -> {} (anchor {:?} -> {:?}); measured physics shift of the held body {:?}; replay frame anchor now {:?}",arrival::block_name(v.last_frame),arrival::block_name(here.frame()),&v.last_anchor[..3],&here.global[..3],d,v.anchor));}
   v.last_frame=here.frame();v.last_anchor=here.global;v}};
 s.virt=Some(v);
 Place{block:here.block,origin:v.frame,global:[v.anchor[0],v.anchor[1],v.anchor[2],here.global[3]]}}
fn note_written(s:&mut State,root:&Transform){if let Some(v)=&mut s.virt{v.written=Some([root[2][0],root[2][1],root[2][2]]);}}
fn live_shift(s:&mut State,chr:usize)->[f32;3]{
 let Some(p)=s.eval_place else {return [0.0;3]};let now=update_live(s,chr);
 if p.block==-1||now.block==-1{return [0.0;3];}
 crate::replay_interpolation::rebase(p.frame(),p.global,now.frame(),now.global).unwrap_or([0.0;3])}
fn root_now(s:&mut State,chr:usize)->Option<Transform>{
 let mut r=s.evaluated_root?;let sh=live_shift(s,chr);for k in 0..3{r[2][k]+=sh[k];}Some(r)}
fn saved_for_now(s:&State,chr:usize)->Option<Transform>{
 let mut t=s.saved?;
 if let Some(p)=s.saved_place{let now=arrival::place(chr);
  if p.block!=-1&&now.block!=-1{if let Some(sh)=crate::replay_interpolation::rebase(p.frame(),p.global,now.frame(),now.global){for k in 0..3{t[2][k]+=sh[k];}}}}
 Some(t)}
/// The player died while the replay held the body: give everything back at once (no return teleport onto a
/// dead body) so the game's own revival menu works.
fn abandon_on_death(s:&mut State,chr:usize){
 s.saved=None;s.restore_left=0;s.replay_blocked=true;
 release(s,chr,"player died");
 s.restore_left=0;if let Some(g)=s.gravity_saved.take(){set_flag(gravity_flag(chr),g);}
 if let Some(b)=s.inv_saved.take(){crate::actors::set_invincible(chr,b);}
 unsafe{tm_render_lock_game_input(0)};
 crate::log_game("BONE_REPLAY: PLAYER_DEATH while the replay owned the body; ownership, gravity, invincibility and input lock returned");
 status("REPLAY STOPPED: your character died. Controls are back; use the game's revival menu.");}
fn release(s:&mut State,chr:usize,reason:&str){
 if !s.owning{return;}s.owning=false;s.virt=None;s.restore_left=RESTORE_FRAMES;
 let a=&s.accuracy;
 crate::log_game(&format!("BONE_REPLAY: released the body ({reason}); accuracy over {} settled frames: written bones retained to draw {:.1}%, drawn position error vs interpolated recording max {:.2} cm mean {:.3} cm; bones kept at the earlier frame because interpolation was invalid: {}",
  a.frames,100.0*a.exact_bones as f64/(a.frames.max(1) as f64),a.max_drawn_cm,a.sum_drawn_cm/(a.frames.max(1) as f64),s.fallback_bones));
 // After a grace warp the saved spot is in another map; the player stays where the replay was.
 // The replay may have sent the player across the map by grace warp: bring them back to where they started, the same way.
 if let Some((place,transform))=ORIGIN.lock().unwrap().take(){
  let here=arrival::place(chr);
  if here.block!=-1&&arrival::needs_warp(&here,&place){
   match arrival::nearest_grace(&place){
    Some((grace,name))=>match arrival::warp_to_grace(grace){
     Ok(())=>{LAST_WARP_NS.store(crate::monotonic_ns().max(1),std::sync::atomic::Ordering::Relaxed);
      s.saved=Some(transform);s.saved_place=Some(place);s.restore_left=0;s.returning=Some((crate::monotonic_ns(),0));
      crate::log_game(&format!("RETURN: warping back to {name} near where you started ({})",arrival::block_name(place.block)));status("BONE REPLAY: taking you back to where you started...");}
     Err(e)=>{s.saved=None;crate::log_game(&format!("RETURN_ERROR: could not warp back ({e}); you stay where the replay ended"));status("BONE REPLAY: could not take you back automatically; use a grace to travel");}},
    None=>{s.saved=None;crate::log_game("RETURN_ERROR: no grace near your starting point; you stay where the replay ended");status("BONE REPLAY: could not take you back automatically; use a grace to travel");}}}
 }
 if s.returning.is_none(){if let Some(t)=saved_for_now(s,chr){write_transform(chr,&t);}}
 if s.module_frames>0{crate::log_game(&format!("WEAPON_LOCATION: written {} frames; the game replaced it before drawing on {} of them",s.module_frames,s.module_lost));}
 if let Some(m)=s.module_saved.take(){weapon_loc::write(chr,&m);}
 crate::item_probe::clear_hide(chr);effects::release_all();
 if s.equip_frames>0{crate::log_game(&format!("EQUIPMENT: written {} frames; the game replaced it before drawing on {} of them",s.equip_frames,s.equip_lost));}
 if let Some(e)=s.equip_saved.take().filter(|_|s.equip_frames>0){if equipment::write(chr,&e){crate::log_game("EQUIPMENT: your own equipment restored");}}
 if let Some(a)=s.loaded.as_mut().and_then(|l|l.actors.as_mut()){a.release();}
 if let Some(c)=s.clock_saved.take(){world_state::write_clock(&c);crate::log_game("WORLD: your time of day restored");}
 if let Some(f)=s.flags_saved.take(){for (flag,state) in &f{world_state::write_flag(*flag,*state);}crate::log_game(&format!("WORLD: {} event flags restored to your own values",f.len()));}}

fn reject(s:&mut State,chr:usize,reason:&str){
 s.replay_blocked=true;release(s,chr,reason);
 crate::log_game(&format!("REPLAY_ERROR_LATCHED: {reason}; stop and explicitly play again to retry"));
}

// Phase 2.1: time of day (always) and, if enabled, the event flags at this frame's replay time.
fn apply_world(s:&mut State){
 let (Some(loaded),t)=(&s.loaded,s.last_t) else {return};let w=loaded.world.clone();
 if s.clock_saved.is_some()&&!w.samples.is_empty(){let i=w.samples.partition_point(|x|x.time<=t).saturating_sub(1);let mut c=w.samples[i].clock;c.multiplier=0.0;world_state::write_clock(&c);}
 if s.flags_saved.is_some(){
  if let Some((_,start))=&w.flags_start{
   for flag in &w.touched{
    let (g,byte,bit)=(flag/1000,((flag%1000)/8) as usize,7-((flag%1000)%8));
    let mut state=start.iter().find(|(id,_)|*id==g).map(|(_,b)|b[byte]&(1<<bit)!=0).unwrap_or(false);
    for e in w.events.iter().take_while(|e|e.time<=t){if e.flag==*flag{state=e.state;}}
    if world_state::read_flag(*flag)!=state{world_state::write_flag(*flag,state);}}}}}

// Phase 1.4: decide how to reach the recorded place when the replay takes the body.
/// Further than this from the recorded start the player is taken there by grace travel (the map around the
/// start is then loaded); closer, the body is placed directly.
const FAR_DIRECT_M:f32=250.0;
fn begin_arrival(s:&mut State,chr:usize){
 s.arrival=Arrival::Placing{tries:0,frames:0,good:0};
 let Some(loaded)=&mut s.loaded else {return;};
 let t=s.host.2;let i=loaded.store.index_at(t);let Some(target)=loaded.store.get(i).map(|f|f.place) else {return;};
 s.virt=None;let here=arrival::place(chr);
 // Positions are rebased by the anchor change (measured), so a different tile anchor is fine; only a
 // different origin id has no known conversion.
 // Another map (or another coordinate space) has no conversion until the game has taken the player there,
 // so it is reached by grace travel first; after that the spaces match.
 let convertible=target.block==-1||crate::replay_interpolation::rebase(target.frame(),target.global,here.frame(),here.global).is_some();
 // Distance from the player to the recorded start, measured in one frame (anchor-corrected physics).
 let far_m=if convertible&&target.block!=-1{
  let tf=loaded.store.get(i).map(|f|f.transform[2]);let live=read_transform(chr).map(|t|t[2]);
  match(tf,live,crate::replay_interpolation::rebase(target.frame(),target.global,here.frame(),here.global)){
   (Some(p),Some(l),Some(sh))=>(0..3).map(|k|(p[k]+sh[k]-l[k]).powi(2)).sum::<f32>().sqrt(),_=>0.0}}else{0.0};
 if target.block==-1{s.arrival=Arrival::Ready;crate::log_game("ARRIVAL: recording has no map data (older file); positions used as recorded");return;}
 crate::log_game(&format!("ARRIVAL: here {} {:?}, replay {} {:?}, distance {:.1} m",arrival::block_name(here.block),&here.global[..3],arrival::block_name(target.block),&target.global[..3],arrival::distance(here.global,target.global)));
 // A grace warp ends the game session of the body (loading screen), which releases ownership; the replay then starts again from
 // scratch. When the nearest grace is itself more than the direct-placement limit away from the recorded start, that would warp
 // again and again. So after a warp in the last two minutes the body is placed directly instead of travelling a second time.
 let last=LAST_WARP_NS.load(std::sync::atomic::Ordering::Relaxed);
 if last!=0&&crate::monotonic_ns().saturating_sub(last)<120_000_000_000&&convertible{crate::log_game(&format!("ARRIVAL: already travelled by grace {:.0} s ago; placing the body directly ({far_m:.0} m) instead of warping again",crate::monotonic_ns().saturating_sub(last) as f64/1e9));return;}
 let far=far_m>FAR_DIRECT_M;
 if convertible&&!far&&!arrival::needs_warp(&here,&target){return;}
 crate::log_game(&format!("ARRIVAL: travel needed (convertible={convertible}, distance {far_m:.0} m, limit {FAR_DIRECT_M:.0} m)"));
 match arrival::nearest_grace(&target){
  None if convertible=>{crate::log_game(&format!("ARRIVAL: no grace found for {}; placing the body directly ({far_m:.0} m)",arrival::block_name(target.block)));}
  None=>{crate::log_game(&format!("ARRIVAL_ERROR: no grace found for {}; cannot travel there",arrival::block_name(target.block)));status("BONE REPLAY ERROR: the replay was recorded where no grace can take you. Travel there first, then play it");reject(s,chr,"no grace to travel to");}
  Some((grace,name))=>match arrival::warp_to_grace(grace){
   Ok(())=>{LAST_WARP_NS.store(crate::monotonic_ns().max(1),std::sync::atomic::Ordering::Relaxed);{let mut o=ORIGIN.lock().unwrap();if o.is_none(){if let (Some(t),Some(p))=(s.saved,s.saved_place){*o=Some((p,t));crate::log_game(&format!("ARRIVAL: origin remembered ({} {:?}) so you can be brought back",arrival::block_name(p.block),&p.global[..3]));}}}s.warped=true;s.arrival=Arrival::Warping{target,since:crate::monotonic_ns(),stable:0};crate::log_game(&format!("ARRIVAL: warping to {name} for {}",arrival::block_name(target.block)));status("BONE REPLAY: travelling to the replay's location...");}
   Err(e) if convertible=>{crate::log_game(&format!("ARRIVAL: grace warp unavailable ({e}); placing the body directly ({far_m:.0} m)"));}
   Err(e)=>{crate::log_game(&format!("ARRIVAL_ERROR: grace warp failed: {e}"));status("BONE REPLAY ERROR: could not travel to the replay's location (see log)");reject(s,chr,"grace warp failed");}}}}
// Each frame while owning: finish a warp once the map has loaded, then confirm the body is on the spot.
fn advance_arrival(s:&mut State,chr:usize,now:u64){
 match s.arrival{
  Arrival::Ready=>{}
  Arrival::Warping{target,since,stable}=>{
   let here=arrival::place(chr);
   // The 4 s floor: a warp inside the same area is still "settled" for a moment before the loading screen starts.
   let settled=!arrival::loading()&&here.block!=-1&&BlockArea::same(here.block,target.block)&&now.saturating_sub(since)>4_000_000_000;
   let stable=if settled{stable+1}else{0};
   if stable>=90{crate::log_game(&format!("ARRIVAL: map loaded ({} after {:.1} s); placing the body",arrival::block_name(here.block),(now-since) as f64/1e9));s.virt=None;s.arrival=Arrival::Placing{tries:0,frames:0,good:0};s.settle=s.settle.max(3);}
   else if now.saturating_sub(since)>60_000_000_000{crate::log_game("ARRIVAL_ERROR: the map did not finish loading within 60 s");status("BONE REPLAY ERROR: the replay's location did not finish loading (see log)");reject(s,chr,"warp timed out");}
   else{s.arrival=Arrival::Warping{target,since,stable};}}
  Arrival::Placing{tries,frames,good}=>{
   let (Some(mut expected),Some(now_root))=(s.expected_root,read_transform(chr)) else {s.arrival=Arrival::Ready;return;};
   {let sh=live_shift(s,chr);for k in 0..3{expected[k]+=sh[k];}}
   let error=(0..3).map(|k|(now_root[2][k]-expected[k]).powi(2)).sum::<f32>().sqrt();
   let good=if error<0.5{good+1}else{0};let frames=frames+1;
   if good>=5{crate::log_game(&format!("ARRIVAL: body on the recorded spot (error {:.2} m, try {})",error,tries+1));s.arrival=Arrival::Ready;}
   else if frames>=30{
    if tries+1>=3{crate::log_game(&format!("ARRIVAL_ERROR: body is {:.2} m from the recorded spot after 3 tries",error));status("BONE REPLAY ERROR: could not place you on the recorded spot (see log)");reject(s,chr,"placement failed");}
    else{crate::log_game(&format!("ARRIVAL: retry {} (error {:.2} m)",tries+2,error));set_flag(proxy_flag(chr),true);s.arrival=Arrival::Placing{tries:tries+1,frames:0,good:0};}}
   else{s.arrival=Arrival::Placing{tries,frames,good};}}}}
struct BlockArea;impl BlockArea{fn same(a:i32,b:i32)->bool{eldenring::cs::BlockId::from(a).area()==eldenring::cs::BlockId::from(b).area()}}

pub fn tick(group:usize,now:u64){
 if group==DRAW_GROUP {
  static START:std::sync::atomic::AtomicU64=std::sync::atomic::AtomicU64::new(0);
  static COUNT:std::sync::atomic::AtomicU64=std::sync::atomic::AtomicU64::new(0);
  use std::sync::atomic::Ordering::Relaxed;
  let start=START.load(Relaxed);let frames=COUNT.fetch_add(1,Relaxed)+1;
  if start==0{START.store(now,Relaxed);}else if now.saturating_sub(start)>=5_000_000_000{
   crate::log_game(&format!("DRAW_CALLBACK_CADENCE hz={:.2} elapsed_ms={} pose_interpolation={} (task cadence, not GPU FPS)",frames as f64*1e9/(now-start)as f64,(now-start)/1_000_000,interpolated_pose_enabled()));
   COUNT.store(0,Relaxed);START.store(now,Relaxed);
  }
 }
 let mut guard=STATE.lock().unwrap();let s=&mut *guard;
 let chr=player_chr();
 if group==KEYS_GROUP{
  let l=link();
  if !crate::offline_allowed(){
   s.replay_blocked=true;
   if s.recording.is_some(){finish_recording(s);status("RECORDING STOPPED: offline/anti-cheat check failed.");}
   if let Some(chr)=chr{release(s,chr,"offline/anti-cheat check failed");}
   // Cleanup only, never leave a delayed return teleport armed for a later player/session.
   s.restore_left=0;s.saved=None;
   if let Some(g)=s.gravity_saved.take(){if let Some(chr)=chr{set_flag(gravity_flag(chr),g);}}
   unsafe{tm_render_lock_game_input(0)};return;
  }
  // Player loss cannot leave the keyboard lock or a return transform armed for a new player.
  if chr.is_none()&&(s.owning||s.restore_left>0){
   s.replay_blocked=true;
   s.owning=false;s.restore_left=0;s.saved=None;s.gravity_saved=None;s.written=None;s.evaluated_root=None;
   unsafe{tm_render_lock_game_input(0)};crate::log_game("BONE_REPLAY: player unavailable; ownership and input lock released");}
  // Recording follows the host recorder.
  let recording=l.linked!=0&&(l.recording==RECORD_RECORDING||l.recording==RECORD_PAUSED);
  if recording&&s.recording.is_none(){let path=text(&l.recording_path);
   if !path.is_empty(){
    match world_file::start_writer(world_path(&path)){
     Ok(tx)=>{
      let flags=world_state::read_flags();let _=tx.send(Message::Flags(now,flags.clone()));
      crate::log_game(&format!("BONE_REPLAY: recording started for {path} ({} event flag groups)",flags.len()));
      effects::capture(true);let _=effects::drain();
      s.recording=Some(Recorder{path,tx,frames:0,first:0,last:0,dropped:0,next_world:now,flags,actors:crate::actors::Recorder::new(),skeleton:None,skeleton_warned:false,module:None,probe:Default::default()});s.last_second=0;}
     Err(e)=>{crate::log_game(&format!("BONE_REPLAY_ERROR: cannot create the world file for {path}: {e}"));status("BONE REPLAY ERROR: cannot write the recording file (see log)");}}}}
  if !recording&&s.recording.is_some(){finish_recording(s);}
  s.record_paused=l.recording==RECORD_PAUSED;
  if let Some(r)=&mut s.recording{if !s.record_paused&&now>=r.next_world{
   r.next_world=now+1_000_000_000;
   if let Some(clock)=world_state::read_clock(){let _=r.tx.try_send(Message::World(world_file::WorldSample{time:now,clock}));}
   let flags=world_state::read_flags();let changes=world_state::diff(&r.flags,&flags);r.flags=flags;
   if !changes.is_empty(){let _=r.tx.try_send(Message::FlagEvents(changes.into_iter().map(|(flag,state)|world_file::FlagEvent{time:now,flag,state}).collect()));}}}
  // Playback follows the loaded replay and the timeline.
  adopt_loaded(s);follow_loaded(s,&l);
  let fresh=l.linked!=0&&l.received_ns!=0&&now.saturating_sub(l.received_ns)<500_000_000;
  if !fresh&&s.owning{s.replay_blocked=true;}
  if fresh&&l.apply_requested!=0&&l.playing!=0&&!s.host.0{s.replay_blocked=false;}
  let want=fresh&&s.loaded.is_some()&&s.recording.is_none()&&l.apply_requested!=0&&!s.replay_blocked;
  // No guessed mounting API: reject a mounted/unmounted mismatch before body ownership/writes.
  // Mounted/on-foot differences no longer block playback (owner request): the rider's recorded pose and root are
  // simply replayed, and Torrent is driven as a recorded character when he is in the world. The game's mounted
  // state itself is never changed (no guessed mount API). A short note says what to expect.
  if want{if let Some(chr)=chr{
   let required=s.loaded.as_ref().and_then(|x|world_file::context_at(&x.world.context,0,l.play_source_ns));
   let live_ride=crate::companions::ride(chr);
   if let Some(required)=required.filter(|c|!crate::companions::mount_compatible(c.ride_flags,live_ride)){
    let recorded_mounted=required.ride_flags&crate::companions::MOUNTED!=0;
    let key=(recorded_mounted as u8)+1;
    if s.mount_note!=key{s.mount_note=key;
     let msg=if recorded_mounted{"MOUNT: this was recorded mounted and you are on foot. Replaying the rider pose; Torrent is shown only if he is in the world (whistle for him first)."}else{"MOUNT: this was recorded on foot and you are mounted. Replaying the on-foot pose; dismounting first gives the cleanest result."};
     crate::log_game(&format!("MOUNT_STATE_MISMATCH: recorded_mounted={recorded_mounted} live_ride={live_ride:?} mount_id={}; replay continues (not blocked)",required.mount_id));status(msg);}}}}
  if let Some(chr)=chr{if s.owning&&crate::actors::body_dead(chr){abandon_on_death(s,chr);}}
  if let Some(chr)=chr{
   if want&&!s.owning{
    s.saved=read_transform(chr);if s.saved.is_some(){
     s.saved_place=Some(arrival::place(chr));s.inv_saved=crate::actors::invincible(chr);crate::actors::set_invincible(chr,true);
     s.owning=true;s.written=None;s.evaluated_root=None;s.accuracy=Accuracy::default();s.settle=3;s.fallback_bones=0;s.warped=false;
     s.module_saved=weapon_loc::read(chr);s.module_written=None;s.module_lost=0;s.module_frames=0;s.equip_saved=equipment::read(chr);s.equip_written=None;s.equip_lost=0;s.equip_frames=0;
     // The clock may also be save-backed. Capture it, but do not override before save isolation.
     s.clock_saved=None;s.options=l.options;
     // Temporary flag restoration alone cannot prevent autosave persisting an override.
     // Keep flag tracks read-only until a verified no-save/lifecycle contract exists.
     s.flags_saved=None;
     begin_arrival(s,chr);
     if s.replay_blocked{s.host=(l.playing!=0,l.timescale,l.play_source_ns,l.received_ns);return;}
     let g=gravity_flag(chr);s.gravity_saved=g.map(|a|unsafe{std::ptr::read_volatile(a as *const u8)}==1);set_flag(g,true);
     unsafe{tm_render_lock_game_input(1)};crate::log_game("BONE_REPLAY: the replay owns the body; controls locked, return spot saved");}}
   else if !want&&s.owning{release(s,chr,if s.loaded.is_none(){"replay unloaded"}else{"timeline idle and overlay closed"});}
   if let (Some((since,stable)),false)=(s.returning,s.owning){
    let here=arrival::place(chr);let target=s.saved_place.unwrap_or_default();let elapsed=crate::monotonic_ns().saturating_sub(since);
    let settled=!arrival::loading()&&here.block!=-1&&BlockArea::same(here.block,target.block)&&elapsed>4_000_000_000;
    let stable=if settled{stable+1}else{0};
    if stable>=90{
     s.returning=None;s.restore_left=RESTORE_FRAMES;s.gravity_saved=gravity_flag(chr).map(|_|false);set_flag(gravity_flag(chr),true);
     crate::log_game(&format!("RETURN: map loaded ({}); placing you back at your starting spot",arrival::block_name(here.block)));}
    else if elapsed>60_000_000_000{
     s.returning=None;s.saved=None;s.restore_left=0;crate::log_game("RETURN_ERROR: the map did not finish loading within 60 s; you stay where you are");status("BONE REPLAY: could not take you back automatically; use a grace to travel");unsafe{tm_render_lock_game_input(0)};}
    else{s.returning=Some((since,stable));}}
   if !s.owning&&s.restore_left>0&&s.returning.is_none(){
    if let Some(t)=saved_for_now(s,chr){write_transform(chr,&t);}set_flag(proxy_flag(chr),true);
    s.restore_left-=1;if s.restore_left==0{if let Some(g)=s.gravity_saved.take(){set_flag(gravity_flag(chr),g);}if let Some(b)=s.inv_saved.take(){crate::actors::set_invincible(chr,b);}unsafe{tm_render_lock_game_input(0)};crate::log_game("BONE_REPLAY: returned to the saved spot; controls unlocked");}}
  }
  s.host=(l.playing!=0,l.timescale,l.play_source_ns,l.received_ns);
  // No fade-out near the camera (foliage, trees, rocks, characters): applied as soon as Theater is connected, because objects
  // that are already in the world keep the values they were created with (the owner still saw fading after a late
  // override); areas loaded afterwards use the changed tables. Option bit 64 = off.
  camera_fade::tick(l.options&64==0&&l.linked!=0,now);
  // Update-LOD override (STEP A): on while recording, or while a replay with recorded actors owns the body.
  let want_omission=l.options&4==0&&(s.recording.is_some()||(s.owning&&s.loaded.as_ref().is_some_and(|l|l.actors.is_some())));
  if want_omission&&!omission::engaged(){if let Err(e)=omission::engage(){static WARN:std::sync::Once=std::sync::Once::new();WARN.call_once(||{crate::log_game(&format!("OMISSION_UNAVAILABLE: {e}"));status("OMISSION: update-level override unavailable (see log); distant actors may be recorded at a reduced rate");});}}
  else if !want_omission&&omission::engaged(){omission::release();}
  // After physics: put the root back where this frame's replay time says (physics may have moved it).
  if s.owning&&!matches!(s.arrival,Arrival::Warping{..}){if let Some(chr)=chr{if let Some(root)=root_now(s,chr){write_transform(chr,&root);note_written(s,&root);}}}
  if s.owning{if let Some(chr)=chr{advance_arrival(s,chr,now);}}
  if s.owning&&!matches!(s.arrival,Arrival::Warping{..}){apply_world(s);}
  return;}
 let Some(chr)=chr else {return;};
 if group==DRAW_GROUP&&!s.record_paused{
  if let Some(r)=&mut s.recording{
   let (Some((local,model)),Some(transform))=(pose_arrays(chr),read_transform(chr)) else {return;};
   let Some(count)=crate::actors::bone_count(chr) else{return;};let bytes=count*48;
   { // Calibration for the tile-offset model: where the frame (origin id) changes, log the continuous quantity.
    static LAST:std::sync::atomic::AtomicI32=std::sync::atomic::AtomicI32::new(i32::MIN);
    let p=arrival::place(chr);let f=p.frame();
    if LAST.swap(f,std::sync::atomic::Ordering::Relaxed)!=f{crate::log_game(&format!("FRAME_CHANGE: frame {} ({}) block {} anchor {:?} physics {:?}; physics-anchor = {:?}",f,arrival::block_name(f),arrival::block_name(p.block),&p.global[..3],&transform[2][..3],std::array::from_fn::<f32,3,_>(|k|transform[2][k]-p.global[k])));}
   }
   let frame=Frame{time:now,transform,matrix:read_matrix(matrix_address(chr)),local:pose(local,bytes).to_vec(),model:pose(model,bytes).to_vec(),place:arrival::place(chr),equip:equipment::read(chr).unwrap_or_default()};
   let definition=crate::skeleton::read(chr,0);
   if definition.is_none()||r.skeleton.as_ref().is_some_and(|d|Some(d)!=definition.as_ref()){
    r.dropped+=1;if !r.skeleton_warned{r.skeleton_warned=true;crate::log_game("PLAYER_CAPTURE_UNAVAILABLE: missing/changed skeleton identity; incompatible pose samples skipped");status("CAPTURE GAP: player skeleton unavailable or changed (see log).");}return;
   }
   let first_definition=if r.frames==0{definition.clone()}else{None};
   match r.tx.try_send(Message::PlayerPose(frame,first_definition)){Ok(())=>{if r.frames==0{r.first=now;r.skeleton=definition;}r.frames+=1;r.last=now;}Err(_)=>r.dropped+=1}
   {let fx=fx_relative(effects::drain(),r.first);if !fx.is_empty(){let _=r.tx.try_send(Message::Effects(fx));}let up=effects::drain_updates();if !up.is_empty(){let _=r.tx.try_send(Message::EffectUpdates(up));}}
   r.probe.sample(chr,now.saturating_sub(r.first) as f64/1e9);
   if let Some(m)=weapon_loc::read(chr){if r.module!=Some(m){r.module=Some(m);let _=r.tx.try_send(Message::PlayerModule(vec![world_file::ModuleSample{time:now,data:m}]));}}
   r.actors.sample(now,chr,&r.tx);}}
 if !s.owning{return;}
 match group{
  WRITE_GROUP=>{
   if s.loaded.as_ref().and_then(|l|l.world.skeletons.get(&0)).is_some_and(|d|crate::skeleton::read(chr,0).as_ref()!=Some(d)){
    status("REPLAY BLOCKED: player skeleton identity changed; pose not applied.");reject(s,chr,"skeleton identity mismatch");return;
   }
   let Some((local,model))=pose_arrays(chr) else {reject(s,chr,"player pose arrays unavailable");return;};
   if matches!(s.arrival,Arrival::Warping{..}){return;}
   // Keep the actual recorded physics root. The chunk field is only an origin guard.
   let here=update_live(s,chr);
   s.shift=[0.0;3];s.now_offset=[0.0;3];
   if !select(s,now,here){reject(s,chr,"invalid root interpolation or coordinate origin");return;}
   let (Some((f,next)),Some(loaded))=(&s.cur,&s.loaded) else {return;};
   if !crate::replay_interpolation::pose_safe_to_apply(&f.local)||!crate::replay_interpolation::pose_safe_to_apply(&f.model)
       ||!crate::replay_interpolation::pose_safe_to_apply(&next.local)||!crate::replay_interpolation::pose_safe_to_apply(&next.model){
    reject(s,chr,"invalid recorded bone coordinates/quaternion");status("REPLAY BLOCKED: invalid pose data; controls released.");return;
   }
   if crate::actors::bone_count(chr)!=Some(f.local.len()/48){reject(s,chr,"player bone count differs");return;}
   s.local_out.resize(f.local.len(),0);s.model_out.resize(f.model.len(),0);
   let alpha=if interpolated_pose_enabled(){s.pose_alpha}else{0.0};
   // Equipment first (grip, active slots, pieces), then the pose: the frame's full state, so scrubbing
   // backwards across a weapon swap reverts it.
   // Weapon model locations (hand / sheath / hidden while a flask is drunk): the state recorded at this replay time.
   s.module_written=None;
   if let Some(loaded)=&s.loaded{let m=&loaded.world.module;let i=m.partition_point(|x|x.time<=s.last_t);
    if i>0{let want=m[i-1].data;if weapon_loc::write(chr,&want){s.module_frames+=1;s.module_written=Some(want);}}}
   // SAFETY (owner report: armor vanished from the inventory after replays): the recorded equipment assembly is
   // written into the player only when the user turned "Replay equipment appearance" on (option bit 16, off by default).
   // Full gear is opt-in (bit 16). Weapon grip and slot selection (a weapon swap) are replayed by default (bit 128 turns that off):
   // they never touch an item handle, only which equipped slot is shown.
   s.equip_written=if s.options&16!=0&&f.equip.recorded()&&equipment::write(chr,&f.equip){s.equip_frames+=1;Some(f.equip)}
    else if s.options&128==0&&f.equip.recorded()&&equipment::write_selection(chr,&f.equip){s.equip_frames+=1;Some(f.equip)}else{None};
   // Bones whose interpolation is invalid keep the earlier recorded frame instead of dropping the replay.
   // Local pose is interpolated per bone; model space is rebuilt from it through the learned hierarchy.
   let Some(l)=crate::replay_interpolation::pose_into(&f.local,&next.local,alpha,&mut s.local_out) else {reject(s,chr,"pose size or time invalid");return;};
   let Some(m)=crate::replay_interpolation::model_from_local(&s.local_out,&loaded.parents,&f.model,&next.model,alpha,&mut s.model_out) else {reject(s,chr,"pose size or time invalid");return;};
   s.fallback_bones+=(l+m) as u64;
   unsafe{std::ptr::copy_nonoverlapping(s.local_out.as_ptr(),local as *mut u8,s.local_out.len());std::ptr::copy_nonoverlapping(s.model_out.as_ptr(),model as *mut u8,s.model_out.len());}
   if let Some(root)=s.evaluated_root{write_transform(chr,&root);note_written(s,&root);}set_flag(proxy_flag(chr),true);set_flag(gravity_flag(chr),true);
   // Recorded enemies/NPCs/bosses at the same replay time (Phase 2.2).
   let (t,live)=(s.last_t,(here.frame(),here.global));
   let anchors=s.loaded.as_ref().map(|l|l.anchors.clone());
   if let (Some(anchors),Some(a))=(anchors,s.loaded.as_mut().and_then(|l|l.actors.as_mut())){a.set_options(s.options);a.write(t,now,live,&anchors,interpolated_pose_enabled());}
   // Recorded one-shot effects (option bit 256): created at their recorded time while the replay plays forward. After a seek (or
   // a jump of more than half a second) the cursor is only moved, so scrubbing never fires a burst; playing on from there creates
   // the effects again, which is how they reappear after a rewind.
   effects::tick(t);
   if s.options&256==0&&effects::ready(){if let Some(loaded)=&s.loaded{let ev=&loaded.world.effects;
    // Pause re-evaluates the same moment and can step back a frame or two: only a real seek (back more than a quarter of a second,
    // or forward more than half a second) clears the effects.
    let jumped=s.fx_t==0||(t<s.fx_t&&s.fx_t-t>250_000_000)||(t>s.fx_t&&t-s.fx_t>500_000_000);
    if jumped{effects::release_all();}
    let mut i=if jumped{ev.partition_point(|e|e.time<=t)}else{s.fx_cursor.min(ev.len())};
    if !jumped{let mut made=0;while i<ev.len()&&ev[i].time<=t&&made<16{
     if let Some(sh)=if loaded.anchors.is_empty(){Some([0.0f32;3])}else{loaded.anchors.translation(ev[i].time,live.0,live.1)}{let p=[ev[i].pos[0]+sh[0],ev[i].pos[1]+sh[1],ev[i].pos[2]+sh[2]];let code=effects::spawn_scene(ev[i].id,p,ev[i].time.saturating_add(20_000_000_000),ev[i].time);let ok=code==1||code==2;if ok{made+=1;if loaded.world.effect_tracks.contains_key(&(ev[i].time,ev[i].id)){s.fx_live.push((ev[i].time,ev[i].id));}}let (before,after)=(code,code);
      static LOGGED:std::sync::atomic::AtomicU32=std::sync::atomic::AtomicU32::new(0);if LOGGED.fetch_add(1,std::sync::atomic::Ordering::Relaxed)<40{crate::log_game(&format!("EFFECT_REPLAY: id {} at t={:.2}s -> ({:.1},{:.1},{:.1}) called={} scene-create result {:?} (1 created, 2 created but empty handle, -1 FXR not resident, -4 check failed, -5 no scene, -7 no CSSfxImp) {:?} shift=({:.1},{:.1},{:.1}) replayed player at {:?}, recorded effect at {:?}",ev[i].id,ev[i].time as f64/1e9,p[0],p[1],p[2],ok,before,after,sh[0],sh[1],sh[2],read_transform(chr).map(|t|t[2]),ev[i].pos));}}
     i+=1;}}
    s.fx_cursor=i;s.fx_t=t;
    // Effects that followed something while recording (auras, trails, weapon effects) get their recorded matrix every frame.
    if jumped{s.fx_live.clear();}
    let tracks=&loaded.world.effect_tracks;let empty=loaded.anchors.is_empty();
    s.fx_live.retain(|k|{
     let Some(tr)=tracks.get(k) else {return false};let Some(last)=tr.last() else {return false};
     if t>last.0.saturating_add(1_000_000_000){return false;}
     if t<tr[0].0{return true;}
     let i=tr.partition_point(|x|x.0<=t);let mut m=tr[i.saturating_sub(1)].1;
     if i>0&&i<tr.len(){let (a,b)=(&tr[i-1],&tr[i]);let f=((t-a.0) as f64/((b.0-a.0).max(1)) as f64) as f32;for q in 12..15{m[q]=a.1[q]+(b.1[q]-a.1[q])*f;}}
     let sh=if empty{Some([0.0f32;3])}else{loaded.anchors.translation(k.0,live.0,live.1)};
     let Some(sh)=sh else {return true};for q in 0..3{m[12+q]+=sh[q];}
     effects::set_transform(k.0,k.1,&m)});}}}
  DRAW_GROUP=>{
   let Some((local,model))=pose_arrays(chr) else {return;};let drawn=read_matrix(matrix_address(chr));
   if let Some(e)=s.equip_written{if equipment::read(chr)!=Some(e){s.equip_lost+=1;}}
   if let Some(m)=s.module_written{if weapon_loc::read(chr)!=Some(m){s.module_lost+=1;}}
   if s.settle>0{s.settle-=1;return;}
   let a=&mut s.accuracy;a.frames+=1;if pose(local,s.local_out.len())==&s.local_out[..]&&pose(model,s.model_out.len())==&s.model_out[..]{a.exact_bones+=1;}
   let d=(0..3).map(|k|(drawn[12+k]-s.expected[k]).powi(2)).sum::<f32>().sqrt()*100.0;a.max_drawn_cm=a.max_drawn_cm.max(d);a.sum_drawn_cm+=d as f64;}
  _=>{}}
}

// Replay system Step 1: record the player's bone transforms and replay them exactly on the player
// character, in memory (no file yet). User decisions (2026-10-07): record only bone transforms,
// local and world space, no animation data (the game's animation is not deterministic); the native
// ghost is retired; the replay body is the player character, with controls locked during playback.
//
// Where the pose lives (skeleton probe K1, write tests K2-K4): ChrIns+0x398 is the
// CSFD4LocationHkaPoseImporter; +0x50 -> 150 local-space hkQsTransforms, +0x60 -> 150 model-space
// hkQsTransforms (48 bytes each). Writes in ChrIns_PrePhysicsSafe survive until Draw_Pre and are what
// is drawn; writes in ChrIns_BehaviorSafe are overwritten; writes after the importer show nothing.
// World placement comes from CSChrPhysicsModule orientation, interpolated_orientation, position.
//
// Frame flow (task groups registered in lib.rs):
//   0 ChrIns_PostPhysics: hotkeys and state changes; during playback rewrites the transform after physics
//   2 ChrIns_PrePhysicsSafe: during playback writes bones and transform
//   4 Draw_Pre: recording samples what is about to be drawn; playback measures what is about to be
//     drawn against the recorded frame (the accuracy numbers in the log)
// Test 1 (2026-10-07): moves matched and bones were exact on 99.9% of frames, physics position error
// 0 cm, but the body was drawn high in the sky and could take damage and die. So playback now also
// records and writes ChrIns.model_matrix (the final matrix the renderer uses, after easing and the
// vertical offset), logs drawn vs recorded matrices once a second, and makes the player immune to
// damage with gravity off while the replay owns the body.
// Offsets stay in this file until Step 2 moves them into GameProfile.
use std::ffi::{c_char,c_void,CString};
use std::sync::Mutex;
use std::sync::atomic::{AtomicU64,Ordering};
use eldenring::cs::{WorldChrMan,ChrIns,CSChrPhysicsModule};
use fromsoftware_shared::FromStatic;

const POSE_IMPORTER:usize=0x398;
const LOCAL_POSE:usize=0x50;
const MODEL_POSE:usize=0x60;
const BONES:usize=150;
const POSE_BYTES:usize=BONES*48;
const MAX_SECONDS:u64=120; // about 1.7 MB per second uncompressed; Step 2 adds the file format and compression
const KEYS_GROUP:usize=0;
const WRITE_GROUP:usize=2;
const DRAW_GROUP:usize=4;
const RESTORE_FRAMES:u32=10;
// shared/TheaterHotkeys.h: AnimProbe (F9) = record start/stop, GhostCreateTest (F10) = play/stop.
const RECORD_ACTION:u32=6;
const PLAY_ACTION:u32=4;

#[link(name="user32")]unsafe extern "system"{fn GetAsyncKeyState(key:i32)->i16;fn GetForegroundWindow()->*mut c_void;fn GetWindowThreadProcessId(window:*mut c_void,pid:*mut u32)->u32;}
#[link(name="kernel32")]unsafe extern "system"{fn GetCurrentProcessId()->u32;}
unsafe extern "C"{fn tm_hotkey_vk(action:u32)->u32;fn tm_render_native_status(text:*const c_char);fn tm_render_lock_game_input(locked:i32);}

type Transform=[[f32;4];3]; // orientation, interpolated_orientation, position
struct Frame{time:u64,local:Vec<u8>,model:Vec<u8>,transform:Transform,matrix:[f32;16]}
#[derive(Default)]struct Accuracy{frames:u64,exact_bones:u64,max_bone_error:f32,max_position_cm:f32,sum_position_cm:f64}
#[derive(PartialEq,Clone,Copy)]enum Mode{Idle,Recording,Playing,Restoring}
struct State{mode:Mode,start:u64,frames:Vec<Frame>,last_second:u64,written:Option<usize>,writes:u64,saved:Option<Transform>,restore_left:u32,accuracy:Accuracy,saved_flags:Option<(u32,bool)>,diag_second:u64}
static STATE:Mutex<State>=Mutex::new(State{mode:Mode::Idle,start:0,frames:Vec::new(),last_second:0,written:None,writes:0,saved:None,restore_left:0,accuracy:Accuracy{frames:0,exact_bones:0,max_bone_error:0.0,max_position_cm:0.0,sum_position_cm:0.0},saved_flags:None,diag_second:0});
static KEYS:AtomicU64=AtomicU64::new(0);

fn status(text:&str){if let Ok(c)=CString::new(text){unsafe{tm_render_native_status(c.as_ptr())}}}
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
fn physics_matrix_address(chr:usize)->usize{let c=ctrl(chr) as *const eldenring::cs::ChrCtrl;unsafe{&raw const (*c).physics_model_matrix as usize}}
fn read_matrix(a:usize)->[f32;16]{unsafe{std::ptr::read_volatile(a as *const [f32;16])}}
fn write_matrix(chr:usize,m:&[f32;16]){unsafe{std::ptr::write_volatile(matrix_address(chr) as *mut [f32;16],*m)}}
// ChrDebugFlags bit 3 = disabled_hit (ignore all incoming damage); CSChrPhysicsModule.gravity_disabled.
fn flags_address(chr:usize)->usize{let c=chr as *const ChrIns;unsafe{&raw const (*c).debug_flags as usize}}
fn gravity_address(chr:usize)->Option<usize>{let p=physics(chr)? as *const CSChrPhysicsModule;Some(unsafe{&raw const (*p).gravity_disabled as usize})}
fn protect(chr:usize)->Option<(u32,bool)>{
 let (f,g)=(flags_address(chr),gravity_address(chr)?);
 let saved=unsafe{(std::ptr::read_volatile(f as *const u32),std::ptr::read_volatile(g as *const bool))};
 unsafe{std::ptr::write_volatile(f as *mut u32,saved.0|1<<3);std::ptr::write_volatile(g as *mut bool,true);}Some(saved)}
fn keep_protected(chr:usize){let f=flags_address(chr);unsafe{std::ptr::write_volatile(f as *mut u32,std::ptr::read_volatile(f as *const u32)|1<<3);}if let Some(g)=gravity_address(chr){unsafe{std::ptr::write_volatile(g as *mut bool,true)}}}
fn unprotect(chr:usize,saved:(u32,bool)){let f=flags_address(chr);unsafe{let now=std::ptr::read_volatile(f as *const u32);std::ptr::write_volatile(f as *mut u32,(now&!(1<<3))|(saved.0&1<<3));}if let Some(g)=gravity_address(chr){unsafe{std::ptr::write_volatile(g as *mut bool,saved.1)}}}
fn pose_arrays(chr:usize)->Option<(usize,usize)>{
 let importer=read_ptr(chr+POSE_IMPORTER);if !object(importer){return None;}
 let (local,model)=(read_ptr(importer+LOCAL_POSE),read_ptr(importer+MODEL_POSE));
 (array(local)&&array(model)).then_some((local,model))}
fn pose(p:usize)->&'static [u8]{unsafe{std::slice::from_raw_parts(p as *const u8,POSE_BYTES)}}
fn max_float_error(a:&[u8],b:&[u8])->f32{a.chunks_exact(4).zip(b.chunks_exact(4)).map(|(x,y)|(f32::from_le_bytes(x.try_into().unwrap())-f32::from_le_bytes(y.try_into().unwrap())).abs()).fold(0.0,f32::max)}

// Rising edges of [record, play] while the game window is focused.
fn pressed()->[bool;2]{
 let mut pid=0;unsafe{GetWindowThreadProcessId(GetForegroundWindow(),&mut pid);}let focused=pid==unsafe{GetCurrentProcessId()};
 let down=[RECORD_ACTION,PLAY_ACTION].map(|a|{let k=unsafe{tm_hotkey_vk(a)} as i32;focused&&k!=0&&unsafe{GetAsyncKeyState(k)}<0});
 let now=(down[0] as u64)|((down[1] as u64)<<1);let was=KEYS.swap(now,Ordering::Relaxed);
 [down[0]&&was&1==0,down[1]&&was&2==0]}

fn duration(s:&State)->f64{s.frames.last().map(|f|f.time as f64/1e9).unwrap_or(0.0)}
fn stop_recording(s:&mut State){
 let seconds=duration(s);
 crate::log_game(&format!("BONE_REPLAY: recorded {} frames over {:.2} s ({:.1} fps, {} MB in memory)",s.frames.len(),seconds,s.frames.len() as f64/seconds.max(0.001),s.frames.len()*POSE_BYTES*2/1_048_576));
 s.mode=Mode::Idle;
 if s.frames.len()<2{s.frames.clear();status("BONE REPLAY: nothing recorded");return;}
 status(&format!("BONE REPLAY: recorded {seconds:.1} s. Press F10 to play it on your character"));}
fn start_playback(s:&mut State,now:u64){
 let Some(chr)=player_chr() else {status("BONE REPLAY: player not ready");return;};
 s.saved=read_transform(chr);if s.saved.is_none(){status("BONE REPLAY: player not ready");return;}
 s.saved_flags=protect(chr);s.diag_second=u64::MAX;
 s.mode=Mode::Playing;s.start=now;s.written=None;s.writes=0;s.accuracy=Accuracy::default();
 unsafe{tm_render_lock_game_input(1)};
 crate::log_game(&format!("BONE_REPLAY: playback started; {} frames, {:.2} s; controls locked; return transform saved",s.frames.len(),duration(s)));
 status("BONE REPLAY: playing on your character (controls locked). F10 stops");}
fn stop_playback(s:&mut State,reason:&str){
 let a=&s.accuracy;
 crate::log_game(&format!("BONE_REPLAY: playback {reason}; {} writes; accuracy over {} measured frames: bones exact {} ({:.1}%), max bone float error {:.6}, position error max {:.3} cm mean {:.3} cm",
  s.writes,a.frames,a.exact_bones,100.0*a.exact_bones as f64/(a.frames.max(1) as f64),a.max_bone_error,a.max_position_cm,a.sum_position_cm/(a.frames.max(1) as f64)));
 s.mode=Mode::Restoring;s.restore_left=RESTORE_FRAMES;
 status(&format!("BONE REPLAY: playback {reason}. Position error max {:.2} cm, bones exact {:.0}% of frames",a.max_position_cm,100.0*a.exact_bones as f64/(a.frames.max(1) as f64)));}

pub fn tick(group:usize,now:u64){
 let mut guard=STATE.lock().unwrap();let s=&mut *guard;
 if group==KEYS_GROUP{
  let [record,play]=pressed();
  match s.mode{
   Mode::Idle if record=>{if player_chr().and_then(pose_arrays).is_some(){s.frames=Vec::new();s.mode=Mode::Recording;s.start=now;s.last_second=0;crate::log_game("BONE_REPLAY: recording started");status("BONE REPLAY: recording. Press F9 to stop");}else{status("BONE REPLAY: player not ready, load in first");}}
   Mode::Idle if play=>{if s.frames.len()>=2{start_playback(s,now);}else{status("BONE REPLAY: nothing recorded yet. Press F9 to record");}}
   Mode::Recording if record=>stop_recording(s),
   Mode::Playing if play||record=>stop_playback(s,"stopped"),
   _=>{}}
  if s.mode==Mode::Recording{
   let elapsed=now.saturating_sub(s.start);
   if elapsed>MAX_SECONDS*1_000_000_000{stop_recording(s);}
   else{let second=elapsed/1_000_000_000;if second!=s.last_second{s.last_second=second;status(&format!("BONE REPLAY: recording {second} s (max {MAX_SECONDS} s). Press F9 to stop"));}}}
 }
 let Some(chr)=player_chr() else {return;};
 match s.mode{
  Mode::Recording if group==DRAW_GROUP=>{
   let (Some((local,model)),Some(transform))=(pose_arrays(chr),read_transform(chr)) else {return;};
   s.frames.push(Frame{time:now.saturating_sub(s.start),local:pose(local).to_vec(),model:pose(model).to_vec(),transform,matrix:read_matrix(matrix_address(chr))});}
  Mode::Playing=>{
   let elapsed=now.saturating_sub(s.start);
   if group==KEYS_GROUP&&elapsed>s.frames.last().map(|f|f.time).unwrap_or(0){stop_playback(s,"finished");return;}
   if group==WRITE_GROUP{
    // Pick the frame for this game frame once, so bones and transform always come from the same sample.
    let index=s.frames.partition_point(|f|f.time<=elapsed).saturating_sub(1);s.written=Some(index);
    let Some((local,model))=pose_arrays(chr) else {return;};let f=&s.frames[index];
    unsafe{std::ptr::copy_nonoverlapping(f.local.as_ptr(),local as *mut u8,POSE_BYTES);std::ptr::copy_nonoverlapping(f.model.as_ptr(),model as *mut u8,POSE_BYTES);}
    write_transform(chr,&f.transform);keep_protected(chr);s.writes+=1;}
   else if group==KEYS_GROUP{if let Some(i)=s.written{write_transform(chr,&s.frames[i].transform);write_matrix(chr,&s.frames[i].matrix);}}
   else if group==DRAW_GROUP{
    let Some(i)=s.written else {return;};let f=&s.frames[i];
    let (Some((local,model)),Some(t))=(pose_arrays(chr),read_transform(chr)) else {return;};
    // Once a second: where the game was about to draw the body (before our matrix write) vs the recording.
    let drawn=read_matrix(matrix_address(chr));let second=elapsed/1_000_000_000;
    if second!=s.diag_second{s.diag_second=second;let pm=read_matrix(physics_matrix_address(chr));
     crate::log_game(&format!("BONE_REPLAY_DIAG: t={second}s frame={i} model_matrix drawn=({:.2},{:.2},{:.2}) recorded=({:.2},{:.2},{:.2}) physics_matrix=({:.2},{:.2},{:.2}) physics_pos=({:.2},{:.2},{:.2}) recorded_pos=({:.2},{:.2},{:.2})",
      drawn[12],drawn[13],drawn[14],f.matrix[12],f.matrix[13],f.matrix[14],pm[12],pm[13],pm[14],t[2][0],t[2][1],t[2][2],f.transform[2][0],f.transform[2][1],f.transform[2][2]));}
    write_matrix(chr,&f.matrix);
    let (l,m)=(pose(local),pose(model));let a=&mut s.accuracy;a.frames+=1;
    if l==&f.local[..]&&m==&f.model[..]{a.exact_bones+=1;}else{a.max_bone_error=a.max_bone_error.max(max_float_error(l,&f.local)).max(max_float_error(m,&f.model));}
    let d=(0..3).map(|k|(t[2][k]-f.transform[2][k]).powi(2)).sum::<f32>().sqrt()*100.0;
    a.max_position_cm=a.max_position_cm.max(d);a.sum_position_cm+=d as f64;}
  }
  Mode::Restoring if group==KEYS_GROUP||group==WRITE_GROUP=>{
   if let Some(t)=s.saved{write_transform(chr,&t);}
   if group==KEYS_GROUP{s.restore_left=s.restore_left.saturating_sub(1);if s.restore_left==0{s.mode=Mode::Idle;if let Some(f)=s.saved_flags.take(){unprotect(chr,f);}unsafe{tm_render_lock_game_input(0)};crate::log_game("BONE_REPLAY: returned to the saved spot; controls unlocked");}}}
  _=>{}}
}

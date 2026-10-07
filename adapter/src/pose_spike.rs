// Bone record/replay spike (F9), the first real use of the skeleton approach.
// Findings so far (K1-K3): the player's pose lives in ChrIns+0x398 (CSFD4LocationHkaPoseImporter):
// +0x50 -> 150 local-space hkQsTransforms, +0x60 -> 150 model-space hkQsTransforms. Writes made in
// ChrIns_PrePhysicsSafe or LocationUpdate_PrePhysics survive until Draw_Pre and freeze the body on
// screen; writes in ChrIns_BehaviorSafe are overwritten, and writes after the importer show nothing.
// This spike: F9 starts recording both arrays every frame (sampled at Draw_Pre, i.e. what was drawn),
// F9 again stops and plays the recording back on the player's own body (written at
// ChrIns_PrePhysicsSafe, frame picked by elapsed time). Position is not recorded yet, so the body
// performs the recorded moves in place. Offsets stay here only for the spike; the real recorder
// moves them into GameProfile.
use std::ffi::{c_char,c_void,CString};
use std::sync::Mutex;
use std::sync::atomic::{AtomicU64,Ordering};
use eldenring::cs::WorldChrMan;
use fromsoftware_shared::FromStatic;

const POSE_IMPORTER:usize=0x398;
const LOCAL_POSE:usize=0x50;
const MODEL_POSE:usize=0x60;
const BONES:usize=150;
const BYTES:usize=BONES*48;
const MAX_SECONDS:u64=20;
// Task groups registered in lib.rs: 0 ChrIns_PostPhysics (keys), 2 ChrIns_PrePhysicsSafe (write), 4 Draw_Pre (record).
const WRITE_GROUP:usize=2;
const RECORD_GROUP:usize=4;

#[link(name="user32")]unsafe extern "system"{fn GetAsyncKeyState(key:i32)->i16;fn GetForegroundWindow()->*mut c_void;fn GetWindowThreadProcessId(window:*mut c_void,pid:*mut u32)->u32;}
#[link(name="kernel32")]unsafe extern "system"{fn GetCurrentProcessId()->u32;}
unsafe extern "C"{fn tm_hotkey_vk(action:u32)->u32;fn tm_render_native_status(text:*const c_char);}

#[derive(PartialEq,Clone,Copy)]enum Mode{Idle,Recording,Playing}
struct Frame{time:u64,local:Vec<u8>,model:Vec<u8>}
struct State{mode:Mode,start:u64,frames:Vec<Frame>,writes:u64,last_second:u64}
static STATE:Mutex<State>=Mutex::new(State{mode:Mode::Idle,start:0,frames:Vec::new(),writes:0,last_second:0});
static KEY:AtomicU64=AtomicU64::new(0);

fn status(text:&str){if let Ok(c)=CString::new(text){unsafe{tm_render_native_status(c.as_ptr())}}}
fn read_ptr(address:usize)->usize{if address<0x10000{return 0;}unsafe{std::ptr::read_volatile(address as *const usize)}}
fn array(p:usize)->bool{(0x10000000000..0x800000000000).contains(&p)&&p%16==0}
fn object(p:usize)->bool{(0x10000000000..0x800000000000).contains(&p)&&p%8==0}
fn pose_arrays()->Option<(usize,usize)>{
 let world=unsafe{WorldChrMan::instance()}.ok()?;let player=world.main_player.as_ref()?;
 let chr=&player.chr_ins as *const _ as usize;let importer=read_ptr(chr+POSE_IMPORTER);if !object(importer){return None;}
 let (local,model)=(read_ptr(importer+LOCAL_POSE),read_ptr(importer+MODEL_POSE));
 (array(local)&&array(model)).then_some((local,model))}
fn copy_out(p:usize)->Vec<u8>{unsafe{std::slice::from_raw_parts(p as *const u8,BYTES).to_vec()}}

fn key_pressed()->bool{
 let mut pid=0;unsafe{GetWindowThreadProcessId(GetForegroundWindow(),&mut pid);}
 let key=unsafe{tm_hotkey_vk(6)} as i32;let down=pid==unsafe{GetCurrentProcessId()}&&key!=0&&unsafe{GetAsyncKeyState(key)}<0;
 let was=KEY.swap(down as u64,Ordering::Relaxed)!=0;down&&!was}

fn stop_recording(s:&mut State,now:u64){
 let seconds=s.frames.last().map(|f|f.time as f64/1e9).unwrap_or(0.0);
 crate::log_game(&format!("BONE_REPLAY: recorded {} frames over {:.2} s ({:.1} fps, {} KB)",s.frames.len(),seconds,s.frames.len() as f64/seconds.max(0.001),s.frames.len()*BYTES*2/1024));
 if s.frames.len()<2{s.mode=Mode::Idle;status("BONE REPLAY (F9): nothing recorded");return;}
 s.mode=Mode::Playing;s.start=now;s.writes=0;
 status("BONE REPLAY (F9): playing back on your body. Stand still and watch!");}

pub fn tick(group:usize,now:u64){
 let mut s=STATE.lock().unwrap();
 if group==0{
  if key_pressed(){
   match s.mode{
    Mode::Idle=>{if pose_arrays().is_some(){*s=State{mode:Mode::Recording,start:now,frames:Vec::new(),writes:0,last_second:0};crate::log_game("BONE_REPLAY: recording started");status("BONE REPLAY (F9): recording your bones. Move around, then press F9 again");}
                 else{status("BONE REPLAY (F9): player not ready, load in first");crate::log_game("BONE_REPLAY: pose arrays not found");}}
    Mode::Recording=>stop_recording(&mut s,now),
    Mode::Playing=>{s.mode=Mode::Idle;crate::log_game(&format!("BONE_REPLAY: playback cancelled after {} writes",s.writes));status("BONE REPLAY (F9): playback stopped");}}
  }
  if s.mode==Mode::Recording&&now.saturating_sub(s.start)>MAX_SECONDS*1_000_000_000{stop_recording(&mut s,now);}
  if s.mode==Mode::Recording{let second=now.saturating_sub(s.start)/1_000_000_000;if second!=s.last_second{s.last_second=second;status(&format!("BONE REPLAY (F9): recording {second} s / {MAX_SECONDS} s. Press F9 to stop and play"));}}
  return;}
 match (s.mode,group){
  (Mode::Recording,RECORD_GROUP)=>{
   let Some((local,model))=pose_arrays() else {return;};let time=now.saturating_sub(s.start);
   s.frames.push(Frame{time,local:copy_out(local),model:copy_out(model)});}
  (Mode::Playing,WRITE_GROUP)=>{
   let elapsed=now.saturating_sub(s.start);
   let last=s.frames.last().map(|f|f.time).unwrap_or(0);
   if elapsed>last{s.mode=Mode::Idle;crate::log_game(&format!("BONE_REPLAY: playback finished; {} writes for {} recorded frames",s.writes,s.frames.len()));status("BONE REPLAY (F9): playback done. Did your body repeat what you did?");return;}
   let Some((local,model))=pose_arrays() else {return;};
   let index=s.frames.partition_point(|f|f.time<=elapsed).saturating_sub(1);
   let frame=&s.frames[index];
   unsafe{std::ptr::copy_nonoverlapping(frame.local.as_ptr(),local as *mut u8,BYTES);std::ptr::copy_nonoverlapping(frame.model.as_ptr(),model as *mut u8,BYTES);}
   s.writes+=1;}
  _=>{}}
}

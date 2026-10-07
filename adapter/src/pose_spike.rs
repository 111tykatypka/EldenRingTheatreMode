// Bone record/replay spike (F9), the first real use of the skeleton approach.
// Findings so far (K1-K3): the player's pose lives in ChrIns+0x398 (CSFD4LocationHkaPoseImporter):
// +0x50 -> 150 local-space hkQsTransforms, +0x60 -> 150 model-space hkQsTransforms. Writes made in
// ChrIns_PrePhysicsSafe or LocationUpdate_PrePhysics survive until Draw_Pre and freeze the body on
// screen; writes in ChrIns_BehaviorSafe are overwritten, and writes after the importer show nothing.
// This spike: F9 starts recording both arrays every frame (sampled at Draw_Pre, i.e. what was drawn),
// F9 again stops. K4 played it back on the player's own body (user: "almost perfect").
// K5: the recording also keeps the physics position and orientation, and plays back, looping, on a
// separate puppet: the native ghost (F10), whose pose arrays, position and orientation are written
// at ChrIns_PrePhysicsSafe and again at LocationUpdate_PrePhysics. Offsets stay here only for the spike; the real recorder
// moves them into GameProfile.
use std::ffi::{c_char,c_void,CString};
use std::sync::Mutex;
use std::sync::atomic::{AtomicU64,Ordering};
use eldenring::cs::{WorldChrMan,ChrIns,CSChrPhysicsModule};
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
const LATE_WRITE_GROUP:usize=3;

#[link(name="user32")]unsafe extern "system"{fn GetAsyncKeyState(key:i32)->i16;fn GetForegroundWindow()->*mut c_void;fn GetWindowThreadProcessId(window:*mut c_void,pid:*mut u32)->u32;}
#[link(name="kernel32")]unsafe extern "system"{fn GetCurrentProcessId()->u32;}
unsafe extern "C"{fn tm_hotkey_vk(action:u32)->u32;fn tm_render_native_status(text:*const c_char);fn tm_native_ghost_actor()->usize;}

#[derive(PartialEq,Clone,Copy)]enum Mode{Idle,Recording,WaitingForPuppet,Playing}
// Physics transform: orientation, interpolated_orientation, position (3 x 16 bytes).
struct Frame{time:u64,local:Vec<u8>,model:Vec<u8>,transform:[[f32;4];3]}
struct State{mode:Mode,start:u64,frames:Vec<Frame>,writes:u64,last_second:u64}
static STATE:Mutex<State>=Mutex::new(State{mode:Mode::Idle,start:0,frames:Vec::new(),writes:0,last_second:0});
static KEY:AtomicU64=AtomicU64::new(0);

fn status(text:&str){if let Ok(c)=CString::new(text){unsafe{tm_render_native_status(c.as_ptr())}}}
fn read_ptr(address:usize)->usize{if address<0x10000{return 0;}unsafe{std::ptr::read_volatile(address as *const usize)}}
fn array(p:usize)->bool{(0x10000000000..0x800000000000).contains(&p)&&p%16==0}
fn object(p:usize)->bool{(0x10000000000..0x800000000000).contains(&p)&&p%8==0}
fn player_chr()->Option<usize>{let world=unsafe{WorldChrMan::instance()}.ok()?;let player=world.main_player.as_ref()?;Some(&player.chr_ins as *const _ as usize)}
fn physics(chr:usize)->Option<usize>{if !object(chr){return None;}let p=unsafe{&*(chr as *const ChrIns)}.modules.physics.as_ref() as *const CSChrPhysicsModule as usize;object(p).then_some(p)}
fn transform_fields(p:usize)->[usize;3]{let m=p as *const CSChrPhysicsModule;unsafe{[&raw const (*m).orientation as usize,&raw const (*m).interpolated_orientation as usize,&raw const (*m).position as usize]}}
fn read_transform(chr:usize)->Option<[[f32;4];3]>{let p=physics(chr)?;Some(transform_fields(p).map(|a|unsafe{std::ptr::read_volatile(a as *const [f32;4])}))}
fn write_transform(chr:usize,t:&[[f32;4];3]){if let Some(p)=physics(chr){for(a,v)in transform_fields(p).iter().zip(t){unsafe{std::ptr::write_volatile(*a as *mut [f32;4],*v)}}}}
fn pose_arrays()->Option<(usize,usize)>{pose_arrays_of(player_chr()?)}
fn pose_arrays_of(chr:usize)->Option<(usize,usize)>{
 let importer=read_ptr(chr+POSE_IMPORTER);if !object(importer){return None;}
 let (local,model)=(read_ptr(importer+LOCAL_POSE),read_ptr(importer+MODEL_POSE));
 (array(local)&&array(model)).then_some((local,model))}
fn copy_out(p:usize)->Vec<u8>{unsafe{std::slice::from_raw_parts(p as *const u8,BYTES).to_vec()}}

fn key_pressed()->bool{
 let mut pid=0;unsafe{GetWindowThreadProcessId(GetForegroundWindow(),&mut pid);}
 let key=unsafe{tm_hotkey_vk(6)} as i32;let down=pid==unsafe{GetCurrentProcessId()}&&key!=0&&unsafe{GetAsyncKeyState(key)}<0;
 let was=KEY.swap(down as u64,Ordering::Relaxed)!=0;down&&!was}

fn stop_recording(s:&mut State,_now:u64){
 let seconds=s.frames.last().map(|f|f.time as f64/1e9).unwrap_or(0.0);
 crate::log_game(&format!("BONE_REPLAY: recorded {} frames over {:.2} s ({:.1} fps, {} KB)",s.frames.len(),seconds,s.frames.len() as f64/seconds.max(0.001),s.frames.len()*BYTES*2/1024));
 if s.frames.len()<2{s.mode=Mode::Idle;status("BONE REPLAY (F9): nothing recorded");return;}
 s.mode=Mode::WaitingForPuppet;s.writes=0;
 status(&format!("BONE REPLAY: recorded {seconds:.1} s. Press F10 to create the puppet; it will replay you (F9 stops)"));}

pub fn tick(group:usize,now:u64){
 let mut s=STATE.lock().unwrap();
 if group==0{
  if key_pressed(){
   match s.mode{
    Mode::Idle=>{if pose_arrays().is_some(){*s=State{mode:Mode::Recording,start:now,frames:Vec::new(),writes:0,last_second:0};crate::log_game("BONE_REPLAY: recording started");status("BONE REPLAY (F9): recording your bones. Move around, then press F9 again");}
                 else{status("BONE REPLAY (F9): player not ready, load in first");crate::log_game("BONE_REPLAY: pose arrays not found");}}
    Mode::Recording=>stop_recording(&mut s,now),
    Mode::WaitingForPuppet|Mode::Playing=>{s.mode=Mode::Idle;crate::log_game(&format!("BONE_REPLAY: playback cancelled after {} writes",s.writes));status("BONE REPLAY (F9): playback stopped");}}
  }
  // Only write bones onto a puppet that uses the same hkaSkeleton (importer+0x48) as the recording,
  // so the 150-bone copy can never overrun a smaller skeleton.
  if s.mode==Mode::WaitingForPuppet&&unsafe{tm_native_ghost_actor()}!=0{
   let skeleton=|chr:usize|{let i=read_ptr(chr+POSE_IMPORTER);if object(i){read_ptr(i+0x48)}else{0}};
   let (mine,theirs)=(player_chr().map(skeleton).unwrap_or(0),skeleton(unsafe{tm_native_ghost_actor()}));
   if mine==0||mine!=theirs{s.mode=Mode::Idle;crate::log_game(&format!("BONE_REPLAY_ERROR: puppet skeleton 0x{theirs:X} differs from the player's 0x{mine:X}; not writing bones"));status("BONE REPLAY: the puppet has a different skeleton; playback refused (see log)");return;}
  }
  if s.mode==Mode::WaitingForPuppet&&unsafe{tm_native_ghost_actor()}!=0{s.mode=Mode::Playing;s.start=now;crate::log_game(&format!("BONE_REPLAY: puppet 0x{:X} found; looping playback of {} frames",unsafe{tm_native_ghost_actor()},s.frames.len()));status("BONE REPLAY: the puppet is replaying you (loops). F9 stops, F11 removes it");}
  if s.mode==Mode::Playing&&unsafe{tm_native_ghost_actor()}==0{s.mode=Mode::Idle;crate::log_game(&format!("BONE_REPLAY: puppet gone; stopped after {} writes",s.writes));status("BONE REPLAY: puppet removed, playback stopped");}
  if s.mode==Mode::Recording&&now.saturating_sub(s.start)>MAX_SECONDS*1_000_000_000{stop_recording(&mut s,now);}
  if s.mode==Mode::Recording{let second=now.saturating_sub(s.start)/1_000_000_000;if second!=s.last_second{s.last_second=second;status(&format!("BONE REPLAY (F9): recording {second} s / {MAX_SECONDS} s. Press F9 to stop and play"));}}
  return;}
 match (s.mode,group){
  (Mode::Recording,RECORD_GROUP)=>{
   let Some(chr)=player_chr() else {return;};let Some((local,model))=pose_arrays_of(chr) else {return;};let Some(transform)=read_transform(chr) else {return;};
   let time=now.saturating_sub(s.start);
   s.frames.push(Frame{time,local:copy_out(local),model:copy_out(model),transform});}
  (Mode::Playing,WRITE_GROUP|LATE_WRITE_GROUP)=>{
   let puppet=unsafe{tm_native_ghost_actor()};if puppet==0{return;}
   let length=s.frames.last().map(|f|f.time).unwrap_or(0).max(1);
   let elapsed=now.saturating_sub(s.start)%length; // loop
   let index=s.frames.partition_point(|f|f.time<=elapsed).saturating_sub(1);
   let frame=&s.frames[index];
   write_transform(puppet,&frame.transform);
   if group==WRITE_GROUP{
    let Some((local,model))=pose_arrays_of(puppet) else {return;};
    unsafe{std::ptr::copy_nonoverlapping(frame.local.as_ptr(),local as *mut u8,BYTES);std::ptr::copy_nonoverlapping(frame.model.as_ptr(),model as *mut u8,BYTES);}
    s.writes+=1;if s.writes==1{crate::log_game(&format!("BONE_REPLAY: first puppet write local=0x{local:X} model=0x{model:X}"));}}}
  _=>{}}
}

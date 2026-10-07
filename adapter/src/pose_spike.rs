// Pose write spike (F9). Skeleton probe K1 found the player's bone pose in
// ChrIns+0x398 (CSFD4LocationHkaPoseImporter): +0x50 -> 150 local-space hkQsTransforms,
// +0x60 -> 150 model-space hkQsTransforms. This test snapshots both arrays when F9 is pressed and
// writes the snapshot back for 3 seconds at three different points of the frame, one after the
// other, so the user can tell which point makes the body hold the pose while they keep running.
// That point is where replay playback will write recorded bones onto the puppet.
// Offsets stay here only for the spike; the real recorder moves them into GameProfile.
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
const GROUPS:[&str;3]=["ChrIns_PostPhysics","LocationUpdate_PostCloth_Post","Draw_Pre"];

#[link(name="user32")]unsafe extern "system"{fn GetAsyncKeyState(key:i32)->i16;fn GetForegroundWindow()->*mut c_void;fn GetWindowThreadProcessId(window:*mut c_void,pid:*mut u32)->u32;}
#[link(name="kernel32")]unsafe extern "system"{fn GetCurrentProcessId()->u32;}
unsafe extern "C"{fn tm_hotkey_vk(action:u32)->u32;fn tm_render_native_status(text:*const c_char);}

struct Snapshot{local:Vec<u8>,model:Vec<u8>,writes:[u64;3]}
static START:AtomicU64=AtomicU64::new(0);
static SNAPSHOT:Mutex<Option<Snapshot>>=Mutex::new(None);
static KEY:AtomicU64=AtomicU64::new(0);
static SEGMENT:AtomicU64=AtomicU64::new(u64::MAX);

fn status(text:&str){if let Ok(c)=CString::new(text){unsafe{tm_render_native_status(c.as_ptr())}}}
fn read_ptr(address:usize)->usize{if address<0x10000{return 0;}unsafe{std::ptr::read_volatile(address as *const usize)}}
fn plausible(p:usize)->bool{(0x10000000000..0x800000000000).contains(&p)&&p%16==0}
fn pose_arrays()->Option<(usize,usize)>{
 let world=unsafe{WorldChrMan::instance()}.ok()?;let player=world.main_player.as_ref()?;
 let chr=&player.chr_ins as *const _ as usize;let importer=read_ptr(chr+POSE_IMPORTER);if !plausible(importer){return None;}
 let (local,model)=(read_ptr(importer+LOCAL_POSE),read_ptr(importer+MODEL_POSE));
 (plausible(local)&&plausible(model)).then_some((local,model))}
// Segment index for a time since F9: 0,1,2 = write in that group; None = pause or finished.
fn segment(elapsed_ms:u64)->(Option<usize>,u64){match elapsed_ms{0..3000=>(Some(0),0),3000..5000=>(None,1),5000..8000=>(Some(1),2),8000..10000=>(None,3),10000..13000=>(Some(2),4),_=>(None,5)}}

pub fn tick(group:usize,now:u64){
 if group==0{
  let mut pid=0;unsafe{GetWindowThreadProcessId(GetForegroundWindow(),&mut pid);}
  let key=unsafe{tm_hotkey_vk(6)} as i32;let down=pid==unsafe{GetCurrentProcessId()}&&key!=0&&unsafe{GetAsyncKeyState(key)}<0;
  let was=KEY.swap(down as u64,Ordering::Relaxed)!=0;
  if down&&!was&&START.load(Ordering::Acquire)==0{
   match pose_arrays(){
    Some((local,model))=>{
     let snap=unsafe{Snapshot{local:std::slice::from_raw_parts(local as *const u8,BYTES).to_vec(),model:std::slice::from_raw_parts(model as *const u8,BYTES).to_vec(),writes:[0;3]}};
     *SNAPSHOT.lock().unwrap()=Some(snap);SEGMENT.store(u64::MAX,Ordering::Relaxed);START.store(now.max(1),Ordering::Release);
     crate::log_game(&format!("POSE_SPIKE: snapshot taken local=0x{local:X} model=0x{model:X} bones={BONES}"));}
    None=>{status("POSE TEST (F9): player not ready, load in first");crate::log_game("POSE_SPIKE: pose arrays not found");}}
  }
 }
 let start=START.load(Ordering::Acquire);if start==0{return;}
 let elapsed=now.saturating_sub(start)/1_000_000;let (active,index)=segment(elapsed);
 if group==0&&SEGMENT.swap(index,Ordering::Relaxed)!=index{
  let writes=SNAPSHOT.lock().unwrap().as_ref().map(|s|s.writes).unwrap_or_default();
  crate::log_game(&format!("POSE_SPIKE: segment {index} at {elapsed} ms; writes so far {:?}",writes));
  match index{
   0=>status("POSE TEST 1/3 (F9): holding your pose. Keep running!"),
   2=>status("POSE TEST 2/3 (F9): holding your pose. Keep running!"),
   4=>status("POSE TEST 3/3 (F9): holding your pose. Keep running!"),
   5=>{status("POSE TEST (F9): done. Which of 1/3, 2/3, 3/3 froze your body?");START.store(0,Ordering::Release);*SNAPSHOT.lock().unwrap()=None;return;}
   _=>status("POSE TEST (F9): pause, moving normally...")}
 }
 if active!=Some(group){return;}
 let Some((local,model))=pose_arrays() else {return;};
 let mut guard=SNAPSHOT.lock().unwrap();let Some(snap)=guard.as_mut() else {return;};
 unsafe{
  std::ptr::copy_nonoverlapping(snap.local.as_ptr(),local as *mut u8,BYTES);
  std::ptr::copy_nonoverlapping(snap.model.as_ptr(),model as *mut u8,BYTES);}
 snap.writes[group]+=1;
 if snap.writes[group]==1{crate::log_game(&format!("POSE_SPIKE: first write in {}",GROUPS[group]));}
}

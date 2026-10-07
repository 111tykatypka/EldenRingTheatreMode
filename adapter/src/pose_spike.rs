// Pose write spike (F9), round 2: K2 wrote the arrays after the pose importer (PostPhysics,
// PostCloth, Draw_Pre) and nothing changed on screen, so this round writes earlier in the frame,
// after the behavior step and before LocationUpdate_PrePhysics, and also writes the two arrays
// hanging off importer+0x198. A Draw_Pre check logs whether each array still holds the snapshot.
// Round 1 notes: Skeleton probe K1 found the player's bone pose in
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
const BONES:usize=150;
const BYTES:usize=BONES*48;
// group 0 = ChrIns_PostPhysics (keys/timing), 1..3 = write points, 4 = Draw_Pre (check only)
const GROUPS:[&str;5]=["ChrIns_PostPhysics","ChrIns_BehaviorSafe","ChrIns_PrePhysicsSafe","LocationUpdate_PrePhysics","Draw_Pre"];
const ARRAYS:[&str;4]=["importer+50(local)","importer+60(model)","importer+198->A8","importer+198->B8"];

#[link(name="user32")]unsafe extern "system"{fn GetAsyncKeyState(key:i32)->i16;fn GetForegroundWindow()->*mut c_void;fn GetWindowThreadProcessId(window:*mut c_void,pid:*mut u32)->u32;}
#[link(name="kernel32")]unsafe extern "system"{fn GetCurrentProcessId()->u32;}
unsafe extern "C"{fn tm_hotkey_vk(action:u32)->u32;fn tm_render_native_status(text:*const c_char);}

struct Snapshot{data:Vec<Vec<u8>>,writes:[u64;5],held:[[u64;4];5],checks:[u64;5]}
static START:AtomicU64=AtomicU64::new(0);
static SNAPSHOT:Mutex<Option<Snapshot>>=Mutex::new(None);
static KEY:AtomicU64=AtomicU64::new(0);
static SEGMENT:AtomicU64=AtomicU64::new(u64::MAX);

fn status(text:&str){if let Ok(c)=CString::new(text){unsafe{tm_render_native_status(c.as_ptr())}}}
fn read_ptr(address:usize)->usize{if address<0x10000{return 0;}unsafe{std::ptr::read_volatile(address as *const usize)}}
fn plausible(p:usize)->bool{(0x10000000000..0x800000000000).contains(&p)&&p%16==0}
// Object pointers only need 8-byte alignment; bone arrays are 16-byte aligned. A missing extra
// array (importer+0x198) is reported as 0 and skipped instead of blocking the test.
fn object(p:usize)->bool{(0x10000000000..0x800000000000).contains(&p)&&p%8==0}
fn pose_arrays()->Option<[usize;4]>{
 let world=unsafe{WorldChrMan::instance()}.ok()?;let player=world.main_player.as_ref()?;
 let chr=&player.chr_ins as *const _ as usize;let importer=read_ptr(chr+POSE_IMPORTER);
 if !object(importer){crate::log_game(&format!("POSE_SPIKE2: importer pointer 0x{importer:X} not usable"));return None;}
 let extra=read_ptr(importer+0x198);
 let pick=|p:usize|if plausible(p){p}else{0};
 let a=[pick(read_ptr(importer+0x50)),pick(read_ptr(importer+0x60)),if object(extra){pick(read_ptr(extra+0xA8))}else{0},if object(extra){pick(read_ptr(extra+0xB8))}else{0}];
 if a[0]==0||a[1]==0{crate::log_game(&format!("POSE_SPIKE2: main pose arrays missing {a:X?} extra=0x{extra:X}"));return None;}
 Some(a)}
// Segment for a time since F9: Some(group) = write in that group; None = pause or finished.
fn segment(elapsed_ms:u64)->(Option<usize>,u64){match elapsed_ms{0..3000=>(Some(1),0),3000..5000=>(None,1),5000..8000=>(Some(2),2),8000..10000=>(None,3),10000..13000=>(Some(3),4),_=>(None,5)}}

pub fn tick(group:usize,now:u64){
 if group==0{
  let mut pid=0;unsafe{GetWindowThreadProcessId(GetForegroundWindow(),&mut pid);}
  let key=unsafe{tm_hotkey_vk(6)} as i32;let down=pid==unsafe{GetCurrentProcessId()}&&key!=0&&unsafe{GetAsyncKeyState(key)}<0;
  let was=KEY.swap(down as u64,Ordering::Relaxed)!=0;
  if down&&!was&&START.load(Ordering::Acquire)==0{
   match pose_arrays(){
    Some(a)=>{
     let data=a.iter().map(|p|if *p==0{Vec::new()}else{unsafe{std::slice::from_raw_parts(*p as *const u8,BYTES).to_vec()}}).collect();
     *SNAPSHOT.lock().unwrap()=Some(Snapshot{data,writes:[0;5],held:[[0;4];5],checks:[0;5]});SEGMENT.store(u64::MAX,Ordering::Relaxed);START.store(now.max(1),Ordering::Release);
     crate::log_game(&format!("POSE_SPIKE2: snapshot taken arrays={a:X?} bones={BONES}"));}
    None=>{status("POSE TEST (F9): player not ready, load in first");crate::log_game("POSE_SPIKE2: pose arrays not found");}}
  }
 }
 let start=START.load(Ordering::Acquire);if start==0{return;}
 let elapsed=now.saturating_sub(start)/1_000_000;let (active,index)=segment(elapsed);
 if group==0&&SEGMENT.swap(index,Ordering::Relaxed)!=index{
  if let Some(s)=SNAPSHOT.lock().unwrap().as_ref(){crate::log_game(&format!("POSE_SPIKE2: segment {index} at {elapsed} ms; writes {:?}; Draw_Pre checks {:?}; arrays still equal to snapshot at Draw_Pre {:?} ({:?})",s.writes,s.checks,s.held,ARRAYS));}
  match index{
   0=>status("POSE TEST 1/3 (F9): holding your pose. Keep running!"),
   2=>status("POSE TEST 2/3 (F9): holding your pose. Keep running!"),
   4=>status("POSE TEST 3/3 (F9): holding your pose. Keep running!"),
   5=>{status("POSE TEST (F9): done. Which of 1/3, 2/3, 3/3 froze your body?");START.store(0,Ordering::Release);*SNAPSHOT.lock().unwrap()=None;return;}
   _=>status("POSE TEST (F9): pause, moving normally...")}
 }
 let Some(active)=active else {return;};
 let Some(arrays)=pose_arrays() else {return;};
 let mut guard=SNAPSHOT.lock().unwrap();let Some(snap)=guard.as_mut() else {return;};
 if group==4{
  snap.checks[active]+=1;
  for(i,p)in arrays.iter().enumerate(){if *p==0||snap.data[i].is_empty(){continue;}if unsafe{std::slice::from_raw_parts(*p as *const u8,BYTES)}==&snap.data[i][..]{snap.held[active][i]+=1;}}
  return;}
 if group!=active{return;}
 for(i,p)in arrays.iter().enumerate(){if *p==0||snap.data[i].is_empty(){continue;}unsafe{std::ptr::copy_nonoverlapping(snap.data[i].as_ptr(),*p as *mut u8,BYTES);}}
 snap.writes[group]+=1;
 if snap.writes[group]==1{crate::log_game(&format!("POSE_SPIKE2: first write in {}",GROUPS[group]));}
}

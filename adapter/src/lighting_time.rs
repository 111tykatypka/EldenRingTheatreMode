//! Explicit manual time-of-day requests through the pinned SDK, not replay clock overrides.
//! Native requests may change save-backed world time. No event flags or save functions are patched.
use std::sync::{Mutex,atomic::{AtomicBool,AtomicI32,Ordering}};
use eldenring::cs::WorldAreaTime;
use fromsoftware_shared::FromStatic;
static REQUEST:AtomicI32=AtomicI32::new(-2); // -2 none, -1 restore, otherwise minutes
static CONNECTED:AtomicBool=AtomicBool::new(false);
struct State {owner:usize, original:Option<(u32,u32,u32)>,last_log:u64}
static STATE:Mutex<State>=Mutex::new(State{owner:0,original:None,last_log:0});
unsafe extern "C" {fn tm_lighting_time_observe(hour:f32,state:i32);}
#[unsafe(no_mangle)]pub extern "C" fn tm_lighting_time_request(minutes:i32){if (-1..=1439).contains(&minutes){REQUEST.store(minutes,Ordering::Release);}}
#[unsafe(no_mangle)]pub extern "C" fn tm_lighting_time_connected(connected:i32){CONNECTED.store(connected!=0,Ordering::Release);if connected==0{REQUEST.store(-1,Ordering::Release);}}
pub fn tick(context:bool){
 let Ok(mut state)=STATE.try_lock() else{return};
 if !context{state.owner=0;state.original=None;REQUEST.store(-2,Ordering::Release);unsafe{tm_lighting_time_observe(0.0,0);}return;}
 let Ok(w)= (unsafe{WorldAreaTime::instance_mut()})else{unsafe{tm_lighting_time_observe(0.0,0);}return};
 let owner=w as *mut WorldAreaTime as usize;
 if state.owner!=owner{state.owner=owner;state.original=None;}
 let hour=w.clock.hours() as u32;let minute=w.clock.minutes() as u32;let second=w.clock.seconds() as u32;
 if hour>=24||minute>=60||second>=60{unsafe{tm_lighting_time_observe(0.0,-1);}return;}
 let mut status=1;
 let request=REQUEST.swap(-2,Ordering::AcqRel);
 if request==-1||!CONNECTED.load(Ordering::Acquire){
  if let Some((h,m,s))=state.original.take(){w.request_time(h,m,s);status=3;crate::log_game("LIGHTING_TIME previous time-of-day requested; native blend pending");}
 }else if request>=0{
  if state.original.is_none(){state.original=Some((hour,minute,second));}
  let h=request as u32/60;let m=request as u32%60;w.request_time(h,m,0);status=2;
  let now=crate::monotonic_ns();if state.last_log==0||now.saturating_sub(state.last_log)>=1_000_000_000{state.last_log=now;crate::log_game(&format!("LIGHTING_TIME requested {h:02}:{m:02}; native transition pending"));}
 }
 // Published clock is observed, not the requested slider value or a claim of a visual result.
 unsafe{tm_lighting_time_observe(hour as f32+minute as f32/60.0+second as f32/3600.0,status);}
}

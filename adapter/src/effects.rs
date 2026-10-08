//! One-shot visual effects (particles, hit sparks, blood) captured while recording and created again during a replay.
//! The native side (native_ui/EldenRingEffectAdapter.cpp) hooks the game\'s effect-at-position function; this module drains the
//! captured events into the replay file and, on playback, asks the same function to create them at the recorded time. Effects
//! that follow a character (trails, auras) use other game paths and are not covered yet.
use crate::world_file::{EffectEvent,EffectUpdate};
#[repr(C)]#[derive(Clone,Copy,Default)]struct RawUpdate{created_ns:u64,id:u32,pad:u32,time_ns:u64,m:[f32;16]}
#[repr(C)]#[derive(Clone,Copy,Default)]struct Raw{time_ns:u64,id:u32,pos:[f32;3]}
unsafe extern "C"{
 fn tm_effect_initialize()->i32;fn tm_effect_capture(on:i32);fn tm_effect_drain(out:*mut Raw,max:u32)->u32;
 fn tm_effect_scene_spawn(manager:usize,id:u32,pos:*const f32,expires_at:u64,tag_time:u64)->i32;fn tm_effect_drain_updates(out:*mut RawUpdate,max:u32)->u32;fn tm_effect_scene_set_transform(tag_time:u64,id:u32,m:*const f32)->i32;fn tm_effect_scene_tick(replay_time:u64);fn tm_effect_update_stats(seen:*mut u64,dropped:*mut u64);fn tm_effect_decal_count()->u64;fn tm_effect_scene_end_stats(natural:*mut u64,cap:*mut u64);fn tm_effect_scene_stats(c:*mut u64,nr:*mut u64,f:*mut u64,ns:*mut u64);fn tm_effect_scene_release_all();
 fn tm_effect_spawn(id:u32,pos:*const f32)->i32;fn tm_effect_stats(seen:*mut u64,dropped:*mut u64,replayed:*mut u64);
}
static READY:std::sync::atomic::AtomicBool=std::sync::atomic::AtomicBool::new(false);
pub fn initialize(){let ok=unsafe{tm_effect_initialize()}!=0;READY.store(ok,std::sync::atomic::Ordering::Release);
 crate::log_game(if ok{"EFFECT_HOOK READY (effect-at-position function hooked; capture only while recording)"}else{"EFFECT_HOOK UNAVAILABLE (function bytes differ or hook failed); effects are neither recorded nor replayed"});}
pub fn ready()->bool{READY.load(std::sync::atomic::Ordering::Acquire)}
pub fn capture(on:bool){if ready(){unsafe{tm_effect_capture(on as i32)}}}
/// Captured effects since the last call, oldest first.
pub fn drain()->Vec<EffectEvent>{
 if !ready(){return Vec::new();}
 let mut out=Vec::new();let mut buffer=[Raw::default();256];
 loop{let n=unsafe{tm_effect_drain(buffer.as_mut_ptr(),buffer.len() as u32)} as usize;if n==0{break;}
  out.extend(buffer[..n].iter().map(|r|EffectEvent{time:r.time_ns,id:r.id,pos:r.pos}));if n<buffer.len(){break;}}
 out.retain(|e|e.pos.iter().all(|v|v.is_finite()));out.sort_by_key(|e|e.time);out}
/// Creates one effect now (a game task only). `pos` is in today\'s physics frame.
pub fn spawn(id:u32,pos:[f32;3])->bool{ready()&&unsafe{tm_effect_spawn(id,pos.as_ptr())}!=0}
/// Replay path: creates the effect through the game's scene controller (only effects whose FXR is resident). 1 created, 0 refused,
/// -1 not resident, -2 no free slot, -3 faulted, -4 function check failed, -5 scene controller unreadable, -7 CSSfxImp unavailable, 2 created but empty handle. Game callback thread only.
/// Per effect id: how many were created and how many were skipped (resource not loaded etc.) during this session's replays.
static PER_ID:std::sync::Mutex<std::collections::BTreeMap<u32,(u32,u32)>>=std::sync::Mutex::new(std::collections::BTreeMap::new());
pub fn skipped_report()->String{
 let map=PER_ID.lock().unwrap();let mut rows:Vec<(u32,u32,u32)>=map.iter().filter(|(_,v)|v.1>0).map(|(k,v)|(*k,v.1,v.0)).collect();
 rows.sort_by(|a,b|b.1.cmp(&a.1));rows.truncate(12);
 rows.iter().map(|(id,skipped,created)|format!("{id}: {skipped} skipped / {created} created")).collect::<Vec<_>>().join("; ")}
pub fn spawn_scene(id:u32,pos:[f32;3],expires_at:u64,tag_time:u64)->i32{
 use fromsoftware_shared::FromStatic;
 let Ok(m)=(unsafe{eldenring::cs::CSSfxImp::instance()}) else {return -7};
 let code=unsafe{tm_effect_scene_spawn(m as *const eldenring::cs::CSSfxImp as usize,id,pos.as_ptr(),expires_at,tag_time)};
 if let Ok(mut map)=PER_ID.lock(){let e=map.entry(id).or_insert((0,0));if code==1||code==2{e.0+=1}else{e.1+=1}}
 code}
/// Per-frame matrices of effects that follow something, captured since the last call.
pub fn drain_updates()->Vec<EffectUpdate>{
 if !ready(){return Vec::new();}
 let mut out=Vec::new();let mut buffer=vec![RawUpdate::default();512];
 loop{let n=unsafe{tm_effect_drain_updates(buffer.as_mut_ptr(),buffer.len() as u32)} as usize;if n==0{break;}
  out.extend(buffer[..n].iter().map(|r|EffectUpdate{created:r.created_ns,id:r.id,time:r.time_ns,m:r.m}));if n<buffer.len(){break;}}
 out.retain(|u|u.m.iter().all(|v|v.is_finite()));out.sort_by_key(|u|u.time);out}
/// Moves a replay-made effect to a recorded matrix; false when that effect no longer exists.
pub fn set_transform(tag_time:u64,id:u32,m:&[f32;16])->bool{ready()&&unsafe{tm_effect_scene_set_transform(tag_time,id,m.as_ptr())}!=0}
/// Stops and releases effects whose lifetime (in replay time) has ended: a paused replay keeps them, slow motion stretches them.
pub fn tick(replay_time:u64){unsafe{tm_effect_scene_tick(replay_time)}}
/// Stops and releases every effect the replay created (seek, unload, end).
pub fn release_all(){unsafe{tm_effect_scene_release_all()}
 static LAST:std::sync::atomic::AtomicU64=std::sync::atomic::AtomicU64::new(0);
 let (mut c,mut nr,mut f,mut ns)=(0u64,0u64,0u64,0u64);unsafe{tm_effect_scene_stats(&mut c,&mut nr,&mut f,&mut ns)}
 let (mut en,mut ec)=(0u64,0u64);unsafe{tm_effect_scene_end_stats(&mut en,&mut ec)}
 if c+nr+f+ns+en+ec!=LAST.swap(c+nr+f+ns+en+ec,std::sync::atomic::Ordering::Relaxed){crate::log_game(&format!("EFFECT_REPLAY_SUMMARY: {c} created, {nr} skipped because the effect resource was not loaded, {ns} recycled when the pool was full, {f} failed; {en} ended by themselves, {ec} stopped at the 20 s safety limit"));crate::log_game(&format!("EFFECT_REPLAY_SKIPPED_BY_ID: {}",skipped_report()));}}
pub fn update_stats()->(u64,u64){let (mut a,mut b)=(0u64,0u64);if ready(){unsafe{tm_effect_update_stats(&mut a,&mut b)}}(a,b)}
pub fn decal_count()->u64{if ready(){unsafe{tm_effect_decal_count()}}else{0}}
pub fn stats()->(u64,u64,u64){let (mut a,mut b,mut c)=(0u64,0u64,0u64);if ready(){unsafe{tm_effect_stats(&mut a,&mut b,&mut c)}}(a,b,c)}

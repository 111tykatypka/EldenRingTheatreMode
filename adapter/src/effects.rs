//! One-shot visual effects (particles, hit sparks, blood) captured while recording and created again during a replay.
//! The native side (native_ui/EldenRingEffectAdapter.cpp) hooks the game\'s effect-at-position function; this module drains the
//! captured events into the replay file and, on playback, asks the same function to create them at the recorded time. Effects
//! that follow a character (trails, auras) use other game paths and are not covered yet.
use crate::world_file::EffectEvent;
#[repr(C)]#[derive(Clone,Copy,Default)]struct Raw{time_ns:u64,id:u32,pos:[f32;3]}
unsafe extern "C"{
 fn tm_effect_initialize()->i32;fn tm_effect_capture(on:i32);fn tm_effect_drain(out:*mut Raw,max:u32)->u32;
 fn tm_effect_scene_spawn(manager:usize,id:u32,pos:*const f32,life_ms:u32)->i32;fn tm_effect_scene_tick();fn tm_effect_scene_release_all();
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
/// -1 not resident, -2 no free slot, -3 faulted. Game callback thread only.
pub fn spawn_scene(id:u32,pos:[f32;3],life_ms:u32)->i32{
 use fromsoftware_shared::FromStatic;
 let Ok(m)=(unsafe{eldenring::cs::CSSfxImp::instance()}) else {return 0};
 unsafe{tm_effect_scene_spawn(m as *const eldenring::cs::CSSfxImp as usize,id,pos.as_ptr(),life_ms)}}
/// Stops and releases effects whose lifetime has ended.
pub fn tick(){unsafe{tm_effect_scene_tick()}}
/// Stops and releases every effect the replay created (seek, unload, end).
pub fn release_all(){unsafe{tm_effect_scene_release_all()}}
pub fn stats()->(u64,u64,u64){let (mut a,mut b,mut c)=(0u64,0u64,0u64);if ready(){unsafe{tm_effect_stats(&mut a,&mut b,&mut c)}}(a,b,c)}

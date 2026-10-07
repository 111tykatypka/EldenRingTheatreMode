//! Getting the player to where a replay was recorded (Phase 1.4).
//!
//! Physics roots are Havok coordinates. The legacy field named `global` stores
//! ChrIns.chunk_position; real captures show that it can stay constant while the player
//! moves, then change with streaming. It is NOT a recorded global player position.
//! No cross-origin conversion or nearest-grace distance is proven by that field alone.
//! Playback must retain its origin guard until a validated conversion is available.
use eldenring::cs::{BlockId,ChrIns,CSLuaEventManImp,SoloParamRepository};
use fromsoftware_shared::FromStatic;
use pelite::pe64::{Pe,PeView};
use crate::game_profile as profile;

/// Legacy location metadata: block, origin and chunk field (NOT global moving root).
#[derive(Clone,Copy,Debug,Default,PartialEq)]
pub struct Place{pub block:i32,pub origin:i32,pub global:[f32;4]}
pub fn place(chr:usize)->Place{
 let c=unsafe{&*(chr as *const ChrIns)};
 Place{block:i32::from(c.block_id),origin:i32::from(c.block_origin),global:[c.chunk_position.0,c.chunk_position.1,c.chunk_position.2,c.chunk_position.3]}}
pub fn block_name(block:i32)->String{if block==-1{"(none)".into()}else{BlockId::from(block).to_string()}}
fn area(block:i32)->u8{BlockId::from(block).area()}
pub fn overworld(block:i32)->bool{block!=-1&&BlockId::from(block).is_overworld()}
pub fn distance(a:[f32;4],b:[f32;4])->f32{(0..3).map(|k|(a[k]-b[k]).powi(2)).sum::<f32>().sqrt()}

/// Whether reaching `target` from `current` needs a warp (loading screen) instead of a direct move.
/// Same legacy map, or two overworld tiles within streaming range, can be reached directly.
pub fn needs_warp(current:&Place,target:&Place)->bool{
 if target.block==-1||current.block==-1{return false;} // recordings without map data: same map assumed
 if overworld(current.block)&&overworld(target.block){return area(current.block)!=area(target.block)||distance(current.global,target.global)>400.0;}
 current.block!=target.block}

/// The grace closest to `target`: same map block first, then same area, nearest by position.
pub fn nearest_grace(target:&Place)->Option<(u32,String)>{
 let repo=unsafe{SoloParamRepository::instance()}.ok()?;
 let t=BlockId::from(target.block);
 let mut best:Option<(u32,f32)>=None;
 for e in repo.bonfire_warps.iter(){
  let Some(row)=repo.get_by_bonfire_warp_param_by_entity_id(e.bonfire_entity_id) else {continue};
  if row.area_no()!=t.area(){continue;}
  let same_block=row.grid_x_no()==t.block()&&row.grid_z_no()==t.region();
  // Grace positions are block-local; within the target block that is directly comparable.
  let d=if same_block{distance([row.pos_x(),row.pos_y(),row.pos_z(),0.0],[target.global[0],target.global[1],target.global[2],0.0]).min(5000.0)}
        else{10_000.0+((row.grid_x_no() as f32-t.block() as f32).powi(2)+(row.grid_z_no() as f32-t.region() as f32).powi(2)).sqrt()*256.0};
  if best.is_none_or(|(_,bd)|d<bd){best=Some((e.bonfire_entity_id,d));}
 }
 best.map(|(id,_)|(id,format!("grace {id}")))}

type WarpFn=unsafe extern "C" fn(usize,usize,u32)->usize;
fn warp_function()->Result<WarpFn,String>{
 static FOUND:std::sync::OnceLock<Result<usize,String>>=std::sync::OnceLock::new();
 let r=FOUND.get_or_init(||{
  let base=unsafe{crate::GetModuleHandleW(std::ptr::null())} as usize;if base==0{return Err("game module not found".into());}
  let pattern=pelite::pattern::parse(profile::AOB_LUA_WARP).map_err(|e|format!("bad warp pattern: {e}"))?;
  let pe=unsafe{PeView::module(base as *const u8)};let mut matches=pe.scanner().matches_code(&pattern);
  let mut save=[0u32;2];let mut found=Vec::new();while matches.next(&mut save){found.push(save[1]);}
  if found.len()!=1{return Err(format!("warp function pattern matched {} times (need exactly 1)",found.len()));}
  crate::log_game(&format!("ARRIVAL: grace warp function at eldenring.exe+0x{:X}",found[0]));Ok(base+found[0] as usize)});
 r.clone().map(|a|unsafe{std::mem::transmute::<usize,WarpFn>(a)})}

/// Asks the game to fast travel to `grace`. Must run on a game task.
pub fn warp_to_grace(grace:u32)->Result<(),String>{
 let f=warp_function()?;
 let man=unsafe{CSLuaEventManImp::instance()}.map_err(|_|"CSLuaEventManager not available".to_string())?;
 let base=man as *const CSLuaEventManImp as usize;
 let read=|o:usize|unsafe{std::ptr::read_volatile((base+o) as *const usize)};
 let (imitation,proxy)=(read(profile::OFF_LUA_EVENT_MAN_IMITATION),read(profile::OFF_LUA_EVENT_MAN_PROXY));
 if imitation==0||proxy==0{return Err("lua event objects not ready".into());}
 let id=grace.checked_sub(profile::VAL_GRACE_ID_BIAS as u32).ok_or("grace id too small")?;
 unsafe{f(imitation,proxy,id)};Ok(())}

/// True while the game shows a loading screen or is still bringing the map in.
pub fn loading()->bool{
 match unsafe{CSLuaEventManImp::instance()}{Ok(m)=>m.lua_event_proxy.is_load_wait,Err(_)=>true}}

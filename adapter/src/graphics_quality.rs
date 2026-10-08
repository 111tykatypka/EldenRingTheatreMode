//! Global shadow and volumetric quality overrides for the lights tab.
//!
//! The game builds its shadow maps, local-light shadows and volumetric fog from three quality parameter tables (one row per
//! graphics quality level): `CS_SHADOW_QUALITY_DETAIL` (shadow map size, filter, blur), `CS_LIGHTING_QUALITY_DETAIL` (local light
//! shadows and their distance) and `CS_VOLUMETRIC_EFFECT_QUALITY_DETAIL` (fog, fog shadows and fog volume sampling). The overlay holds
//! 16 integer settings; while the master switch is on, every row of those tables is replaced in memory with the chosen values.
//! The untouched rows are remembered and put back when the switch is turned off or the repository is replaced. Changes in the
//! tables are normally picked up when the game applies a graphics quality level (changing the quality in the game menu, or
//! loading an area), not necessarily the same frame.
use eldenring::cs::{SoloParamRepository,WorldChrMan};
use eldenring::param::{CS_LIGHTING_QUALITY_DETAIL,CS_SHADOW_QUALITY_DETAIL,CS_VOLUMETRIC_EFFECT_QUALITY_DETAIL,ParamDef};
use fromsoftware_shared::FromStatic;
use std::panic::{catch_unwind,AssertUnwindSafe};
use std::sync::Mutex;

/// "Leave the engine value alone".
const DEFAULT:i32=i32::MIN;
// Setting slots (same order as the overlay writes them).
const MASTER:usize=0;const SHADOW_SIZE:usize=1;const FILTER:usize=2;const BLUR:usize=3;const LOCAL_SHADOWS:usize=4;const LOCAL_LEVEL:usize=5;
const LOCAL_DIST:usize=6;const FOG:usize=7;const FOG_SHADOW:usize=8;const FOG_SAMPLES:usize=9;const FOG_LIGHT_DIST:usize=10;const VOLUME:usize=11;
const VOLUME_SHADOW:usize=12;const VOLUME_FORCE:usize=13;const VOLUME_RES:usize=14;const VOLUME_RAY:usize=15;

struct State{root:usize,applied:Option<[i32;16]>,shadow:Vec<(u32,CS_SHADOW_QUALITY_DETAIL)>,light:Vec<(u32,CS_LIGHTING_QUALITY_DETAIL)>,fog:Vec<(u32,CS_VOLUMETRIC_EFFECT_QUALITY_DETAIL)>,next_try:u64,faulted:bool}
static STATE:Mutex<State>=Mutex::new(State{root:0,applied:None,shadow:Vec::new(),light:Vec::new(),fog:Vec::new(),next_try:0,faulted:false});

fn cap<'a>(repo:&'a mut SoloParamRepository,name:&str)->Option<&'a mut eldenring::fd4::FD4ParamResCap>{repo.params_mut().find(|p|p.struct_name()==name)}

fn restore(state:&mut State,repo:&mut SoloParamRepository){
 if repo as *mut SoloParamRepository as usize==state.root{
  let (mut a,mut b,mut c)=(0,0,0);
  let _=catch_unwind(AssertUnwindSafe(||{if let Some(cap)=cap(repo,CS_SHADOW_QUALITY_DETAIL::NAME){for (id,saved) in &state.shadow{if let Some(row)=unsafe{cap.get_mut::<CS_SHADOW_QUALITY_DETAIL>(*id)}{*row=saved.clone();a+=1;}}}}));
  let _=catch_unwind(AssertUnwindSafe(||{if let Some(cap)=cap(repo,CS_LIGHTING_QUALITY_DETAIL::NAME){for (id,saved) in &state.light{if let Some(row)=unsafe{cap.get_mut::<CS_LIGHTING_QUALITY_DETAIL>(*id)}{*row=saved.clone();b+=1;}}}}));
  let _=catch_unwind(AssertUnwindSafe(||{if let Some(cap)=cap(repo,CS_VOLUMETRIC_EFFECT_QUALITY_DETAIL::NAME){for (id,saved) in &state.fog{if let Some(row)=unsafe{cap.get_mut::<CS_VOLUMETRIC_EFFECT_QUALITY_DETAIL>(*id)}{*row=saved.clone();c+=1;}}}}));
  crate::log_game(&format!("GFX_QUALITY: restored {a} shadow, {b} lighting and {c} volumetric quality rows"));
 }else if state.applied.is_some(){crate::log_game("GFX_QUALITY: the parameter repository was replaced; old rows left alone");}
 state.shadow.clear();state.light.clear();state.fog.clear();state.applied=None;state.root=0;}

fn set(v:&[i32;16],slot:usize)->Option<i32>{if v[slot]==DEFAULT{None}else{Some(v[slot])}}

/// Once per game frame. `values` = the overlay's 16 settings (slot 0 = master switch).
pub fn tick(values:&[i32;16],now:u64){
 let mut state=STATE.lock().unwrap_or_else(|e|e.into_inner());
 let active=values[MASTER]!=0;
 if state.faulted||(!active&&state.applied.is_none()){return;}
 if active&&state.applied.is_none()&&(now<state.next_try||unsafe{WorldChrMan::instance()}.is_err()){return;}
 let Ok(repo)=(unsafe{SoloParamRepository::instance_mut()}) else {return};
 let root=repo as *mut SoloParamRepository as usize;
 if !active{restore(&mut state,repo);return;}
 if state.applied==Some(*values)&&state.root==root{return;}
 if state.applied.is_some()&&state.root!=root{restore(&mut state,repo);}
 state.root=root;state.next_try=now+3_000_000_000;
 let v=*values;
 let first=state.applied.is_none();
 let ok=catch_unwind(AssertUnwindSafe(||{
  let mut counts=(0usize,0usize,0usize);
  if let Some(cap)=cap(repo,CS_SHADOW_QUALITY_DETAIL::NAME){
   if first{state.shadow=unsafe{cap.data.rows_mut::<CS_SHADOW_QUALITY_DETAIL>()}.map(|(id,row)|(id,row.clone())).collect();}
   for (id,saved) in &state.shadow{if let Some(row)=unsafe{cap.get_mut::<CS_SHADOW_QUALITY_DETAIL>(*id)}{
    *row=saved.clone();
    if let Some(size)=set(&v,SHADOW_SIZE){let size=size.clamp(256,16384) as u32;row.set_texture_min_size(size);row.set_texture_max_size(size);}
    if let Some(x)=set(&v,FILTER){row.set_max_filter_level(x.clamp(0,8) as u8);}
    if let Some(x)=set(&v,BLUR){row.set_blur_count_bias(x.clamp(-8,16));}
    counts.0+=1;}}}
  if let Some(cap)=cap(repo,CS_LIGHTING_QUALITY_DETAIL::NAME){
   if first{state.light=unsafe{cap.data.rows_mut::<CS_LIGHTING_QUALITY_DETAIL>()}.map(|(id,row)|(id,row.clone())).collect();}
   for (id,saved) in &state.light{if let Some(row)=unsafe{cap.get_mut::<CS_LIGHTING_QUALITY_DETAIL>(*id)}{
    *row=saved.clone();
    if let Some(x)=set(&v,LOCAL_SHADOWS){row.set_local_light_shadow_enabled((x!=0) as u8);}
    if let Some(x)=set(&v,LOCAL_LEVEL){row.set_local_light_shadow_spec_level_max(x.clamp(0,5) as u8);}
    if let Some(x)=set(&v,LOCAL_DIST){let base=saved.local_light_dist_factor();row.set_local_light_dist_factor(base*(x.clamp(25,800) as f32)/100.0);}
    counts.1+=1;}}}
  if let Some(cap)=cap(repo,CS_VOLUMETRIC_EFFECT_QUALITY_DETAIL::NAME){
   if first{state.fog=unsafe{cap.data.rows_mut::<CS_VOLUMETRIC_EFFECT_QUALITY_DETAIL>()}.map(|(id,row)|(id,row.clone())).collect();}
   for (id,saved) in &state.fog{if let Some(row)=unsafe{cap.get_mut::<CS_VOLUMETRIC_EFFECT_QUALITY_DETAIL>(*id)}{
    *row=saved.clone();
    if let Some(x)=set(&v,FOG){row.set_fog_enabled((x!=0) as u8);}
    if let Some(x)=set(&v,FOG_SHADOW){row.set_fog_shadow_enabled((x!=0) as u8);}
    if let Some(x)=set(&v,FOG_SAMPLES){row.set_fog_shadow_sample_count_bias(x.clamp(-16,16));}
    if let Some(x)=set(&v,FOG_LIGHT_DIST){let base=saved.fog_local_light_dist_scale();row.set_fog_local_light_dist_scale(base*(x.clamp(25,800) as f32)/100.0);}
    if let Some(x)=set(&v,VOLUME){row.set_fog_volume_enabled((x!=0) as u8);}
    if let Some(x)=set(&v,VOLUME_SHADOW){row.set_fog_volume_shadow_enabled((x!=0) as u8);}
    if let Some(x)=set(&v,VOLUME_FORCE){row.set_fog_volume_force_shadowing((x!=0) as u8);}
    if let Some(x)=set(&v,VOLUME_RES){row.set_fog_volume_resolution(x.clamp(0,8) as u8);}
    if let Some(x)=set(&v,VOLUME_RAY){row.set_fog_volume_ray_marcing_sample_count_offset(x.clamp(-8,8) as i8);}
    counts.2+=1;}}}
  counts}));
 match ok{
  Ok(counts)=>{state.applied=Some(v);crate::log_game(&format!("GFX_QUALITY: applied to {} shadow, {} lighting and {} volumetric quality rows (settings {:?})",counts.0,counts.1,counts.2,v.iter().map(|x|if *x==DEFAULT{"-".to_string()}else{x.to_string()}).collect::<Vec<_>>()));}
  Err(_)=>{state.faulted=true;crate::log_game("GFX_QUALITY: reading the quality tables failed; the feature is disabled until the game restarts");}}
}

//! No fade-out when the camera gets close to things (foliage, trees, rocks, characters).
//!
//! The game fades objects the camera comes near. Parameter tables involved (SDK-typed rows, STATIC; field meanings from
//! the paramdef texts of the Elden Ring Debug Tool):
//! - `ASSET_GEOMETORY_PARAM_ST.cam_near_behavior_type` for map assets: 0 never disappears; every other value is cleared.
//! - `CHR_MODEL_PARAM_ST.camera_dither_fade_id` for characters and models: -1 = take it from the material, 0 = never
//!   disappears, 1.. = a CAMERA_FADE_PARAM_ST row. Every row is set to 0.
//! - `CAMERA_FADE_PARAM_ST`: alpha is 0 at NearMinDist, ramps to MiddleAlpha at NearMaxDist, stays MiddleAlpha until
//!   FarMinDist, ramps to 1 at FarMaxDist. The near range is moved to (-1, 0) and MiddleAlpha set to 1.
//! - Grass `dithering` bytes (LOG: all 501 rows of the first grass table are already 0, so this is a no-op there).
//! Everything is changed in memory only, remembered row by row and put back exactly when the feature is switched off or
//! the repository is replaced. Each table is processed on its own, so one table that cannot be read does not stop the
//! others; the parameter repository is only touched once the world exists (at start-up it is not filled yet, and the
//! first version of this file latched an error there and never applied anything).
use eldenring::cs::{AssetEnvironmentGeometryParam,ChrModelParam,GrassTypeParam,GrassTypeParam_Lv1,GrassTypeParam_Lv2,SoloParamRepository,WorldChrMan};
use eldenring::param::{CAMERA_FADE_PARAM_ST,ParamDef};
use fromsoftware_shared::FromStatic;
use std::panic::{catch_unwind,AssertUnwindSafe};
use std::sync::Mutex;

struct State{root:usize,applied:bool,assets:Vec<(u32,i8)>,fades:Vec<(u32,f32,f32,f32)>,models:Vec<(u32,i16)>,grass:Vec<(u8,u32,u8)>,failures:u32,next_try:u64,faulted:bool,sections:[bool;4]}
static STATE:Mutex<State>=Mutex::new(State{root:0,applied:false,assets:Vec::new(),fades:Vec::new(),models:Vec::new(),grass:Vec::new(),failures:0,next_try:0,faulted:false,sections:[false;4]});

fn camera_fade_cap(repo:&mut SoloParamRepository)->Option<&mut eldenring::fd4::FD4ParamResCap>{
 repo.params_mut().find(|p|p.struct_name()==CAMERA_FADE_PARAM_ST::NAME)}

fn restore(state:&mut State,repo:&mut SoloParamRepository){
 let (mut a,mut f,mut m,mut g)=(0,0,0,0);
 if repo as *mut SoloParamRepository as usize==state.root{
  let _=catch_unwind(AssertUnwindSafe(||{for &(id,v) in &state.assets{if let Some(row)=repo.get_mut::<AssetEnvironmentGeometryParam>(id){if row.cam_near_behavior_type()==0{row.set_cam_near_behavior_type(v);a+=1;}}}}));
  let _=catch_unwind(AssertUnwindSafe(||{if let Some(cap)=camera_fade_cap(repo){for &(id,min,max,mid) in &state.fades{
   if let Some(row)=unsafe{cap.get_mut::<CAMERA_FADE_PARAM_ST>(id)}{if row.near_min_dist()==-1.0&&row.near_max_dist()==0.0{row.set_near_min_dist(min);row.set_near_max_dist(max);row.set_middle_alpha(mid);f+=1;}}}}}));
  let _=catch_unwind(AssertUnwindSafe(||{for &(id,v) in &state.models{if let Some(row)=repo.get_mut::<ChrModelParam>(id){if row.camera_dither_fade_id()==0{row.set_camera_dither_fade_id(v);m+=1;}}}}));
  let _=catch_unwind(AssertUnwindSafe(||{for &(table,id,v) in &state.grass{let row=match table{0=>repo.get_mut::<GrassTypeParam>(id),1=>repo.get_mut::<GrassTypeParam_Lv1>(id),_=>repo.get_mut::<GrassTypeParam_Lv2>(id)};if let Some(row)=row{if row.dithering()==0{row.set_dithering(v);g+=1;}}}}));
  crate::log_game(&format!("CAMERA_FADE: restored {a} of {} asset rows, {f} of {} camera-fade rows, {m} of {} model rows, {g} of {} grass rows",state.assets.len(),state.fades.len(),state.models.len(),state.grass.len()));
 }else if state.applied{crate::log_game("CAMERA_FADE: the parameter repository was replaced; old rows left alone");}
 state.assets.clear();state.fades.clear();state.models.clear();state.grass.clear();state.applied=false;state.root=0;state.sections=[false;4];}

/// Call once per game frame; `active` = the feature is wanted right now. Never panics into the game.
pub fn tick(active:bool,now:u64){
 let mut state=STATE.lock().unwrap_or_else(|e|e.into_inner());
 if state.faulted||(!active&&!state.applied){return;}
 if active&&!state.applied&&(now<state.next_try||unsafe{WorldChrMan::instance()}.is_err()){return;}
 let Ok(repo)=(unsafe{SoloParamRepository::instance_mut()}) else {return};
 let root=repo as *mut SoloParamRepository as usize;
 if !active{restore(&mut state,repo);return;}
 if state.applied&&state.root==root{return;}
 if state.applied{restore(&mut state,repo);}
 state.root=root;
 let mut near_hist=std::collections::BTreeMap::<i8,u32>::new();
 let ok_assets=catch_unwind(AssertUnwindSafe(||{let mut out=Vec::new();
  // Every value except 0 ("never disappears") is switched, including -1 and unknown values.
  for (id,row) in repo.rows_mut::<AssetEnvironmentGeometryParam>(){let v=row.cam_near_behavior_type();*near_hist.entry(v).or_default()+=1;if v!=0{out.push((id,v));row.set_cam_near_behavior_type(0);}}out}));
 let ok_fades=catch_unwind(AssertUnwindSafe(||{let mut out=Vec::new();
  if let Some(cap)=camera_fade_cap(repo){for (id,row) in unsafe{cap.data.rows_mut::<CAMERA_FADE_PARAM_ST>()}{
   out.push((id,row.near_min_dist(),row.near_max_dist(),row.middle_alpha()));row.set_near_min_dist(-1.0);row.set_near_max_dist(0.0);row.set_middle_alpha(1.0);}}out}));
 let ok_models=catch_unwind(AssertUnwindSafe(||{let mut out=Vec::new();
  for (id,row) in repo.rows_mut::<ChrModelParam>(){let v=row.camera_dither_fade_id();if v!=0{out.push((id,v));row.set_camera_dither_fade_id(0);}}out}));
 let mut grass_hist=std::collections::BTreeMap::<u8,u32>::new();
 let ok_grass=catch_unwind(AssertUnwindSafe(||{let mut out=Vec::new();
  for (id,row) in repo.rows_mut::<GrassTypeParam>(){let v=row.dithering();*grass_hist.entry(v).or_default()+=1;if v!=0{out.push((0u8,id,v));row.set_dithering(0);}}
  for (id,row) in repo.rows_mut::<GrassTypeParam_Lv1>(){let v=row.dithering();*grass_hist.entry(v).or_default()+=1;if v!=0{out.push((1,id,v));row.set_dithering(0);}}
  for (id,row) in repo.rows_mut::<GrassTypeParam_Lv2>(){let v=row.dithering();*grass_hist.entry(v).or_default()+=1;if v!=0{out.push((2,id,v));row.set_dithering(0);}}out}));
 fn why<T>(r:&Result<T,Box<dyn std::any::Any+Send>>)->String{match r{Ok(_)=>"ok".into(),Err(e)=>e.downcast_ref::<String>().cloned().or_else(||e.downcast_ref::<&str>().map(|s|s.to_string())).unwrap_or_else(||"panic".into())}}
 crate::log_game(&format!("CAMERA_FADE: table results: assets={}, camera-fade={}, models={}, grass={}",why(&ok_assets),why(&ok_fades),why(&ok_models),why(&ok_grass)));
 state.sections=[ok_assets.is_ok(),ok_fades.is_ok(),ok_models.is_ok(),ok_grass.is_ok()];
 if let Ok(v)=ok_assets{state.assets=v;}
 if let Ok(v)=ok_fades{state.fades=v;}
 if let Ok(v)=ok_models{state.models=v;}
 if let Ok(v)=ok_grass{state.grass=v;}
 if state.sections.iter().any(|s|*s){
  state.applied=true;state.failures=0;
  crate::log_game(&format!("CAMERA_FADE: no near-camera fade; tables processed [assets,camera-fade,models,grass] = {:?}; {} asset rows set to 'never disappears', {} camera-fade rows set to opaque, {} model rows set to 'never disappears', {} grass rows cleared; original asset camera-near values {near_hist:?}; original grass dithering values {grass_hist:?}",state.sections,state.assets.len(),state.fades.len(),state.models.len(),state.grass.len()));
 }else{
  state.failures+=1;state.next_try=now+3_000_000_000;
  crate::log_game(&format!("CAMERA_FADE: no table could be read yet (attempt {}); trying again in 3 s",state.failures));
  if state.failures>=6{state.faulted=true;crate::log_game("CAMERA_FADE ERROR: the parameter tables never became readable; feature disabled for this session");}}
}

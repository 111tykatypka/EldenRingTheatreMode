//! No fade-out when the camera gets close to things (foliage, trees, rocks, characters).
//!
//! The game fades objects the camera comes near. Two parameter tables drive it (SDK-typed rows, STATIC):
//! - `ASSET_GEOMETORY_PARAM_ST.cam_near_behavior_type` for map assets (trees, bushes, rocks, ruins): 0 never disappears,
//!   1 and 2 fade near the camera (same field the camera-proximity work in the cinematic branch uses);
//! - `CHR_MODEL_PARAM_ST.camera_dither_fade_id` for characters and models (paramdef text of the Elden Ring Debug Tool:
//!   -1 = take it from the material, 0 = never disappears, 1.. = a CAMERA_FADE_PARAM_ST row): every row is set to 0.
//! - `CAMERA_FADE_PARAM_ST` (paramdef text: alpha is 0 at NearMinDist, ramps to MiddleAlpha at NearMaxDist, stays
//!   MiddleAlpha until FarMinDist, ramps to 1 at FarMaxDist): the near range is moved to (-1, 0) and MiddleAlpha is set to
//!   1, so alpha is 1 at every real distance. (Moving only the near range would have left the middle band translucent.)
//! Grass has its own table (`GRASS_TYPE_PARAM_ST.dithering`, an enum whose values are not labelled); it is only
//! counted in the log. Everything is changed in memory only, remembered row by row and put back exactly when the
//! feature is switched off or the repository is replaced; nothing is saved to disk.
use eldenring::cs::{AssetEnvironmentGeometryParam,ChrModelParam,GrassTypeParam,GrassTypeParam_Lv1,GrassTypeParam_Lv2,SoloParamRepository};
use eldenring::param::{CAMERA_FADE_PARAM_ST,ParamDef};
use fromsoftware_shared::FromStatic;
use std::sync::Mutex;

#[derive(Default)]
struct State{root:usize,applied:bool,assets:Vec<(u32,i8)>,fades:Vec<(u32,f32,f32,f32)>,models:Vec<(u32,i16)>,grass:Vec<(u8,u32,u8)>,faulted:bool}
static STATE:Mutex<State>=Mutex::new(State{root:0,applied:false,assets:Vec::new(),fades:Vec::new(),models:Vec::new(),grass:Vec::new(),faulted:false});

fn camera_fade_rows(repo:&mut SoloParamRepository)->Option<&mut eldenring::fd4::FD4ParamResCap>{
 repo.params_mut().find(|p|p.struct_name()==CAMERA_FADE_PARAM_ST::NAME)}

fn restore(state:&mut State,repo:&mut SoloParamRepository){
 let (mut a,mut f)=(0,0);
 if repo as *mut SoloParamRepository as usize==state.root{
  for &(id,v) in &state.assets{if let Some(row)=repo.get_mut::<AssetEnvironmentGeometryParam>(id){if row.cam_near_behavior_type()==0{row.set_cam_near_behavior_type(v);a+=1;}}}
  if let Some(cap)=camera_fade_rows(repo){for &(id,min,max,mid) in &state.fades{
   if let Some(row)=unsafe{cap.get_mut::<CAMERA_FADE_PARAM_ST>(id)}{if row.near_min_dist()==-1.0&&row.near_max_dist()==0.0{row.set_near_min_dist(min);row.set_near_max_dist(max);row.set_middle_alpha(mid);f+=1;}}}}
  let mut m=0;for &(id,v) in &state.models{if let Some(row)=repo.get_mut::<ChrModelParam>(id){if row.camera_dither_fade_id()==0{row.set_camera_dither_fade_id(v);m+=1;}}}
  let mut g=0;for &(table,id,v) in &state.grass{let row=match table{0=>repo.get_mut::<GrassTypeParam>(id),1=>repo.get_mut::<GrassTypeParam_Lv1>(id),_=>repo.get_mut::<GrassTypeParam_Lv2>(id)};if let Some(row)=row{if row.dithering()==0{row.set_dithering(v);g+=1;}}}
  crate::log_game(&format!("CAMERA_FADE: restored {g} of {} grass rows, {a} of {} asset rows, {f} of {} camera-fade rows, {m} of {} model rows",state.grass.len(),state.assets.len(),state.fades.len(),state.models.len()));
 }else if state.applied{crate::log_game("CAMERA_FADE: the parameter repository was replaced; old rows left alone");}
 state.assets.clear();state.fades.clear();state.models.clear();state.grass.clear();state.applied=false;state.root=0;}

/// Call once per game frame; `active` = the feature is wanted right now. Never panics into the game.
pub fn tick(active:bool){
 let mut state=STATE.lock().unwrap_or_else(|e|e.into_inner());
 if state.faulted||(!active&&!state.applied){return;}
 let result=std::panic::catch_unwind(std::panic::AssertUnwindSafe(||{
  let Ok(repo)=(unsafe{SoloParamRepository::instance_mut()}) else {return};
  let root=repo as *mut SoloParamRepository as usize;
  if !active{restore(&mut state,repo);return;}
  if state.applied&&state.root==root{return;}
  if state.applied{restore(&mut state,repo);}
  state.root=root;
  // Every value except 0 ("never disappears") is switched, including -1 and unknown values: the aim is no fade at all.
  let mut near_hist=std::collections::BTreeMap::<i8,u32>::new();
  for (id,row) in repo.rows_mut::<AssetEnvironmentGeometryParam>(){let v=row.cam_near_behavior_type();*near_hist.entry(v).or_default()+=1;if v!=0{state.assets.push((id,v));row.set_cam_near_behavior_type(0);}}
  if let Some(cap)=camera_fade_rows(repo){
   for (id,row) in unsafe{cap.data.rows_mut::<CAMERA_FADE_PARAM_ST>()}{
    state.fades.push((id,row.near_min_dist(),row.near_max_dist(),row.middle_alpha()));row.set_near_min_dist(-1.0);row.set_near_max_dist(0.0);row.set_middle_alpha(1.0);}}
  for (id,row) in repo.rows_mut::<ChrModelParam>(){let v=row.camera_dither_fade_id();if v!=0{state.models.push((id,v));row.set_camera_dither_fade_id(0);}}
  // Grass: the "dithering" byte of the three grass tables (no labels exist for values other than 0); set to 0 and remembered.
  let mut grass=std::collections::BTreeMap::<u8,u32>::new();
  for (id,row) in repo.rows_mut::<GrassTypeParam>(){let v=row.dithering();*grass.entry(v).or_default()+=1;if v!=0{state.grass.push((0,id,v));row.set_dithering(0);}}
  for (id,row) in repo.rows_mut::<GrassTypeParam_Lv1>(){let v=row.dithering();*grass.entry(v).or_default()+=1;if v!=0{state.grass.push((1,id,v));row.set_dithering(0);}}
  for (id,row) in repo.rows_mut::<GrassTypeParam_Lv2>(){let v=row.dithering();*grass.entry(v).or_default()+=1;if v!=0{state.grass.push((2,id,v));row.set_dithering(0);}}
  state.applied=true;
  crate::log_game(&format!("CAMERA_FADE: no near-camera fade; {} asset rows set to 'never disappears', {} camera-fade rows set to opaque, {} model rows set to \"never disappears\"; original asset camera-near values {near_hist:?}; original grass dithering values {grass:?} (all set to 0)",state.assets.len(),state.fades.len(),state.models.len()));
 }));
 if result.is_err(){state.faulted=true;crate::log_game("CAMERA_FADE ERROR: the parameter tables did not validate; feature disabled for this session");}
}

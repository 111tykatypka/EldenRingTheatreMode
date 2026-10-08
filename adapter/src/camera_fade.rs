//! No fade-out when the camera gets close to things (foliage, trees, rocks, characters).
//!
//! The game fades objects the camera comes near. Two parameter tables drive it (SDK-typed rows, STATIC):
//! - `ASSET_GEOMETORY_PARAM_ST.cam_near_behavior_type` for map assets (trees, bushes, rocks, ruins): 0 never disappears,
//!   1 and 2 fade near the camera (same field the camera-proximity work in the cinematic branch uses);
//! - `CAMERA_FADE_PARAM_ST.near_min_dist / near_max_dist` for characters and models that name a camera-fade row through
//!   `CHR_MODEL_PARAM_ST.camera_dither_fade_id`: alpha ramps between those two distances, so the range is moved to
//!   (-1, 0) which is fully opaque for every real distance and cannot divide by zero.
//! Grass has its own table (`GRASS_TYPE_PARAM_ST.dithering`, an enum whose values are not labelled); it is only
//! counted in the log. Everything is changed in memory only, remembered row by row and put back exactly when the
//! feature is switched off or the repository is replaced; nothing is saved to disk.
use eldenring::cs::{AssetEnvironmentGeometryParam,GrassTypeParam,SoloParamRepository};
use eldenring::param::{CAMERA_FADE_PARAM_ST,ParamDef};
use fromsoftware_shared::FromStatic;
use std::sync::Mutex;

#[derive(Default)]
struct State{root:usize,applied:bool,assets:Vec<(u32,i8)>,fades:Vec<(u32,f32,f32)>,faulted:bool}
static STATE:Mutex<State>=Mutex::new(State{root:0,applied:false,assets:Vec::new(),fades:Vec::new(),faulted:false});

fn camera_fade_rows(repo:&mut SoloParamRepository)->Option<&mut eldenring::fd4::FD4ParamResCap>{
 repo.params_mut().find(|p|p.struct_name()==CAMERA_FADE_PARAM_ST::NAME)}

fn restore(state:&mut State,repo:&mut SoloParamRepository){
 let (mut a,mut f)=(0,0);
 if repo as *mut SoloParamRepository as usize==state.root{
  for &(id,v) in &state.assets{if let Some(row)=repo.get_mut::<AssetEnvironmentGeometryParam>(id){if row.cam_near_behavior_type()==0{row.set_cam_near_behavior_type(v);a+=1;}}}
  if let Some(cap)=camera_fade_rows(repo){for &(id,min,max) in &state.fades{
   if let Some(row)=unsafe{cap.get_mut::<CAMERA_FADE_PARAM_ST>(id)}{if row.near_min_dist()==-1.0&&row.near_max_dist()==0.0{row.set_near_min_dist(min);row.set_near_max_dist(max);f+=1;}}}}
  crate::log_game(&format!("CAMERA_FADE: restored {a} of {} asset rows and {f} of {} camera-fade rows",state.assets.len(),state.fades.len()));
 }else if state.applied{crate::log_game("CAMERA_FADE: the parameter repository was replaced; old rows left alone");}
 state.assets.clear();state.fades.clear();state.applied=false;state.root=0;}

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
  for (id,row) in repo.rows_mut::<AssetEnvironmentGeometryParam>(){let v=row.cam_near_behavior_type();if v==1||v==2{state.assets.push((id,v));row.set_cam_near_behavior_type(0);}}
  if let Some(cap)=camera_fade_rows(repo){
   for (id,row) in unsafe{cap.data.rows_mut::<CAMERA_FADE_PARAM_ST>()}{
    state.fades.push((id,row.near_min_dist(),row.near_max_dist()));row.set_near_min_dist(-1.0);row.set_near_max_dist(0.0);}}
  let mut grass=std::collections::BTreeMap::<u8,u32>::new();
  for (_,row) in repo.rows::<GrassTypeParam>(){*grass.entry(row.dithering()).or_default()+=1;}
  state.applied=true;
  crate::log_game(&format!("CAMERA_FADE: no near-camera fade; {} asset rows set to 'never disappears', {} camera-fade rows set to opaque; grass dithering values (not changed): {grass:?}",state.assets.len(),state.fades.len()));
 }));
 if result.is_err(){state.faulted=true;crate::log_game("CAMERA_FADE ERROR: the parameter tables did not validate; feature disabled for this session");}
}

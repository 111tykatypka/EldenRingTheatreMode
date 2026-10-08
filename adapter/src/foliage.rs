//! Temporary camera-proximity asset visibility override. Game callback only.
//! Exact pinned SDK typed rows; no disk/save changes, raw offsets or cached row dereferences.
use eldenring::cs::{SoloParamRepository,AssetEnvironmentGeometryParam,GrassTypeParam};
use fromsoftware_shared::FromStatic;
use std::sync::{Mutex,OnceLock};

unsafe extern "C" {fn tm_camera_asset_fade_requested()->i32;}
#[derive(Default)]
struct State {root:usize,checked:bool,saved:Vec<(u32,usize,i8)>,faulted:bool}
static STATE:OnceLock<Mutex<State>>=OnceLock::new();

fn restore(state:&mut State,repo:&mut SoloParamRepository){
    let root=repo as *mut SoloParamRepository as usize;
    if root==state.root {
        let mut restored=0;
        for &(id,address,original) in &state.saved {
            if let Some(row)=repo.get_mut::<AssetEnvironmentGeometryParam>(id){
                // The numeric address is a generation check only. Write solely
                // through a freshly reacquired public mutable row, if still owned.
                if row as *mut _ as usize==address&&row.cam_near_behavior_type()==0 {
                    row.set_cam_near_behavior_type(original);restored+=1;
                }
            }
        }
        if !state.saved.is_empty(){crate::log_game(&format!("FOLIAGE_OVERRIDE restored={} of={} (unowned changes preserved)",restored,state.saved.len()));}
    }else if !state.saved.is_empty(){crate::log_game("FOLIAGE_OVERRIDE repository generation changed; old rows not dereferenced");}
    state.saved.clear();state.root=0;state.checked=false;
}

pub fn tick(allowed:bool){
    let request=unsafe{tm_camera_asset_fade_requested()};
    if request<0&&allowed{return;} // Never stall the game callback on the camera mutex.
    let mut state=STATE.get_or_init(||Mutex::new(State::default())).lock().unwrap_or_else(|e|e.into_inner());
    let active=allowed&&request==1&&!state.faulted;
    if !active&&state.saved.is_empty(){state.checked=false;return;}
    let result=std::panic::catch_unwind(std::panic::AssertUnwindSafe(||{
        let Ok(repo)=(unsafe{SoloParamRepository::instance_mut()})else{return;};
        let root=repo as *mut SoloParamRepository as usize;
        if !active{restore(&mut state,repo);return;}
        if state.checked&&state.root==root{return;}
        if !state.saved.is_empty(){restore(&mut state,repo);}
        state.root=root;
        // Primary ER enum: 0 never disappears; 1/2 proximity fade. Preserve -1
        // inheritance and unknown values rather than guessing their semantics.
        for (id,row) in repo.rows_mut::<AssetEnvironmentGeometryParam>() {
            let value=row.cam_near_behavior_type();
            if value==1||value==2 {
                state.saved.push((id,row as *mut _ as usize,value));
                row.set_cam_near_behavior_type(0);
            }
        }
        let mut histogram=std::collections::BTreeMap::<u8,usize>::new();
        for (_,row) in repo.rows::<GrassTypeParam>() {*histogram.entry(row.dithering()).or_default()+=1;}
        state.checked=true;
        crate::log_game(&format!("FOLIAGE_OVERRIDE asset_rows={} camera_near_behavior=0; grass_dithering={:?}; grass_override=UNAVAILABLE (enum only labels Type 0)",state.saved.len(),histogram));
    }));
    if result.is_err()&&!state.faulted{state.faulted=true;crate::log_game("FOLIAGE_OVERRIDE ERROR: SDK table validation failed; further application disabled, owned restoration will be attempted");}
}

//! Native PARAM wind-response strength. This is not a global force/direction API.
//! Only the verified game callback mutates freshly reacquired SDK rows.
use eldenring::cs::{SoloParamRepository,GrassTypeParam,AssetEnvironmentGeometryParam as AssetGeometryParam};
use fromsoftware_shared::FromStatic;
use std::sync::{Mutex,OnceLock};
unsafe extern "C" {fn tm_wind_request(strength:*mut f32)->i32;fn tm_wind_report(status:i32,grass:u32,assets:u32);fn tm_wind_inspect_tick(allowed:i32);}
struct Grass {id:u32,address:usize,original:u8,last:u8}
struct Asset {id:u32,address:usize,original:[f32;2],last:[f32;2]}
#[derive(Default)]
struct State {root:usize,grass:Vec<Grass>,assets:Vec<Asset>,strength:Option<f32>,faulted:bool}
static STATE:OnceLock<Mutex<State>>=OnceLock::new();
fn restore(state:&mut State,repo:&mut SoloParamRepository){
    if state.root==repo as *mut _ as usize {
        for s in &state.grass {if let Some(row)=repo.get_mut::<GrassTypeParam>(s.id){
            if row as *mut _ as usize==s.address&&row.wind_amplitude()==s.last{row.set_wind_amplitude(s.original);}
        }}
        for s in &state.assets {if let Some(row)=repo.get_mut::<AssetGeometryParam>(s.id){
            if row as *mut _ as usize==s.address {
                if row.wind_effect_rate_0().to_bits()==s.last[0].to_bits(){row.set_wind_effect_rate_0(s.original[0]);}
                if row.wind_effect_rate_1().to_bits()==s.last[1].to_bits(){row.set_wind_effect_rate_1(s.original[1]);}
            }
        }}
        crate::log_game("WIND_RESPONSE owned row values restored; unrelated edits preserved");
    } else {crate::log_game("WIND_RESPONSE repository replaced; old rows not dereferenced");}
    state.grass.clear();state.assets.clear();state.root=0;state.strength=None;
}
pub fn tick(allowed:bool){
    unsafe{tm_wind_inspect_tick(allowed as i32);}
    let mut strength=1.;let request=unsafe{tm_wind_request(&mut strength)};
    if allowed&&request<0{return;}
    let mut state=STATE.get_or_init(||Mutex::new(State::default())).lock().unwrap_or_else(|e|e.into_inner());
    let active=allowed&&request==1&&strength.is_finite()&&(0.0..=3.0).contains(&strength)&&!state.faulted;
    if !active&&state.root==0{unsafe{tm_wind_report(if state.faulted{-1}else{0},0,0)};return;}
    let result=std::panic::catch_unwind(std::panic::AssertUnwindSafe(||{
        let Ok(repo)=(unsafe{SoloParamRepository::instance_mut()})else{
            // Do not discard originals during a transient missing repository.
            unsafe{tm_wind_report(-2,0,0)};return;
        };
        let root=repo as *mut _ as usize;
        if state.root!=0&&(state.root!=root||!active){restore(&mut state,repo);}
        if !active{unsafe{tm_wind_report(0,0,0)};return;}
        if state.root==0 {
            state.root=root;
            for (id,row) in repo.rows::<GrassTypeParam>(){
                state.grass.push(Grass{id,address:row as *const _ as usize,original:row.wind_amplitude(),last:row.wind_amplitude()});
            }
            for (id,row) in repo.rows::<AssetGeometryParam>(){
                let values=[row.wind_effect_rate_0(),row.wind_effect_rate_1()];
                // Keep assets without a recognized native wind response untouched.
                if (row.wind_effect_type_0()>0&&row.wind_effect_type_0()<=2)||(row.wind_effect_type_1()>0&&row.wind_effect_type_1()<=2){
                    if values.iter().all(|v|v.is_finite()&&*v>=0.&&*v<=9999.){
                        state.assets.push(Asset{id,address:row as *const _ as usize,original:values,last:values});
                    }
                }
            }
        }
        if state.strength==Some(strength){return;}
        let mut grass=0;let mut assets=0;
        for s in &mut state.grass {if let Some(row)=repo.get_mut::<GrassTypeParam>(s.id){
            if row as *mut _ as usize==s.address&&row.wind_amplitude()==s.last{
                s.last=(f32::from(s.original)*strength).round().clamp(0.,255.) as u8;
                row.set_wind_amplitude(s.last);grass+=1;
            }
        }}
        for s in &mut state.assets {if let Some(row)=repo.get_mut::<AssetGeometryParam>(s.id){
            if row as *mut _ as usize==s.address{
                let mut touched=false;
                if (1..=2).contains(&row.wind_effect_type_0())&&row.wind_effect_rate_0().to_bits()==s.last[0].to_bits(){s.last[0]=(s.original[0]*strength).min(9999.);row.set_wind_effect_rate_0(s.last[0]);touched=true;}
                if (1..=2).contains(&row.wind_effect_type_1())&&row.wind_effect_rate_1().to_bits()==s.last[1].to_bits(){s.last[1]=(s.original[1]*strength).min(9999.);row.set_wind_effect_rate_1(s.last[1]);touched=true;}
                if touched{assets+=1;}
            }
        }}
        state.strength=Some(strength);
        unsafe{tm_wind_report(1,grass,assets)};
        crate::log_game(&format!("WIND_RESPONSE multiplier={strength:.3} grass_rows={grass} asset_rows={assets}; direction/cloth UNAVAILABLE; visual validation required"));
    }));
    if result.is_err(){state.faulted=true;unsafe{tm_wind_report(-1,0,0)};crate::log_game("WIND_RESPONSE SDK access failed; writes disabled; restoration pending");}
}

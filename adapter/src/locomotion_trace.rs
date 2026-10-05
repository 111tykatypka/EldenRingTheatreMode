//! Read-only character-driving differential trace. No engine references leave the callback.
use eldenring::cs::PlayerIns;
use std::{fs::OpenOptions, io::Write, sync::{Mutex, atomic::{AtomicBool, AtomicU32, AtomicU64, Ordering}}, time::Duration};

static REQUESTED: AtomicBool = AtomicBool::new(false);
static ACTIVE: AtomicBool = AtomicBool::new(false);
static AVAILABLE: AtomicBool = AtomicBool::new(false);
static FAILED: AtomicBool = AtomicBool::new(false);
static PHASE: AtomicU32 = AtomicU32::new(0);
static EPOCH: AtomicU64 = AtomicU64::new(0);
static SKIPPED: AtomicU64 = AtomicU64::new(0);
static LATEST: Mutex<Option<Snapshot>> = Mutex::new(None);
const LABELS: [&str;7] = ["UNMARKED", "IDLE", "WALK", "RUN", "SPRINT", "ROLL", "JUMP"];
const FLOAT_NAMES: [&str;23] = ["position_x", "position_y", "position_z", "horizontal_speed_derived", "vertical_speed_derived", "root_motion_x", "root_motion_y", "root_motion_z", "root_motion_w", "animation_rate", "hks_animation_rate", "hks_root_motion_multiplier", "turn_speed", "motion_multiplier", "movement_request_duration", "root_motion_reduction", "animation_phase", "animation_length", "rotation_multiplier", "quaternion_x", "quaternion_y", "quaternion_z", "quaternion_w"];
const INT_NAMES: [&str;15] = ["action_requests", "cancel_ready", "disabled_actions", "movement_flags", "debug_flags", "ctrl_flags", "ctrl_hks_flags", "movement_limit", "grounded", "falling", "observed_tae_id", "pending_event_id", "idle_event_id", "replay_recorder_present", "net_sync_flags"];

#[derive(Clone,Copy)]
pub struct Snapshot { ns:u64, epoch:u64, phase:u32, present:bool, floats:[f64;23], ints:[u64;15], recorder:[u32;3], recorder_oldest:[f32;4] }
pub fn command(kind:u16,phase:u32) {
    match kind {
        crate::control_protocol::TRACE_START => { FAILED.store(false,Ordering::Release); PHASE.store(1,Ordering::Release); EPOCH.fetch_add(1,Ordering::AcqRel); REQUESTED.store(true,Ordering::Release); },
        crate::control_protocol::TRACE_STOP => {REQUESTED.store(false,Ordering::Release);},
        crate::control_protocol::TRACE_MARK if phase<=6 => {PHASE.store(phase,Ordering::Release);},
        _=>{}
    }
}
pub fn stop(){ REQUESTED.store(false,Ordering::Release); }
pub fn busy()->bool{REQUESTED.load(Ordering::Acquire)}
pub fn flags()->u32 { (if ACTIVE.load(Ordering::Acquire){4}else{0}) | (if AVAILABLE.load(Ordering::Acquire){8}else{0}) | (if FAILED.load(Ordering::Acquire){16}else{0}) }

#[derive(Default)] pub struct Capture { previous:Option<(u64,[f64;3])> }
impl Capture {
    /// Must run before any immutable WorldChrMan borrow in ChrIns_PostPhysics.
    pub fn tick(&mut self,ns:u64) {
        if !ACTIVE.load(Ordering::Acquire){self.previous=None;return;}
        let epoch=EPOCH.load(Ordering::Acquire);
        let phase=PHASE.load(Ordering::Acquire);
        let mut s=Snapshot{ns,epoch,phase,present:false,floats:[0.;23],ints:[0;15],recorder:[0;3],recorder_oldest:[0.;4]};
        if let Ok(player)=unsafe{PlayerIns::local_player()} {
            s.present=true;
            let m=&player.chr_ins.modules;let p=&m.physics;let b=&m.behavior;let a=&m.action_request;let ctrl=&player.chr_ins.chr_ctrl;let modifier=&ctrl.modifier.data;
            let pos=[f64::from(p.position.0),f64::from(p.position.1),f64::from(p.position.2)];
            s.floats[..3].copy_from_slice(&pos);
            if let Some((before,last))=self.previous { if ns>before && ns-before<250_000_000 {let dt=(ns-before) as f64/1e9;s.floats[3]=((pos[0]-last[0]).powi(2)+(pos[2]-last[2]).powi(2)).sqrt()/dt;s.floats[4]=(pos[1]-last[1])/dt;} }
            self.previous=Some((ns,pos));
            let observed=crate::player_action::observe(player);
            let vals=[b.root_motion.0,b.root_motion.1,b.root_motion.2,b.root_motion.3,b.animation_speed,m.behavior_data.hks_animation_speed_multiplier,m.behavior_data.hks_root_motion_mult,m.behavior_data.turn_speed,p.motion_multiplier,a.movement_request_duration,modifier.root_motion_reduction,observed.animation_time,observed.animation_length,p.rotation_multiplier,p.orientation.0,p.orientation.1,p.orientation.2,p.orientation.3];
            for(i,v)in vals.into_iter().enumerate(){s.floats[5+i]=f64::from(v);}
            s.ints=[a.action_requests.0,a.cancel_ready_actions.0,a.disabled_action_inputs.0,u64::from(a.movement_request_flags.0),u64::from(player.chr_ins.debug_flags.0),u64::from(ctrl.flags.0),u64::from(modifier.hks_flags.0),modifier.movement_limit as u64,p.standing_on_solid_ground as u64,p.is_falling as u64,observed.animation_id as i64 as u64,m.event.request_animation_id as i64 as u64,m.event.idle_anim_id as i64 as u64,player.replay_recorder.is_some() as u64,u64::from(player.chr_ins.net_chr_sync_flags.0)];
            if let Some(r)=player.replay_recorder.as_ref(){s.recorder=[r.max_frame_rate,r.frame_counter,r.frame_duration];s.recorder_oldest=[r.position.0,r.position.1,r.position.2,r.rotation];}
        } else {self.previous=None;}
        // Never block the game on a logger lock or filesystem operation.
        if let Ok(mut slot)=LATEST.try_lock(){*slot=Some(s);}else{SKIPPED.fetch_add(1,Ordering::Relaxed);}
    }
}

fn changed(a:f64,b:f64,index:usize)->bool {
    if !a.is_finite() || !b.is_finite(){return a.to_bits()!=b.to_bits();}
    let threshold=match index {0..=4=>0.05,5..=8=>0.005,14|16=>0.5,_=>0.01};
    (a-b).abs()>=threshold
}
fn diff(old:Option<Snapshot>,s:Snapshot)->String {
    let mut out=String::new();
    for(i,name)in FLOAT_NAMES.into_iter().enumerate(){if old.is_none_or(|o|changed(o.floats[i],s.floats[i],i)){out.push_str(&format!("  {name}: {} -> {:.6}{}\n",old.map_or("BASELINE".into(),|o|format!("{:.6}",o.floats[i])),s.floats[i],if s.floats[i].is_finite(){""}else{" INVALID_NONFINITE"}));}}
    for(i,name)in INT_NAMES.into_iter().enumerate(){if old.is_none_or(|o|o.ints[i]!=s.ints[i]){out.push_str(&format!("  {name}: {} -> {} (0x{:X})\n",old.map_or("BASELINE".into(),|o|format!("{}",o.ints[i] as i64)),s.ints[i] as i64,s.ints[i]));}}
    out
}
fn write(f:&mut std::fs::File,text:&str)->bool {
    match f.write_all(text.as_bytes()).and_then(|_|f.flush()) {Ok(())=>true,Err(e)=>{FAILED.store(true,Ordering::Release);REQUESTED.store(false,Ordering::Release);ACTIVE.store(false,Ordering::Release);crate::log_game(&format!("LOCOMOTION_TRACE_WRITE_ERROR={e}; trace OFF"));false}}
}

/// Dedicated worker: receives copied latest values only; never accesses game objects.
pub fn worker() {
    AVAILABLE.store(true,Ordering::Release);
    let path=std::env::temp_dir().join("TheaterModeLocomotionTrace.log");
    let mut file:Option<std::fs::File>=None;let mut epoch=0;let mut old:Option<Snapshot>=None;let mut phase=0;let mut last_ns=0;let mut polls=0u64;
    loop {
        let requested=REQUESTED.load(Ordering::Acquire);let wanted_epoch=EPOCH.load(Ordering::Acquire);
        if requested && (file.is_none()||epoch!=wanted_epoch) {
            if let Some(mut f)=file.take(){write(&mut f,"TRACE_END replaced_session
");}
            match OpenOptions::new().create(true).append(true).open(&path) {
                Ok(mut f)=>{epoch=wanted_epoch;old=None;phase=0;last_ns=0;polls=0;let _=writeln!(f,"\nTRACE_BEGIN epoch={epoch} utc_unix_ms={} profile=EldenRing_1_17 pinned=3c8c1d7 sample=ChrIns_PostPhysics diff_max_hz=10; labels=USER_MARKED; speed=DERIVED not graph input\nUNAVAILABLE: analog_magnitude, MoveSpeedLevel, MoveSpeedIndex, HKS_state, behavior_node, native_replay_frame_payload; NO GAME WRITES",std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap_or_default().as_millis());file=Some(f);ACTIVE.store(true,Ordering::Release);},
                Err(e)=>{FAILED.store(true,Ordering::Release);REQUESTED.store(false,Ordering::Release);crate::log_game(&format!("LOCOMOTION_TRACE_OPEN_ERROR={e}; trace OFF"));}
            }
        }
        if !requested {ACTIVE.store(false,Ordering::Release);if let Some(mut f)=file.take(){write(&mut f,&format!("TRACE_END polls={polls} callback_lock_skips={} latest_value_coalescing=YES\n",SKIPPED.swap(0,Ordering::AcqRel)));}old=None;}
        if let Some(f)=file.as_mut() {
            let s=LATEST.lock().ok().and_then(|s|*s).filter(|s|s.epoch==epoch);
            if let Some(s)=s {if s.ns!=last_ns {
                polls+=1;
                if last_ns!=0 && (s.ns<last_ns||s.ns-last_ns>250_000_000){write(f,&format!("UPDATE_GAP previous_ns={last_ns} current_ns={}\n",s.ns));}
                let transition=old.is_none_or(|o|o.phase!=s.phase||o.present!=s.present);
                let text=if s.present{diff(if transition{None}else{old},s)}else{String::new()};
                if transition||!text.is_empty(){write(f,&format!("STATE ns={} marker={} previous={} player={} recorder(max_hz,counter,duration)={:?} native_oldest(x,y,z,rotation_scalar)={:?}\n{}",s.ns,LABELS[s.phase as usize],LABELS[phase as usize],if s.present{"FOUND"}else{"LOST"},s.recorder,s.recorder_oldest,text));}
                // Retain previous emitted baseline, so small accumulated changes remain observable.
                if transition||!text.is_empty(){old=Some(s);}phase=s.phase;last_ns=s.ns;
            }}
        }
        std::thread::sleep(Duration::from_millis(100));
    }
}

#[cfg(test)]mod tests {use super::*;
    fn sample()->Snapshot{Snapshot{ns:1,epoch:1,phase:1,present:true,floats:[0.;23],ints:[0;15],recorder:[0;3],recorder_oldest:[0.;4]}}
    #[test]fn structured_diff_filters_noise_and_reports_invalid(){let a=sample();let mut b=a;b.floats[3]=0.001;assert!(diff(Some(a),b).is_empty());b.floats[3]=1.63;b.ints[3]=1;let d=diff(Some(a),b);assert!(d.contains("horizontal_speed_derived")&&d.contains("movement_flags")&&!d.contains("observed_tae_id"));b.floats[9]=f64::NAN;assert!(diff(Some(a),b).contains("INVALID_NONFINITE"));}
}

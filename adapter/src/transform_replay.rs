//! Transform authority only. Replay time/interpolation are supplied by the host.
use crate::transform_probe::{distance, Transform};

pub const INACTIVE: u32 = 0;
pub const PLAYING: u32 = 1;
pub const PAUSED: u32 = 2;
pub const FINISHED: u32 = 3;
pub const ERROR: u32 = 4;
pub const TARGET_LEASE_NS: u64 = 250_000_000;
pub const MAX_START_DISTANCE: f64 = 15.0;
pub const MAX_TARGET_STEP: f64 = 5.0;
pub const ERR_SESSION: u32 = 9;
pub const ERR_START_DISTANCE: u32 = 10;
pub const ERR_TIME: u32 = 11;
pub const ERR_TARGET_STALE: u32 = 12;
pub const ERR_TARGET_STEP: u32 = 13;
pub const INTERPOLATION_DELAY_NS:u64=8_333_333;
pub const MAX_INTERPOLATION_DELAY_NS:u64=33_333_333;

#[derive(Clone, Copy, Debug)]
pub enum Action { Begin, Apply, Finish }
#[derive(Clone, Copy, Debug)]
pub struct Request {
    pub player_action:crate::player_action::State,pub animation_enabled:bool,
    pub action: Action, pub session: u64, pub sequence: u64, pub generation: u64,
    pub received_ns: u64, pub replay_ns: u64, pub paused: bool, pub target: Transform,
}
#[derive(Default)]
pub struct Playback {
    pub phase: u32, pub detail: u32, pub session: u64,
    target: Option<Request>, previous:Option<Request>, finish_pending: bool,
}
impl Playback {
    pub fn cancel(&mut self) { *self = Self::default(); }
    fn fail(&mut self, detail: u32) -> Result<(), u32> {
        self.phase = ERROR; self.detail = detail; self.target = None; self.finish_pending = false;
        Err(detail)
    }
    pub fn ingest(&mut self, r: Request, live: Transform) -> Result<(), u32> {
        if !r.target.valid() || !live.valid() { return self.fail(4); }
        if r.session == 0 { return self.fail(ERR_SESSION); }
        match r.action {
            Action::Begin => {
                if matches!(self.phase, PLAYING | PAUSED) || r.replay_ns != 0 { return self.fail(ERR_SESSION); }
                if distance(live, r.target) > MAX_START_DISTANCE { return self.fail(ERR_START_DISTANCE); }
                self.session = r.session;
            }
            Action::Apply | Action::Finish => {
                let Some(previous) = self.target else { return self.fail(ERR_SESSION); };
                if !matches!(self.phase, PLAYING | PAUSED) || self.session != r.session || previous.generation != r.generation {
                    return self.fail(ERR_SESSION);
                }
                if r.sequence <= previous.sequence || r.replay_ns < previous.replay_ns || r.received_ns<previous.received_ns { return self.fail(ERR_TIME); }
                if distance(previous.target, r.target) > MAX_TARGET_STEP { return self.fail(ERR_TARGET_STEP); }
            }
        }
        self.previous=if matches!(r.action,Action::Apply)&&!r.paused&&self.phase==PLAYING {self.target} else {None};
        self.phase = if r.paused { PAUSED } else { PLAYING };
        self.detail = 0; self.finish_pending = matches!(r.action, Action::Finish); self.target = Some(r);
        Ok(())
    }
    pub fn frame(&mut self, generation: u64, now_ns: u64) -> Option<Request> {
        if !matches!(self.phase, PLAYING | PAUSED) { return None; }
        let mut target = self.target?;
        if target.generation != generation { self.cancel(); return None; }
        if now_ns < target.received_ns || now_ns - target.received_ns > TARGET_LEASE_NS {
            let _ = self.fail(ERR_TARGET_STALE); return None;
        }
        if self.finish_pending { self.phase = FINISHED; self.target = None; self.finish_pending = false; }
        else if self.phase==PLAYING {if let Some(previous)=self.previous {
            if target.received_ns>previous.received_ns {let delay=(target.received_ns-previous.received_ns).clamp(INTERPOLATION_DELAY_NS,MAX_INTERPOLATION_DELAY_NS);let view_time=now_ns.saturating_sub(delay);let t=view_time.saturating_sub(previous.received_ns) as f64/(target.received_ns-previous.received_ns) as f64;
                target.target=previous.target.interpolate(target.target,t);if t<1.0 {target.player_action=previous.player_action;}
                target.replay_ns=previous.replay_ns+((target.replay_ns-previous.replay_ns) as f64*t.clamp(0.0,1.0)) as u64;
            }
        }}
        Some(target)
    }
}

#[cfg(test)] mod tests {
    use super::*;
    fn live() -> Transform { Transform { position: [1.0,2.0,3.0], quaternion: [0.0,0.0,0.0,1.0] } }
    fn begin() -> Request { Request {player_action:Default::default(),animation_enabled:false, action: Action::Begin, session: 4, sequence: 1, generation: 2,
        received_ns: 100, replay_ns: 0, paused: false, target: live() } }
    #[test] fn inactive_start_pause_resume_finish_stop() {
        let mut p = Playback::default(); assert!(p.frame(2,100).is_none());
        p.ingest(begin(),live()).unwrap(); assert_eq!(p.frame(2,100).unwrap().replay_ns,0);
        p.ingest(Request { action: Action::Apply,sequence:2,replay_ns:10,paused:true,..begin() },live()).unwrap();
        assert_eq!(p.phase,PAUSED); assert!(p.frame(2,101).is_some());
        p.ingest(Request { action: Action::Apply,sequence:3,replay_ns:20,..begin() },live()).unwrap();
        assert_eq!(p.phase,PLAYING);
        p.ingest(Request { action: Action::Finish,sequence:4,replay_ns:30,..begin() },live()).unwrap();
        assert!(p.frame(2,101).is_some()); assert_eq!(p.phase,FINISHED); assert!(p.frame(2,102).is_none());
        p.cancel(); assert_eq!(p.phase,INACTIVE); assert!(p.frame(2,103).is_none());
    }
    #[test] fn guards_distance_session_time_and_large_step() {
        let mut p = Playback::default();
        assert_eq!(p.ingest(Request { target:Transform { position:[100.0,2.0,3.0],..live() },..begin() },live()),Err(ERR_START_DISTANCE));
        p.ingest(begin(),live()).unwrap();
        assert_eq!(p.ingest(Request { action:Action::Apply,sequence:2,session:9,..begin() },live()),Err(ERR_SESSION));
        p.ingest(begin(),live()).unwrap();
        assert_eq!(p.ingest(Request { action:Action::Apply,..begin() },live()),Err(ERR_TIME));
        p.ingest(begin(),live()).unwrap();
        assert_eq!(p.ingest(Request { action:Action::Apply,sequence:2,target:Transform { position:[9.0,2.0,3.0],..live() },..begin() },live()),Err(ERR_TARGET_STEP));
        assert!(p.frame(2,101).is_none());
    }
    #[test] fn stale_ui_and_generation_fail_open_even_with_heartbeats() {
        let mut p = Playback::default(); p.ingest(begin(),live()).unwrap();
        assert!(p.frame(2,100+TARGET_LEASE_NS).is_some());
        assert!(p.frame(2,101+TARGET_LEASE_NS).is_none()); assert_eq!(p.detail,ERR_TARGET_STALE);
        p.ingest(begin(),live()).unwrap(); assert!(p.frame(3,101).is_none()); assert_eq!(p.phase,INACTIVE);
        assert!(p.ingest(Request { target:Transform { quaternion:[0.0;4],..live() },..begin() },live()).is_err());
    }
    #[test] fn bounded_interpolation_and_sign_equivalent_rotations(){
        let mut p=Playback::default();p.ingest(begin(),live()).unwrap();
        let next=Request {action:Action::Apply,sequence:2,received_ns:100+2*INTERPOLATION_DELAY_NS,replay_ns:20_000_000,target:Transform {position:[3.0,2.0,3.0],quaternion:[0.0,0.0,0.0,-1.0]},..begin()};p.ingest(next,live()).unwrap();
        let middle=p.frame(2,next.received_ns+INTERPOLATION_DELAY_NS).unwrap();assert!((middle.target.position[0]-2.0).abs()<0.0001);assert!(middle.target.valid());assert!(middle.target.quaternion[3].abs()>0.999);
        let held=p.frame(2,next.received_ns+100_000_000).unwrap();assert_eq!(held.target.position,next.target.position); // No extrapolation.
        for degrees in [0.0_f64,90.0,180.0,270.0,360.0] {let rad=degrees.to_radians()/2.0;let q=Transform {quaternion:[0.0,rad.sin() as f32,0.0,rad.cos() as f32],..live()};assert!(live().interpolate(q,0.5).valid());}
    }
}

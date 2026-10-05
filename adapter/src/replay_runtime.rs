use crate::{control_protocol as wire, transform_probe::{self, Transform}, transform_replay::{self as replay, Action, Playback, Request}};
use eldenring::{cs::PlayerIns, rotation::Quaternion};
use std::sync::{Mutex, atomic::{AtomicBool, AtomicU32, AtomicU64, Ordering}};

static CONNECTED: AtomicBool = AtomicBool::new(false);
static HEARTBEAT_NS: AtomicU64 = AtomicU64::new(0);
static GENERATION: AtomicU64 = AtomicU64::new(0);
static PHASE: AtomicU32 = AtomicU32::new(replay::INACTIVE);
static DETAIL: AtomicU32 = AtomicU32::new(0);
static SESSION: AtomicU64 = AtomicU64::new(0);
static REPLAY_NS: AtomicU64 = AtomicU64::new(0);
static APPLIED_SEQUENCE: AtomicU64 = AtomicU64::new(0);
static APPLIED_GENERATION: AtomicU64 = AtomicU64::new(0);
static RECEIVE_COUNT:AtomicU64=AtomicU64::new(0);
static PENDING: AtomicBool = AtomicBool::new(false);
static REQUEST: Mutex<Option<Request>> = Mutex::new(None);

pub fn stop(detail: u32) {
    GENERATION.fetch_add(1, Ordering::AcqRel);
    let old = PHASE.swap(if detail==0 {replay::INACTIVE} else {replay::ERROR}, Ordering::AcqRel);
    DETAIL.store(detail, Ordering::Release); SESSION.store(0, Ordering::Release); PENDING.store(false, Ordering::Release);
    REPLAY_NS.store(0,Ordering::Release); APPLIED_SEQUENCE.store(0,Ordering::Release);
    if matches!(old,replay::PLAYING|replay::PAUSED) || detail!=0 {
        crate::log_game(&format!("REPLAY_STOP detail={detail}; all subsequent transform writes disabled"));
    }
}
pub fn connection(connected: bool) { CONNECTED.store(connected,Ordering::Release); stop(0); }
pub fn heartbeat(now_ns: u64) { HEARTBEAT_NS.store(now_ns,Ordering::Release); }
pub fn active() -> bool { PENDING.load(Ordering::Acquire) || matches!(status().0,replay::PLAYING|replay::PAUSED) }
pub fn status() -> (u32,u32,u64,u64,u64) {
    // An in-flight callback may finish publishing just after STOP. Its old
    // generation must never look like a new active session/acknowledgement.
    if APPLIED_GENERATION.load(Ordering::Acquire)!=GENERATION.load(Ordering::Acquire) {
        let detail=DETAIL.load(Ordering::Acquire);
        return (if detail==0 {replay::INACTIVE} else {replay::ERROR},detail,0,0,0);
    }
    (PHASE.load(Ordering::Acquire),DETAIL.load(Ordering::Acquire),SESSION.load(Ordering::Acquire),
     REPLAY_NS.load(Ordering::Acquire),APPLIED_SEQUENCE.load(Ordering::Acquire))
}
pub fn receive(packet: wire::Packet, _now_ns: u64) {
    RECEIVE_COUNT.fetch_add(1,Ordering::Relaxed);
    let action = match packet.kind { wire::REPLAY_BEGIN=>Action::Begin,wire::REPLAY_APPLY=>Action::Apply,wire::REPLAY_FINISH=>Action::Finish,_=>return };
    if matches!(action,Action::Begin) {
        if active() { stop(replay::ERR_SESSION); return; }
        GENERATION.fetch_add(1,Ordering::AcqRel);
        PHASE.store(replay::INACTIVE,Ordering::Release); SESSION.store(0,Ordering::Release);
        APPLIED_SEQUENCE.store(0,Ordering::Release); DETAIL.store(0,Ordering::Release);
    } else if SESSION.load(Ordering::Acquire)!=packet.session || !active() { stop(replay::ERR_SESSION); return; }
    let request = Request {player_action:packet.player_action,animation_enabled:packet.flags&1!=0,action,session:packet.session,sequence:packet.sequence,generation:GENERATION.load(Ordering::Acquire),
        received_ns:packet.timestamp_ns,replay_ns:packet.replay_timestamp_ns,paused:packet.replay_state==replay::PAUSED,
        target:Transform {position:packet.position,quaternion:packet.quaternion}};
    if let Ok(mut slot)=REQUEST.lock() { *slot=Some(request); PENDING.store(true,Ordering::Release); }
    else { stop(6); }
}

#[derive(Default)]
pub struct GameReplay {
    playback: Playback, generation: u64, log_ns: u64, writes: u64, input:crate::local_input::LocalInputLock,animation:crate::player_action::AnimationLease, last_applied:Option<Transform>, perf_ns:u64, callbacks:u64, perf_writes:u64, perf_received:u64,
}
impl GameReplay {
    /// The IPC worker only copies requests. All mutable bindings are used here,
    /// on ChrIns_PostPhysics, before the sampler borrows WorldChrMan immutably.
    pub fn tick(&mut self, now_ns: u64) {
        self.callbacks+=1;
        if self.perf_ns==0{self.perf_ns=now_ns;self.perf_received=RECEIVE_COUNT.load(Ordering::Relaxed);}
        if now_ns.saturating_sub(self.perf_ns)>=1_000_000_000 {let elapsed=(now_ns-self.perf_ns) as f64/1e9;let received=RECEIVE_COUNT.load(Ordering::Relaxed);
            if self.writes>self.perf_writes||active(){crate::log_game(&format!("REPLAY_PERF ipc_receive_hz={:.2} game_callback_hz={:.2} apply_hz={:.2} interpolation_delay_ms=8.333..33.333 (one target interval, bounded)",(received-self.perf_received) as f64/elapsed,self.callbacks as f64/elapsed,(self.writes-self.perf_writes) as f64/elapsed));}
            self.callbacks=0;self.perf_ns=now_ns;self.perf_writes=self.writes;self.perf_received=received;
        }
        let generation=GENERATION.load(Ordering::Acquire);
        if generation!=self.generation { self.input.restore();self.animation.restore();self.playback.cancel(); self.generation=generation;self.last_applied=None; }
        let request=match REQUEST.try_lock() {Ok(mut slot)=>slot.take(),Err(_)=>None}
            .filter(|r| r.generation==generation);
        if request.is_none() && !matches!(self.playback.phase,replay::PLAYING|replay::PAUSED) {self.input.restore();self.animation.restore();return;}
        if !transform_probe::lease_valid(CONNECTED.load(Ordering::Acquire),HEARTBEAT_NS.load(Ordering::Acquire),now_ns) {
            stop(2); self.playback.cancel();self.input.restore();self.animation.restore(); return;
        }
        if crate::PROFILE.load(Ordering::Acquire)!=crate::STATE_WAITING || crate::INIT_STATE.load(Ordering::Acquire)!=crate::STATE_READY {
            stop(5); self.playback.cancel();self.input.restore();self.animation.restore(); crate::log_game("REPLAY_ERROR=RUNTIME_NOT_READY"); return;
        }
        // Reacquire on EVERY callback. Do not store an engine pointer/reference.
        let player=match unsafe{PlayerIns::local_player_mut()} {
            Ok(player)=>player,Err(_)=>{stop(3);self.playback.cancel();self.input.restore();self.animation.restore();crate::log_game("REPLAY_PLAYER_LOST; no automatic rearm");return;}
        };
        if !self.input.same_owner(player){stop(3);self.playback.cancel();self.input.discard_lost_owner();self.animation.discard();crate::log_game("REPLAY_PLAYER_CHANGED; writes OFF");return;}
        let physics=&mut player.chr_ins.modules.physics;
        let live=Transform {position:[physics.position.0,physics.position.1,physics.position.2],
            quaternion:[physics.orientation.0,physics.orientation.1,physics.orientation.2,physics.orientation.3]};
        if !live.valid() {stop(4);self.playback.cancel();self.input.restore_player(player);self.animation.restore_player(player);crate::log_game("REPLAY_ERROR=INVALID_LIVE_TRANSFORM");return;}
        let previous=self.last_applied;
        if let Some(r)=request {
            if matches!(r.action,Action::Begin) {
                crate::log_game(&format!("REPLAY_START session={} live={:?} first={:?} delta={:?} distance={:.6} policy=warning-only map=UNKNOWN",
                    r.session,live,r.target,std::array::from_fn::<_,3,_>(|i|r.target.position[i]-live.position[i]),
                    transform_probe::distance(live,r.target)));
            }
            if let Err(detail)=self.playback.ingest(r,live) {stop(detail);self.input.restore_player(player);self.animation.restore_player(player);crate::log_game(&format!("REPLAY_ERROR=TARGET_REJECTED detail={detail}"));return;}
            PENDING.store(false,Ordering::Release);
        }
        let Some(r)=self.playback.frame(generation,now_ns) else {
            if self.playback.phase==replay::ERROR {stop(self.playback.detail);self.input.restore_player(player);self.animation.restore_player(player);crate::log_game("REPLAY_ERROR=STALE_TARGET; normal gameplay restored");}
            return;
        };
        // STOP may arrive after a request was read; recheck just before the write.
        if generation!=GENERATION.load(Ordering::Acquire) || !CONNECTED.load(Ordering::Acquire) {return;}
        self.input.apply(player);
        self.animation.apply(player,r.player_action,r.animation_enabled);
        let physics=&mut player.chr_ins.modules.physics;
        physics.position.0=r.target.position[0];physics.position.1=r.target.position[1];physics.position.2=r.target.position[2];
        physics.orientation=Quaternion(r.target.quaternion[0],r.target.quaternion[1],r.target.quaternion[2],r.target.quaternion[3]);
        self.writes+=1;self.last_applied=Some(r.target);
        // Test A: no guessed offsets, no velocity/gravity/input/HKS changes,
        // no proxy sync flags until visual runtime evidence establishes a need.
        if matches!(r.action,Action::Begin) || now_ns.saturating_sub(self.log_ns)>=1_000_000_000 {
            crate::log_game(&format!("REPLAY_APPLY session={} sequence={} replay_ns={} requested={:?} actual_before={:?} previous_target_error={:?} writes={} mode={}; position/orientation ONLY",
                r.session,r.sequence,r.replay_ns,r.target,live,previous.map(|p|transform_probe::distance(p,live)),self.writes,self.playback.phase));
            self.log_ns=now_ns;
        }
        if generation==GENERATION.load(Ordering::Acquire) {
            let old=PHASE.swap(self.playback.phase,Ordering::AcqRel);
            SESSION.store(r.session,Ordering::Release);REPLAY_NS.store(r.replay_ns,Ordering::Release);
            APPLIED_SEQUENCE.store(r.sequence,Ordering::Release);APPLIED_GENERATION.store(generation,Ordering::Release);
            if old!=self.playback.phase {crate::log_game(&format!("REPLAY_STATE={} session={} replay_ns={}",self.playback.phase,r.session,r.replay_ns));}
            if self.playback.phase==replay::FINISHED {self.input.restore_player(player);self.animation.restore_player(player);crate::log_game("REPLAY_FINISHED; final transform applied once; writes OFF");}
        }
    }
    pub fn fail(&mut self) {self.input.restore();self.animation.restore();self.playback.cancel();stop(6);crate::log_game("REPLAY_ERROR=CALLBACK_PANIC");}
}

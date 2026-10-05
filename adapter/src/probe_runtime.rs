use crate::{control_protocol as wire, transform_probe::{self, Observation, Transform}};
use eldenring::{cs::PlayerIns, rotation::Quaternion};
use std::{ffi::c_void, sync::{Mutex, atomic::{AtomicBool, AtomicU32, AtomicU64, Ordering}}};

pub const OFF:u32=0; pub const ARMED:u32=1; pub const OBSERVING:u32=2;
pub const COMPLETE:u32=3; pub const ERROR:u32=4;
static CONNECTED:AtomicBool=AtomicBool::new(false);
static HEARTBEAT_NS:AtomicU64=AtomicU64::new(0);
static GENERATION:AtomicU64=AtomicU64::new(0);
static STATE:AtomicU32=AtomicU32::new(OFF);
static DETAIL:AtomicU32=AtomicU32::new(0);
static COMMAND_SEQUENCE:AtomicU64=AtomicU64::new(0);
static PENDING:Mutex<Option<Request>>=Mutex::new(None);
#[derive(Clone,Copy)] struct Request { sequence:u64, generation:u64, received_ns:u64, delta:[f32;3] }

fn stop(detail:u32) {
    GENERATION.fetch_add(1,Ordering::AcqRel);
    STATE.store(if detail==0 {OFF} else {ERROR},Ordering::Release);
    DETAIL.store(detail,Ordering::Release);
    // Generation invalidation also cancels a pending request without locking.
}
fn ready()->bool { crate::PROFILE.load(Ordering::Acquire)==crate::STATE_WAITING &&
    crate::INIT_STATE.load(Ordering::Acquire)==crate::STATE_READY && crate::PRESENT.load(Ordering::Acquire)!=0 }

#[derive(Default)] pub struct GameProbe { observation:Option<Observation> }
impl GameProbe {
    /// Called only by the verified game task, before the immutable sampler borrow.
    pub fn tick(&mut self,now_ns:u64) {
        let generation=GENERATION.load(Ordering::Acquire);
        let valid_lease=transform_probe::lease_valid(CONNECTED.load(Ordering::Acquire),HEARTBEAT_NS.load(Ordering::Acquire),now_ns);
        if let Some(o)=self.observation {
            if !valid_lease || o.generation!=generation || crate::PROFILE.load(Ordering::Acquire)!=crate::STATE_WAITING {
                self.observation=None;
                if o.generation==generation {stop(2);}
                crate::log_game("PROBE_STATE=OFF reason=STOP_OR_CONNECTION_LOST; no further writes");
            }
        }
        // Never wait on the IPC worker from a game callback.
        let request=match PENDING.try_lock() { Ok(mut pending)=>pending.take(), Err(_)=>None }
            .filter(|r| r.generation==GENERATION.load(Ordering::Acquire));
        if self.observation.is_none() && request.is_none(){return;}
        if !valid_lease {stop(2);self.observation=None;return;}
        if request.is_some_and(|r|now_ns<r.received_ns||now_ns-r.received_ns>transform_probe::LEASE_NS){stop(8);return;}
        // The binding internally reacquires WorldChrMan::instance_mut/main_player.
        // No PlayerIns/WorldChrMan reference survives this invocation.
        let player=match unsafe {PlayerIns::local_player_mut()} {
            Ok(player)=>player,
            Err(_)=>{stop(3);self.observation=None;crate::log_game("PROBE_PLAYER_LOST; PROBE_STATE=OFF");return;}
        };
        let physics=&mut player.chr_ins.modules.physics;
        let actual=Transform { position:[physics.position.0,physics.position.1,physics.position.2],
            quaternion:[physics.orientation.0,physics.orientation.1,physics.orientation.2,physics.orientation.3] };
        if !actual.valid(){stop(4);self.observation=None;crate::log_game("PROBE_ERROR=INVALID_LIVE_TRANSFORM; no write");return;}
        if let Some(mut o)=self.observation {
            o.callbacks+=1;
            if o.callbacks==1 {
                let dot=o.requested.quaternion.iter().zip(actual.quaternion).map(|(a,b)|f64::from(*a)*f64::from(b)).sum::<f64>().abs().clamp(0.0,1.0);
                crate::log_game(&format!("PROBE_NEXT_CALLBACK sequence={} requested={:?} observed={:?} position_error={:.6} delta_from_original={:.6} rotation_error_deg={:.6}; visual movement/collision require user confirmation",o.sequence,o.requested,actual,transform_probe::distance(o.requested,actual),transform_probe::distance(o.original,actual),2.0*dot.acos().to_degrees()));
            }
            if !o.active(generation,now_ns) {
                self.observation=None;let _=STATE.compare_exchange(OBSERVING,COMPLETE,Ordering::AcqRel,Ordering::Acquire);
                crate::log_game(&format!("PROBE_STATE=OFF reason=OBSERVATION_COMPLETE sequence={} callbacks={} final={:?}; total_transform_writes=1; original is NOT automatically restored",o.sequence,o.callbacks,actual));
            } else { self.observation=Some(o); }
        }
        if let Some(r)=request {
            if self.observation.is_some() || r.generation!=GENERATION.load(Ordering::Acquire) || now_ns<r.received_ns || now_ns-r.received_ns>transform_probe::LEASE_NS {return;}
            let Some(requested)=actual.offset(r.delta) else {stop(4);crate::log_game("PROBE_ERROR=INVALID_TARGET; no write");return;};
            if !ready(){stop(5);crate::log_game("PROBE_ERROR=RUNTIME_NOT_READY; no write");return;}
            // Test A intentionally writes ONLY these documented transform fields.
            // No ChrCtrl proxy flags, Euler fields, input, or collision are changed.
            if r.generation!=GENERATION.load(Ordering::Acquire) || !CONNECTED.load(Ordering::Acquire){return;}
            physics.position.0=requested.position[0];physics.position.1=requested.position[1];physics.position.2=requested.position[2];
            physics.orientation=Quaternion(requested.quaternion[0],requested.quaternion[1],requested.quaternion[2],requested.quaternion[3]);
            let immediate=Transform { position:[physics.position.0,physics.position.1,physics.position.2],
                quaternion:[physics.orientation.0,physics.orientation.1,physics.orientation.2,physics.orientation.3] };
            crate::log_game(&format!("PROBE_WRITE_A sequence={} original={:?} requested={:?} immediately_after={:?} displacement={:.6}; position/orientation ONLY",r.sequence,actual,requested,immediate,transform_probe::distance(actual,requested)));
            self.observation=Some(Observation {original:actual,requested,began_ns:now_ns,generation:r.generation,sequence:r.sequence,callbacks:0});
            if STATE.compare_exchange(ARMED,OBSERVING,Ordering::AcqRel,Ordering::Acquire).is_err(){self.observation=None;crate::log_game("PROBE_STOP raced with single write; PROBE_STATE=OFF; no repeated writes");return;}
            crate::log_game("PROBE_STATE=OBSERVING; writes finished; auto-OFF after 2 seconds");
        }
    }
    pub fn fail(&mut self){self.observation=None;stop(6);crate::log_game("PROBE_ERROR=CALLBACK_PANIC; PROBE_STATE=OFF");}
}

#[link(name="kernel32")] unsafe extern "system" {
    fn CreateNamedPipeW(name:*const u16,open:u32,mode:u32,max:u32,out:u32,input:u32,timeout:u32,security:*mut c_void)->*mut c_void;
    fn ConnectNamedPipe(handle:*mut c_void,overlapped:*mut c_void)->i32;
    fn ReadFile(handle:*mut c_void,buffer:*mut c_void,size:u32,read:*mut u32,overlapped:*mut c_void)->i32;
    fn WriteFile(handle:*mut c_void,buffer:*const c_void,size:u32,written:*mut u32,overlapped:*mut c_void)->i32;
    fn DisconnectNamedPipe(handle:*mut c_void)->i32;fn CloseHandle(handle:*mut c_void)->i32;fn GetLastError()->u32;
}
fn read_packet(handle:*mut c_void)->Option<[u8;wire::BYTES]> {
    let mut b=[0u8;wire::BYTES];let mut offset=0;
    while offset<b.len(){let mut count=0;let ok=unsafe{ReadFile(handle,b[offset..].as_mut_ptr().cast(),(b.len()-offset) as u32,&mut count,std::ptr::null_mut())};if ok==0||count==0{return None;}offset+=count as usize;}
    Some(b)
}
fn status()->wire::Packet {
    let sample=crate::latest().unwrap_or_default();
    wire::Packet {kind:wire::STATUS,sequence:COMMAND_SEQUENCE.load(Ordering::Acquire),timestamp_ns:sample.timestamp_ns,
        position:sample.position,quaternion:sample.quaternion_xyzw,state:STATE.load(Ordering::Acquire),detail:DETAIL.load(Ordering::Acquire),
        flags:if ready(){1}else{0} }
}
pub fn pipe_worker() {
    use std::os::windows::ffi::OsStrExt;
    crate::log_game("PHASE4A=TRANSFORM_WRITE_PROBE; PROBE_STATE=OFF; no REPLAY_APPLY implemented in this checkpoint");
    let name=std::ffi::OsStr::new(wire::PIPE).encode_wide().chain(Some(0)).collect::<Vec<_>>();
    loop {
        // Duplex local-only control pipe; retain the original sample pipe unchanged.
        let h=unsafe{CreateNamedPipeW(name.as_ptr(),3|0x0008_0000,0x8,1,wire::BYTES as u32,wire::BYTES as u32,0,std::ptr::null_mut())};
        if h==(-1isize as *mut c_void){crate::log_game(&format!("CONTROL_PIPE_CREATE_ERROR={}",unsafe{GetLastError()}));stop(7);return;}
        if unsafe{ConnectNamedPipe(h,std::ptr::null_mut())}==0 && unsafe{GetLastError()}!=535 {unsafe{CloseHandle(h)};std::thread::sleep(std::time::Duration::from_millis(100));continue;}
        stop(0);COMMAND_SEQUENCE.store(0,Ordering::Release);CONNECTED.store(true,Ordering::Release);let mut last_sequence=0;
        crate::log_game("CONTROL_IPC=CONNECTED; PROBE_STATE=OFF; no player writes until explicit PROBE_NUDGE");
        while let Some(bytes)=read_packet(h) {
            let now_ns=unsafe{crate::GetTickCount64()}*1_000_000;
            let packet=match wire::Packet::decode(&bytes).and_then(|p|p.validate_command(last_sequence,now_ns).map(|_|p)) {
                Ok(packet)=>packet,Err(e)=>{stop(8);crate::log_game(&format!("CONTROL_ERROR=MALFORMED_PACKET ({e}); PROBE_STATE=OFF"));break;}
            };
            last_sequence=packet.sequence;HEARTBEAT_NS.store(now_ns,Ordering::Release);
            if packet.kind==wire::STOP {COMMAND_SEQUENCE.store(packet.sequence,Ordering::Release);stop(0);crate::log_game("PROBE_STOP; PROBE_STATE=OFF");}
            if packet.kind==wire::PROBE_NUDGE {
                COMMAND_SEQUENCE.store(packet.sequence,Ordering::Release);
                if !ready(){stop(5);crate::log_game("PROBE_REJECTED=RUNTIME_NOT_READY");}
                else if matches!(STATE.load(Ordering::Acquire),ARMED|OBSERVING){crate::log_game("PROBE_REJECTED=BUSY");}
                else if let Ok(mut slot)=PENDING.lock(){DETAIL.store(0,Ordering::Release);STATE.store(ARMED,Ordering::Release);*slot=Some(Request {sequence:packet.sequence,generation:GENERATION.load(Ordering::Acquire),received_ns:now_ns,delta:packet.position});crate::log_game("PROBE_STATE=ARMED; awaiting game callback");}
                else {stop(6);}
            }
            let reply=status().encode();let mut written=0;
            if unsafe{WriteFile(h,reply.as_ptr().cast(),wire::BYTES as u32,&mut written,std::ptr::null_mut())}==0||written as usize!=wire::BYTES {break;}
        }
        CONNECTED.store(false,Ordering::Release);stop(0);crate::log_game("CONTROL_IPC=DISCONNECTED; PROBE_STATE=OFF");
        unsafe{DisconnectNamedPipe(h);CloseHandle(h);}
    }
}

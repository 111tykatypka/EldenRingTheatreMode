//! Independent, fixed-size local control protocol. No engine pointers or ERPLAY data.
pub const MAGIC: u32 = 0x544d4354;
pub const VERSION: u16 = 3;
pub const LEGACY_BYTES: usize = 64;
pub const BYTES: usize = 128;
pub const HELLO: u16 = 1;
pub const HEARTBEAT: u16 = 2;
pub const PROBE_NUDGE: u16 = 3;
pub const STOP: u16 = 4;
pub const REPLAY_BEGIN: u16 = 5;
pub const REPLAY_APPLY: u16 = 6;
pub const REPLAY_FINISH: u16 = 7;
pub const TRACE_START: u16 = 8;
pub const TRACE_STOP: u16 = 9;
pub const TRACE_MARK: u16 = 10;
pub const ACTOR_APPLY: u16 = 11;
pub const RUNTIME_TRACE_START:u16=12;
pub const RUNTIME_TRACE_STOP:u16=13;
pub const OWNERSHIP_PROBE:u16=14;
pub const STATUS: u16 = 0x8000;
pub const PIPE: &str = r"\\.\pipe\EldenRingTheaterMode_1_17_Control";

#[derive(Clone, Copy, Debug)]
pub struct Packet {
    pub version: u16,
    pub player_action:crate::player_action::State,
    pub replay_timestamp_ns: u64, pub session: u64,
    pub replay_state: u32, pub replay_detail: u32, pub applied_sequence: u64,
    pub kind: u16,
    pub sequence: u64,
    pub timestamp_ns: u64,
    pub position: [f32; 3],
    pub quaternion: [f32; 4],
    pub state: u32,
    pub detail: u32,
    pub flags: u32,
}
impl Packet {
    pub fn encode(self) -> [u8; BYTES] {
        let mut b = [0; BYTES];
        b[0..4].copy_from_slice(&MAGIC.to_le_bytes());
        b[4..6].copy_from_slice(&self.version.to_le_bytes());
        b[6..8].copy_from_slice(&self.kind.to_le_bytes());
        b[8..16].copy_from_slice(&self.sequence.to_le_bytes());
        b[16..24].copy_from_slice(&self.timestamp_ns.to_le_bytes());
        for (i, v) in self.position.into_iter().chain(self.quaternion).enumerate() {
            b[24 + i * 4..28 + i * 4].copy_from_slice(&v.to_le_bytes());
        }
        for (i, v) in [self.state, self.detail, self.flags].into_iter().enumerate() {
            b[52 + i * 4..56 + i * 4].copy_from_slice(&v.to_le_bytes());
        }
        b[64..72].copy_from_slice(&self.replay_timestamp_ns.to_le_bytes());
        b[72..80].copy_from_slice(&self.session.to_le_bytes());
        b[80..84].copy_from_slice(&self.replay_state.to_le_bytes());
        b[84..88].copy_from_slice(&self.replay_detail.to_le_bytes());
        b[88..96].copy_from_slice(&self.applied_sequence.to_le_bytes());
        b[96..128].copy_from_slice(&self.player_action.encode());
        b
    }
    pub fn byte_count(&self) -> usize { if self.version==1 {LEGACY_BYTES} else if self.version==2 {96} else {BYTES} }
    pub fn decode(b: &[u8]) -> Result<Self, &'static str> {
        if b.len()<LEGACY_BYTES {return Err("packet length");}
        let version=u16::from_le_bytes(b[4..6].try_into().unwrap());
        if !matches!(version,1|2|VERSION) || b.len()!=if version==1 {LEGACY_BYTES} else if version==2 {96} else {BYTES} {return Err("version/length");}
        let u32_at = |i| u32::from_le_bytes(b[i..i + 4].try_into().unwrap());
        let u64_at = |i| u64::from_le_bytes(b[i..i + 8].try_into().unwrap());
        if u32_at(0) != MAGIC {
            return Err("magic/version");
        }
        Ok(Self {
            version,player_action:if version==3 {crate::player_action::State::decode(&b[96..128])}else{Default::default()}, replay_timestamp_ns:if version==1 {0} else {u64_at(64)},session:if version==1 {0} else {u64_at(72)},
            replay_state:if version==1 {0} else {u32_at(80)},replay_detail:if version==1 {0} else {u32_at(84)},
            applied_sequence:if version==1 {0} else {u64_at(88)},
            kind: u16::from_le_bytes(b[6..8].try_into().unwrap()), sequence: u64_at(8),
            timestamp_ns: u64_at(16),
            position: std::array::from_fn(|i| f32::from_bits(u32_at(24 + i * 4))),
            quaternion: std::array::from_fn(|i| f32::from_bits(u32_at(36 + i * 4))),
            state: u32_at(52), detail: u32_at(56), flags: u32_at(60),
        })
    }
    pub fn validate_command(&self, last_sequence: u64, now_ns: u64) -> Result<(), &'static str> {
        if self.kind==OWNERSHIP_PROBE {
            if !(1..=5).contains(&self.flags)||self.replay_timestamp_ns!=0||self.replay_state!=0{return Err("ownership probe mode/reserved");}
            let actor=Self{kind:ACTOR_APPLY,flags:0,replay_state:1,..*self};return actor.validate_command(last_sequence,now_ns);
        }
        if self.kind==ACTOR_APPLY {
            if self.version!=3 || self.sequence==0 || self.sequence<=last_sequence || self.session==0 || self.applied_sequence==0 || (self.applied_sequence as u32 >>28)!=1 || self.flags!=0 || !matches!(self.replay_state,1|2) {return Err("actor identity/session/header");}
            if !self.player_action.valid() || !self.position.iter().chain(self.quaternion.iter()).all(|v|v.is_finite()) {return Err("invalid actor state");}
            let norm:f64=self.quaternion.iter().map(|v|f64::from(*v).powi(2)).sum();
            if (norm-1.0).abs()>0.001 || self.timestamp_ns>now_ns || now_ns-self.timestamp_ns>250_000_000 {return Err("invalid/stale actor target");}
            return Ok(());
        }
        if !matches!(self.kind, HELLO | HEARTBEAT | PROBE_NUDGE | STOP | REPLAY_BEGIN | REPLAY_APPLY | REPLAY_FINISH | TRACE_START | TRACE_STOP | TRACE_MARK | RUNTIME_TRACE_START | RUNTIME_TRACE_STOP) { return Err("unknown command"); }
        if matches!(self.kind,RUNTIME_TRACE_START|RUNTIME_TRACE_STOP)&&self.version!=3{return Err("runtime trace requires v3");}
        if matches!(self.kind,TRACE_START|TRACE_STOP|TRACE_MARK) && self.version!=3 {return Err("trace requires v3");}
        let replay=matches!(self.kind,REPLAY_BEGIN|REPLAY_APPLY|REPLAY_FINISH);
        if replay && self.version<2 {return Err("replay requires protocol v2");}
        if self.sequence == 0 || self.sequence <= last_sequence { return Err("sequence regression"); }
        if self.state != 0 || self.detail != 0 || (self.flags != 0 && !(replay && self.version==3 && matches!(self.flags,1|2))) || self.replay_detail!=0 || self.applied_sequence!=0 { return Err("reserved command fields"); }
        if !self.position.iter().chain(self.quaternion.iter()).all(|v| v.is_finite()) { return Err("non-finite payload"); }
        let norm: f64 = self.quaternion.iter().map(|v| f64::from(*v).powi(2)).sum();
        if (norm - 1.0).abs() > 0.001 { return Err("invalid quaternion normalization"); }
        // STOP has no write payload and is accepted even when its timestamp is old.
        if self.kind != STOP && (self.timestamp_ns > now_ns || now_ns - self.timestamp_ns > if replay {250_000_000} else {500_000_000}) {
            return Err("stale/future command");
        }
        if self.version==3 && (!self.player_action.valid() || (!replay && self.player_action!=crate::player_action::State::default())){return Err("invalid/reserved player action");}
        if replay {
            if self.session==0 || !matches!(self.replay_state,1|2) {return Err("replay session/state");}
            if self.kind==REPLAY_BEGIN && (self.replay_timestamp_ns!=0 || self.replay_state!=1) {return Err("begin must use sample zero / playing");}
            return Ok(());
        }
        if self.session!=0 || self.replay_timestamp_ns!=0 || self.replay_state!=0 {return Err("nonzero replay fields");}
        if self.quaternion != [0.0, 0.0, 0.0, 1.0] { return Err("probe orientation payload must be identity"); }
        if self.kind == PROBE_NUDGE {
            let d: f64 = self.position.iter().map(|v| f64::from(*v).powi(2)).sum();
            if self.position[1] != 0.0 || !(d > 0.0 && d <= 1.0) { return Err("probe delta must be horizontal and <= 1 unit"); }
        } else if self.kind==TRACE_MARK {if self.position[1]!=0.0||self.position[2]!=0.0||!(0.0..=6.0).contains(&self.position[0])||self.position[0].fract()!=0.0{return Err("invalid trace marker");}}
        else if self.position != [0.0; 3] { return Err("nonzero control payload"); }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn command() -> Packet { Packet { version:3,player_action:Default::default(), replay_timestamp_ns:0,session:0,replay_state:0,replay_detail:0,applied_sequence:0, kind: PROBE_NUDGE, sequence: 7, timestamp_ns: 100,
        position: [0.5, 0.0, 0.0], quaternion: [0.0, 0.0, 0.0, 1.0], state: 0, detail: 0, flags: 0 } }
    #[test]fn nightly_commands_have_explicit_modes(){
        let trace=Packet{kind:RUNTIME_TRACE_START,position:[0.;3],..command()};assert!(trace.validate_command(6,100).is_ok());assert!(Packet{version:2,..trace}.validate_command(6,100).is_err());
        let actor=Packet{kind:OWNERSHIP_PROBE,session:1,applied_sequence:0x10000001,position:[0.;3],flags:1,..command()};
        for mode in 1..=5{assert!(Packet{flags:mode,..actor}.validate_command(6,100).is_ok());}
        for mode in [0,6,32]{assert!(Packet{flags:mode,..actor}.validate_command(6,100).is_err());}
        let replay=Packet{kind:REPLAY_BEGIN,session:1,replay_state:1,flags:2,..command()};assert!(replay.validate_command(6,100).is_ok());assert!(Packet{flags:3,..replay}.validate_command(6,100).is_err());
    }
    #[test]fn actor_targets_validate_without_raw_pointers(){
        let p=Packet{kind:ACTOR_APPLY,session:1,replay_state:1,applied_sequence:0x10000001,detail:123,replay_detail:456,state:5,..command()};
        assert!(p.validate_command(6,100).is_ok());
        assert!(Packet::decode(&p.encode()).unwrap().validate_command(6,100).is_ok());
        for bad in [Packet{applied_sequence:0,..p},Packet{applied_sequence:0x20000001,..p},Packet{session:0,..p},Packet{flags:1,..p},Packet{position:[f32::NAN,0.,0.],..p},Packet{quaternion:[0.;4],..p}]{assert!(bad.validate_command(6,100).is_err());}
        assert!(p.validate_command(7,100).is_err());assert!(p.validate_command(6,250_000_101).is_err());
    }
    #[test] fn wire_layout_and_round_trip() {
        let p = command(); let b = p.encode();
        assert_eq!(&b[..8], &[0x54, 0x43, 0x4d, 0x54, 3, 0, 3, 0]);
        assert_eq!(&b[24..28], &0.5f32.to_le_bytes());
        assert_eq!(Packet::decode(&b).unwrap().position, p.position);
        let mut bad = b; bad[4] = 4; assert!(Packet::decode(&bad).is_err());
    }
    #[test] fn refuses_unsafe_or_stale_commands() {
        let p = command(); assert!(p.validate_command(6, 100).is_ok());
        assert!(p.validate_command(7, 100).is_err());
        assert!(p.validate_command(6, 500_000_101).is_err());
        assert!(p.validate_command(6, 99).is_err());
        for delta in [[f32::NAN,0.0,0.0], [f32::INFINITY,0.0,0.0], [1.1,0.0,0.0], [0.5,0.1,0.0]] {
            assert!(Packet { position: delta, ..p }.validate_command(6, 100).is_err());
        }
        assert!(Packet { quaternion: [0.0;4], ..p }.validate_command(6,100).is_err());
        assert!(Packet { kind: 99, ..p }.validate_command(6,100).is_err());
    }
    #[test] fn replay_v2_and_legacy_control() {
        let replay=Packet {kind:REPLAY_BEGIN,session:8,replay_state:1,position:[100.0,2.0,3.0],quaternion:[0.0,1.0,0.0,0.0],..command()};
        assert!(replay.validate_command(6,100).is_ok());
        let decoded=Packet::decode(&replay.encode()).unwrap(); assert_eq!(decoded.session,8);
        assert!(Packet {replay_timestamp_ns:1,..replay}.validate_command(6,100).is_err());
        assert!(Packet {session:0,..replay}.validate_command(6,100).is_err());
        assert!(Packet {quaternion:[0.0;4],..replay}.validate_command(6,100).is_err());
        let v2=Packet {version:2,..replay}.encode();assert!(Packet::decode(&v2[..96]).unwrap().validate_command(6,100).is_ok());
        let action=crate::player_action::State{flags:1,animation_id:100,..Default::default()};let current=Packet {player_action:action,flags:1,..replay};assert!(Packet::decode(&current.encode()).unwrap().validate_command(6,100).is_ok());
        assert!(Packet {player_action:crate::player_action::State{action:99,..action},..current}.validate_command(6,100).is_err());
        let old=Packet {version:1,..command()}.encode();
        assert!(Packet::decode(&old[..LEGACY_BYTES]).unwrap().validate_command(6,100).is_ok());
        assert!(Packet::decode(&old).is_err());
    }
    #[test]fn trace_commands_reject_bad_markers(){let p=Packet{kind:TRACE_MARK,position:[2.,0.,0.],..command()};assert!(p.validate_command(6,100).is_ok());for phase in [-1.,2.5,7.,f32::NAN]{assert!(Packet{position:[phase,0.,0.],..p}.validate_command(6,100).is_err());}assert!(Packet{version:2,..p}.validate_command(6,100).is_err());}

}

//! Fixed-size host <-> game control packets (status link only; see control_link.rs).
//! Kinds 3 and 5..15 belonged to the retired probe/position replay/trace tools and are no longer used.
pub const MAGIC: u32 = 0x544d4354;
pub const VERSION: u16 = 3;
pub const LEGACY_BYTES: usize = 64;
pub const BYTES: usize = 128;
pub const HELLO: u16 = 1;
pub const HEARTBEAT: u16 = 2;
pub const STOP: u16 = 4;
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
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn wire_layout_and_round_trip() {
        let p=Packet{version:3,player_action:Default::default(),replay_timestamp_ns:0,session:0,replay_state:0,replay_detail:0,applied_sequence:0,kind:HEARTBEAT,sequence:7,timestamp_ns:100,
            position:[0.5,0.0,0.0],quaternion:[0.0,0.0,0.0,1.0],state:0,detail:0,flags:0};
        let b=p.encode();
        assert_eq!(&b[..8],&[0x54,0x43,0x4d,0x54,3,0,2,0]);
        assert_eq!(&b[24..28],&0.5f32.to_le_bytes());
        assert_eq!(Packet::decode(&b).unwrap().position,p.position);
        let mut bad=b;bad[4]=4;assert!(Packet::decode(&bad).is_err());
    }
}

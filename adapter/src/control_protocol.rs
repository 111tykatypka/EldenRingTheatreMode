//! Independent, fixed-size local control protocol. No engine pointers or ERPLAY data.
pub const MAGIC: u32 = 0x544d4354;
pub const VERSION: u16 = 1;
pub const BYTES: usize = 64;
pub const HELLO: u16 = 1;
pub const HEARTBEAT: u16 = 2;
pub const PROBE_NUDGE: u16 = 3;
pub const STOP: u16 = 4;
pub const STATUS: u16 = 0x8000;
pub const PIPE: &str = r"\\.\pipe\EldenRingTheaterMode_1_17_Control";

#[derive(Clone, Copy, Debug)]
pub struct Packet {
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
        b[4..6].copy_from_slice(&VERSION.to_le_bytes());
        b[6..8].copy_from_slice(&self.kind.to_le_bytes());
        b[8..16].copy_from_slice(&self.sequence.to_le_bytes());
        b[16..24].copy_from_slice(&self.timestamp_ns.to_le_bytes());
        for (i, v) in self.position.into_iter().chain(self.quaternion).enumerate() {
            b[24 + i * 4..28 + i * 4].copy_from_slice(&v.to_le_bytes());
        }
        for (i, v) in [self.state, self.detail, self.flags].into_iter().enumerate() {
            b[52 + i * 4..56 + i * 4].copy_from_slice(&v.to_le_bytes());
        }
        b
    }
    pub fn decode(b: &[u8; BYTES]) -> Result<Self, &'static str> {
        let u32_at = |i| u32::from_le_bytes(b[i..i + 4].try_into().unwrap());
        let u64_at = |i| u64::from_le_bytes(b[i..i + 8].try_into().unwrap());
        if u32_at(0) != MAGIC || u16::from_le_bytes(b[4..6].try_into().unwrap()) != VERSION {
            return Err("magic/version");
        }
        Ok(Self {
            kind: u16::from_le_bytes(b[6..8].try_into().unwrap()), sequence: u64_at(8),
            timestamp_ns: u64_at(16),
            position: std::array::from_fn(|i| f32::from_bits(u32_at(24 + i * 4))),
            quaternion: std::array::from_fn(|i| f32::from_bits(u32_at(36 + i * 4))),
            state: u32_at(52), detail: u32_at(56), flags: u32_at(60),
        })
    }
    pub fn validate_command(&self, last_sequence: u64, now_ns: u64) -> Result<(), &'static str> {
        if !matches!(self.kind, HELLO | HEARTBEAT | PROBE_NUDGE | STOP) { return Err("unknown command"); }
        if self.sequence == 0 || self.sequence <= last_sequence { return Err("sequence regression"); }
        if self.state != 0 || self.detail != 0 || self.flags != 0 { return Err("reserved command fields"); }
        if !self.position.iter().chain(self.quaternion.iter()).all(|v| v.is_finite()) { return Err("non-finite payload"); }
        let norm: f64 = self.quaternion.iter().map(|v| f64::from(*v).powi(2)).sum();
        if (norm - 1.0).abs() > 0.001 { return Err("invalid quaternion normalization"); }
        // STOP has no write payload and is accepted even when its timestamp is old.
        if self.kind != STOP && (self.timestamp_ns > now_ns || now_ns - self.timestamp_ns > 500_000_000) {
            return Err("stale/future command");
        }
        if self.quaternion != [0.0, 0.0, 0.0, 1.0] { return Err("probe orientation payload must be identity"); }
        if self.kind == PROBE_NUDGE {
            let d: f64 = self.position.iter().map(|v| f64::from(*v).powi(2)).sum();
            if self.position[1] != 0.0 || !(d > 0.0 && d <= 1.0) { return Err("probe delta must be horizontal and <= 1 unit"); }
        } else if self.position != [0.0; 3] { return Err("nonzero control payload"); }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn command() -> Packet { Packet { kind: PROBE_NUDGE, sequence: 7, timestamp_ns: 100,
        position: [0.5, 0.0, 0.0], quaternion: [0.0, 0.0, 0.0, 1.0], state: 0, detail: 0, flags: 0 } }
    #[test] fn wire_layout_and_round_trip() {
        let p = command(); let b = p.encode();
        assert_eq!(&b[..8], &[0x54, 0x43, 0x4d, 0x54, 1, 0, 3, 0]);
        assert_eq!(&b[24..28], &0.5f32.to_le_bytes());
        assert_eq!(Packet::decode(&b).unwrap().position, p.position);
        let mut bad = b; bad[4] = 2; assert!(Packet::decode(&bad).is_err());
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
}

//! One write, followed by observation only. This is not replay playback.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Transform { pub position: [f32; 3], pub quaternion: [f32; 4] }
impl Transform {
    pub fn interpolate(self, mut next:Self, t:f64)->Self {
        let t=t.clamp(0.0,1.0);
        let normalize=|q:[f32;4]| {let n=q.iter().map(|x|f64::from(*x).powi(2)).sum::<f64>().sqrt();q.map(|x|(f64::from(x)/n) as f32)};
        let a=normalize(self.quaternion);next.quaternion=normalize(next.quaternion);
        let mut dot=a.iter().zip(next.quaternion).map(|(a,b)|f64::from(*a)*f64::from(b)).sum::<f64>();
        if dot<0.0 {dot=-dot;next.quaternion=next.quaternion.map(|x|-x);}
        let q=if dot>0.9995 {std::array::from_fn(|i|(f64::from(a[i])+t*f64::from(next.quaternion[i]-a[i])) as f32)}
        else {let angle=dot.clamp(-1.0,1.0).acos();let d=angle.sin();let x=((1.0-t)*angle).sin()/d;let y=(t*angle).sin()/d;std::array::from_fn(|i|(x*f64::from(a[i])+y*f64::from(next.quaternion[i])) as f32)};
        Self {position:std::array::from_fn(|i|(f64::from(self.position[i])+t*f64::from(next.position[i]-self.position[i])) as f32),quaternion:normalize(q)}
    }
    pub fn valid(self) -> bool {
        let norm: f64 = self.quaternion.iter().map(|v| f64::from(*v).powi(2)).sum();
        self.position.iter().chain(self.quaternion.iter()).all(|v| v.is_finite()) && (norm-1.0).abs() <= 0.01
    }
    pub fn offset(self, delta: [f32; 3]) -> Option<Self> {
        let next = Self { position: std::array::from_fn(|i| self.position[i] + delta[i]), ..self };
        (self.valid() && next.valid() && distance(self, next) <= 1.001).then_some(next)
    }
}
pub fn distance(a: Transform, b: Transform) -> f64 {
    a.position.into_iter().zip(b.position).map(|(a,b)| (f64::from(a)-f64::from(b)).powi(2)).sum::<f64>().sqrt()
}
pub const OBSERVE_NS: u64 = 2_000_000_000;
pub const LEASE_NS: u64 = 500_000_000;
pub fn lease_valid(connected: bool, heartbeat_ns: u64, now_ns: u64) -> bool {
    connected && now_ns >= heartbeat_ns && now_ns-heartbeat_ns <= LEASE_NS
}
#[derive(Clone, Copy)]
pub struct Observation {
    pub original: Transform, pub requested: Transform, pub began_ns: u64,
    pub generation: u64, pub sequence: u64, pub callbacks: u64,
}
impl Observation {
    pub fn active(&self, generation: u64, now_ns: u64) -> bool {
        self.generation == generation && now_ns >= self.began_ns && now_ns-self.began_ns < OBSERVE_NS
    }
}
#[cfg(test)] mod tests {
    use super::*;
    #[test] fn safe_offset_preserves_orientation() {
        let t=Transform { position: [1.0,2.0,3.0], quaternion: [0.0,0.0,0.0,1.0] };
        let next=t.offset([0.5,0.0,0.0]).unwrap(); assert_eq!(next.position,[1.5,2.0,3.0]);
        assert_eq!(next.quaternion,t.quaternion); assert!(t.offset([2.0,0.0,0.0]).is_none());
        assert!(Transform { quaternion: [0.0;4], ..t }.offset([0.5,0.0,0.0]).is_none());
    }
    #[test] fn timeout_disconnect_and_stop_generation() {
        assert!(lease_valid(true,10,500_000_010)); assert!(!lease_valid(true,10,500_000_011));
        assert!(!lease_valid(false,10,10)); assert!(!lease_valid(true,10,9));
        let t=Transform { position: [0.0;3], quaternion: [0.0,0.0,0.0,1.0] };
        let o=Observation { original:t,requested:t,began_ns:10,generation:2,sequence:1,callbacks:0 };
        assert!(o.active(2,11)); assert!(!o.active(3,11)); assert!(!o.active(2,2_000_000_010));
    }
}

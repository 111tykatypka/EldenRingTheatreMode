//! Lossless v2 pose/float encoding and legacy quantized v1 decoding. No game access.
//!
//! A pose is an array of hkQsTransform (translation xyzw, rotation xyzw, scale xyzw; 48 bytes).
//! v2 preserves all twelve u32 words per bone with XOR/varint. The historical v1 codec below
//! is retained for compatibility/tests; it is not used by the new writer.
//! In v1 each frame is encoded against the previous frame of the same stream (or against zero for a
//! keyframe, which starts every chunk so any chunk decodes on its own):
//! - rotation: "smallest three" (index of the largest component + the other three as i16), i.e.
//!   about 2e-5 rad resolution;
//! - translation and scale x/y/z: integers in 1/100000 m (0.01 mm);
//! - the w words of translation and scale (engine padding, kept bit-exact): raw bits XOR previous;
//! all as zigzag varint deltas, then runs of zero bytes are collapsed. Unchanged bones cost ~1 byte.
pub const QS:usize=48;
/// Lossless storage of every engine float word, including quaternion magnitude,
/// padding, signed zero and NaN payload bits. XOR is compression, not quantization.
pub fn encode_pose_exact(pose:&[u8],prev:&mut Vec<u32>,out:&mut Vec<u8>){
 assert_eq!(pose.len()%4,0);
 if prev.len()!=pose.len()/4{prev.resize(pose.len()/4,0);prev.fill(0);}
 for (word,p) in pose.chunks_exact(4).zip(prev){let v=u32::from_le_bytes(word.try_into().unwrap());put_varint(out,(v^*p) as u64);*p=v;}
}
pub fn decode_pose_exact(raw:&[u8],at:&mut usize,n:usize,prev:&mut Vec<u32>,out:&mut [u8])->Option<()>{
 if out.len()!=n.checked_mul(QS)?{return None;}
 if prev.len()!=out.len()/4{prev.resize(out.len()/4,0);prev.fill(0);}
 for (word,p) in out.chunks_exact_mut(4).zip(prev){*p^=u32::try_from(get_varint(raw,at)?).ok()?;word.copy_from_slice(&p.to_le_bytes());}Some(())
}
#[cfg(test)]mod exact_tests{
 use super::*;
 #[test]fn every_bit_survives_pose_storage(){
  let pose:Vec<u8>=[0u32,0x80000000,0x7fc01234,0x7f800000,0xff800000,1,0x3f800000,0x3f000001,0xdeadbeef,7,8,9].into_iter().flat_map(u32::to_le_bytes).collect();
  let(mut enc,mut dec,mut raw,mut out)=(Vec::new(),Vec::new(),Vec::new(),vec![0;QS]);
  encode_pose_exact(&pose,&mut enc,&mut raw);encode_pose_exact(&pose,&mut enc,&mut raw);
  let mut at=0;decode_pose_exact(&raw,&mut at,1,&mut dec,&mut out).unwrap();assert_eq!(out,pose);
  decode_pose_exact(&raw,&mut at,1,&mut dec,&mut out).unwrap();assert_eq!(out,pose);assert_eq!(at,raw.len());
  let mut at=0;let mut prev=Vec::new();let truncated=&raw[..raw.len()-1];
  decode_pose_exact(truncated,&mut at,1,&mut prev,&mut out).unwrap();
  assert!(decode_pose_exact(truncated,&mut at,1,&mut prev,&mut out).is_none());
 }
}
const T_SCALE:f64=100_000.0;
const Q_SCALE:f64=32767.0/std::f64::consts::FRAC_1_SQRT_2;

fn zigzag(v:i64)->u64{((v<<1)^(v>>63)) as u64}
fn unzigzag(v:u64)->i64{((v>>1) as i64)^-((v&1) as i64)}
pub fn put_varint(out:&mut Vec<u8>,mut v:u64){while v>=0x80{out.push((v as u8)|0x80);v>>=7;}out.push(v as u8);}
pub fn get_varint(b:&[u8],at:&mut usize)->Option<u64>{let mut v=0u64;let mut shift=0;loop{let x=*b.get(*at)?;*at+=1;if shift==63&&x&0xfe!=0{return None;}v|=((x&0x7f) as u64)<<shift;if x&0x80==0{return Some(v);}shift+=7;if shift>63{return None;}}}
pub fn put_signed(out:&mut Vec<u8>,v:i64){put_varint(out,zigzag(v));}
pub fn get_signed(b:&[u8],at:&mut usize)->Option<i64>{get_varint(b,at).map(unzigzag)}

/// One bone in quantized form; the encoder works on these, frame to frame.
#[derive(Clone,Copy,Default,PartialEq,Debug)]
pub struct QBone{q_index:i64,q:[i64;3],t:[i64;3],s:[i64;3],t_w:u32,s_w:u32}
#[cfg(test)]fn f(b:&[u8],i:usize)->f32{f32::from_le_bytes(b[i*4..i*4+4].try_into().unwrap())}
#[cfg(test)]pub fn quantize(bone:&[u8])->QBone{
 let mut q=[f(bone,4) as f64,f(bone,5) as f64,f(bone,6) as f64,f(bone,7) as f64];
 let n=q.iter().map(|v|v*v).sum::<f64>().sqrt();if n>1e-12&&n.is_finite(){for v in &mut q{*v/=n;}}else{q=[0.0,0.0,0.0,1.0];}
 let largest=(0..4).max_by(|a,b|q[*a].abs().total_cmp(&q[*b].abs())).unwrap();
 if q[largest]<0.0{for v in &mut q{*v=-*v;}}
 let mut rest=[0i64;3];let mut k=0;for (i,v) in q.iter().enumerate(){if i!=largest{rest[k]=(v*Q_SCALE).round() as i64;k+=1;}}
 let fx=|i:usize|{let v=f(bone,i) as f64;if v.is_finite(){(v*T_SCALE).round() as i64}else{0}};
 QBone{q_index:largest as i64,q:rest,t:[fx(0),fx(1),fx(2)],s:[fx(8),fx(9),fx(10)],
  t_w:u32::from_le_bytes(bone[12..16].try_into().unwrap()),s_w:u32::from_le_bytes(bone[44..48].try_into().unwrap())}}
pub fn dequantize(b:&QBone,out:&mut [u8]){
 let mut q=[0f64;4];let mut k=0;for (i,v) in q.iter_mut().enumerate(){if i as i64!=b.q_index{*v=b.q[k] as f64/Q_SCALE;k+=1;}}
 let rest:f64=q.iter().map(|v|v*v).sum();q[b.q_index as usize]=(1.0-rest).max(0.0).sqrt();
 let put=|out:&mut [u8],i:usize,v:f32|out[i*4..i*4+4].copy_from_slice(&v.to_le_bytes());
 for i in 0..3{put(out,i,(b.t[i] as f64/T_SCALE) as f32);put(out,8+i,(b.s[i] as f64/T_SCALE) as f32);}
 for i in 0..4{put(out,4+i,q[i] as f32);}
 out[12..16].copy_from_slice(&b.t_w.to_le_bytes());out[44..48].copy_from_slice(&b.s_w.to_le_bytes());}

/// Encodes `pose` (n * 48 bytes) against `prev` (same length; updated to this frame). A keyframe
/// passes a fresh zeroed `prev`.
#[cfg(test)]pub fn encode_pose(pose:&[u8],prev:&mut Vec<QBone>,out:&mut Vec<u8>){
 let n=pose.len()/QS;if prev.len()!=n{*prev=vec![QBone::default();n];}
 for (i,p) in prev.iter_mut().enumerate(){
  let b=quantize(&pose[i*QS..i*QS+QS]);
  put_signed(out,b.q_index-p.q_index);for k in 0..3{put_signed(out,b.q[k]-p.q[k]);}
  for k in 0..3{put_signed(out,b.t[k]-p.t[k]);}for k in 0..3{put_signed(out,b.s[k]-p.s[k]);}
  put_varint(out,(b.t_w^p.t_w) as u64);put_varint(out,(b.s_w^p.s_w) as u64);
  *p=b;}}
pub fn decode_pose(b:&[u8],at:&mut usize,n:usize,prev:&mut Vec<QBone>,out:&mut [u8])->Option<()>{
 if prev.len()!=n{*prev=vec![QBone::default();n];}if out.len()!=n*QS{return None;}
 for (i,p) in prev.iter_mut().enumerate(){
  let mut q=*p;
  q.q_index=p.q_index+get_signed(b,at)?;if !(0..4).contains(&q.q_index){return None;}
  for k in 0..3{q.q[k]=p.q[k]+get_signed(b,at)?;}for k in 0..3{q.t[k]=p.t[k]+get_signed(b,at)?;}for k in 0..3{q.s[k]=p.s[k]+get_signed(b,at)?;}
  q.t_w=p.t_w^(get_varint(b,at)? as u32);q.s_w=p.s_w^(get_varint(b,at)? as u32);
  dequantize(&q,&mut out[i*QS..i*QS+QS]);*p=q;}
 Some(())}

/// f32 fields kept bit-exact: XOR with the previous value, varint.
pub fn encode_f32s(v:&[f32],prev:&mut Vec<u32>,out:&mut Vec<u8>){
 if prev.len()!=v.len(){*prev=vec![0;v.len()];}
 for (x,p) in v.iter().zip(prev.iter_mut()){let bits=x.to_bits();put_varint(out,(bits^*p) as u64);*p=bits;}}
pub fn decode_f32s(b:&[u8],at:&mut usize,prev:&mut Vec<u32>,out:&mut [f32])->Option<()>{
 if prev.len()!=out.len(){*prev=vec![0;out.len()];}
 for (x,p) in out.iter_mut().zip(prev.iter_mut()){*p^=u32::try_from(get_varint(b,at)?).ok()?;*x=f32::from_bits(*p);}Some(())}

/// Collapses runs of zero bytes: 0x00 followed by (run length - 1). Most delta streams are zeros.
pub fn pack_zeros(raw:&[u8])->Vec<u8>{
 let mut out=Vec::with_capacity(raw.len()/2);let mut i=0;
 while i<raw.len(){if raw[i]==0{let mut run=1;while i+run<raw.len()&&raw[i+run]==0&&run<256{run+=1;}out.push(0);out.push((run-1) as u8);i+=run;}else{out.push(raw[i]);i+=1;}}
 out}
pub fn unpack_zeros(packed:&[u8])->Option<Vec<u8>>{
 let mut out=Vec::with_capacity(packed.len()*3);let mut i=0;
 while i<packed.len(){if packed[i]==0{let run=*packed.get(i+1)? as usize+1;out.resize(out.len()+run,0);i+=2;}else{out.push(packed[i]);i+=1;}}
 Some(out)}

/// CRC-32 (IEEE) for chunk integrity.
pub fn crc32(data:&[u8])->u32{
 static TABLE:std::sync::OnceLock<[u32;256]>=std::sync::OnceLock::new();
 let t=TABLE.get_or_init(||std::array::from_fn(|i|{let mut c=i as u32;for _ in 0..8{c=if c&1!=0{0xEDB88320^(c>>1)}else{c>>1};}c}));
 let mut c=!0u32;for b in data{c=t[((c^*b as u32)&0xff) as usize]^(c>>8);}!c}

#[cfg(test)]mod tests{
 use super::*;
 fn bone(t:[f32;3],q:[f32;4],s:f32)->Vec<u8>{let v=[t[0],t[1],t[2],0.0,q[0],q[1],q[2],q[3],s,s,s,1.0];v.iter().flat_map(|x|x.to_le_bytes()).collect()}
 fn pose(k:f32)->Vec<u8>{(0..150).flat_map(|i|{let a=(i as f32*0.1+k).sin()*0.7;let n=(a*a+0.3f32.powi(2)+1.0).sqrt();bone([0.1*i as f32,k,-0.5],[a/n,0.3/n,0.0,1.0/n],1.0)}).collect()}
 fn floats(b:&[u8])->Vec<f32>{b.chunks_exact(4).map(|x|f32::from_le_bytes(x.try_into().unwrap())).collect()}
 #[test]fn varint_round_trip(){let mut o=Vec::new();for v in [0i64,1,-1,63,-64,300,-300,i32::MAX as i64,i64::MIN/2]{put_signed(&mut o,v);}let mut at=0;for v in [0i64,1,-1,63,-64,300,-300,i32::MAX as i64,i64::MIN/2]{assert_eq!(get_signed(&o,&mut at),Some(v));}}
 #[test]fn pose_round_trip_is_accurate_and_small(){
  let frames:Vec<Vec<u8>>=(0..60).map(|i|pose(i as f32*0.01)).collect();
  let mut prev=Vec::new();let mut raw=Vec::new();for f in &frames{encode_pose(f,&mut prev,&mut raw);}
  let packed=pack_zeros(&raw);let raw2=unpack_zeros(&packed).unwrap();assert_eq!(raw,raw2);
  let mut prev=Vec::new();let mut at=0;let mut out=vec![0u8;150*QS];
  for f in &frames{decode_pose(&raw2,&mut at,150,&mut prev,&mut out).unwrap();
   for (a,b) in floats(f).chunks_exact(12).zip(floats(&out).chunks_exact(12)){
    for k in 0..3{assert!((a[k]-b[k]).abs()<2e-5);assert!((a[8+k]-b[8+k]).abs()<2e-5);}
    let dot:f32=(4..8).map(|k|a[k]*b[k]).sum();assert!(dot.abs()>0.99999);
    assert_eq!(a[3].to_bits(),b[3].to_bits());assert_eq!(a[11].to_bits(),b[11].to_bits());}}
  assert_eq!(at,raw2.len());
  let size=packed.len() as f64/60.0;assert!(size<150.0*16.0,"bytes per frame {size}"); // vs 7200 raw
 }
 #[test]fn f32_round_trip_exact(){let v=[1.5f32,-0.0,f32::NAN,3e-30];let mut p=Vec::new();let mut o=Vec::new();encode_f32s(&v,&mut p,&mut o);let mut q=Vec::new();let mut at=0;let mut out=[0f32;4];decode_f32s(&o,&mut at,&mut q,&mut out).unwrap();for (a,b) in v.iter().zip(out){assert_eq!(a.to_bits(),b.to_bits());}}
 #[test]fn crc_known_value(){assert_eq!(crc32(b"123456789"),0xCBF43926);}
 #[test]fn overflowing_words_are_rejected(){assert!(get_varint(&[255,255,255,255,255,255,255,255,255,2],&mut 0).is_none());let mut b=Vec::new();put_varint(&mut b,1u64<<32);assert!(decode_f32s(&b,&mut 0,&mut Vec::new(),&mut [0.0]).is_none());}
}

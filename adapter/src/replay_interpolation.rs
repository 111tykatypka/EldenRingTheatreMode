//! Pure interpolation of the known recorded physics transform. No game memory access.
pub type Transform=[[f32;4];3];
fn usable_quaternion(q:&[f32])->bool{
 let n=q.iter().map(|x|(*x as f64).powi(2)).sum::<f64>();
 q.len()==4&&q.iter().all(|x|x.is_finite())&&n.is_finite()&&(n-1.0).abs()<=0.1
}
/// Storage preserves every bit for research; game writes accept semantic components only.
/// hkQsTransform padding W words are deliberately excluded (they are not pose coordinates).
pub fn pose_safe_to_apply(pose:&[u8])->bool{
 !pose.is_empty()&&pose.len()%48==0&&pose.chunks_exact(48).all(|b|{
  let values=std::array::from_fn::<_,12,_>(|i|f32::from_le_bytes(b[i*4..i*4+4].try_into().unwrap()));
  [0,1,2,8,9,10].into_iter().all(|i|values[i].is_finite())&&usable_quaternion(&values[4..8])
 })
}
pub fn source_time(source:u64,anchor:u64,now:u64,playing:bool,speed:f64)->u64{
 if !playing||anchor==0||!speed.is_finite()||speed<=0.0{return source;}
 source.saturating_add((now.saturating_sub(anchor).min(250_000_000) as f64*speed) as u64)
}
/// Playback in the same measured coordinate frame preserves the physics trajectory.
/// ChrIns.chunk_position is not a sampled moving root; never replace root XYZ with it.
/// Different origin translations need a verified conversion, not a guessed sign/offset.
/// Physics-space translation that carries a position recorded under `recorded_anchor` into the live
/// physics frame (`live_anchor`).
///
/// MEASURED on real recordings (tools/dump_player_root.py): when the game re-bases the physics origin,
/// the player's physics position jumps by exactly the change of `ChrIns.chunk_position` (physics x
/// 32.09 -> 0.15 = -31.94 while the anchor went -48 -> -80), and `physics - anchor` stays continuous
/// (80.09 -> 80.15, one frame of running). So a world point is `physics - anchor`, and
///   live_physics = recorded_physics + (live_anchor - recorded_anchor).
/// Different origin ids (or non-finite values) have no known conversion: None.
pub fn rebase(recorded_origin:i32,recorded_anchor:[f32;4],live_origin:i32,live_anchor:[f32;4])->Option<[f32;3]>{
 if recorded_origin!=live_origin||!(0..3).all(|i|recorded_anchor[i].is_finite()&&live_anchor[i].is_finite()){return None;}
 Some(std::array::from_fn(|i|live_anchor[i]-recorded_anchor[i]))}
/// The player's anchor over the recording (only where it changes). Everything in a recording shares one
/// physics frame, so enemies and NPCs are rebased with the PLAYER's anchor at their sample time (their own
/// chunk fields are not a frame anchor: a stationary enemy kept a constant, unrelated value).
#[derive(Clone,Debug,Default)]
pub struct AnchorTrack{entries:Vec<(u64,i32,[f32;4])>}
impl AnchorTrack{
 pub fn push(&mut self,time:u64,origin:i32,anchor:[f32;4]){if self.entries.last().is_none_or(|l|l.1!=origin||l.2[..3]!=anchor[..3]){self.entries.push((time,origin,anchor));}}
 pub fn is_empty(&self)->bool{self.entries.is_empty()}
 pub fn len_changes(&self)->usize{self.entries.len().saturating_sub(1)}
 /// Anchor in force at `time` (the first entry applies to everything before it).
 pub fn at(&self,time:u64)->Option<(i32,[f32;4])>{if self.entries.is_empty(){return None;}let i=self.entries.partition_point(|e|e.0<=time).saturating_sub(1);let e=self.entries[i];Some((e.1,e.2))}
 /// Translation into the live frame for a sample recorded at `time`.
 pub fn translation(&self,time:u64,live_origin:i32,live_anchor:[f32;4])->Option<[f32;3]>{let (o,a)=self.at(time)?;rebase(o,a,live_origin,live_anchor)}
}
pub fn same_root_space(recorded_origin:i32,recorded_chunk:[f32;4],live_origin:i32,live_chunk:[f32;4])->bool{
 recorded_origin==live_origin&&(0..3).all(|i|recorded_chunk[i].is_finite()&&live_chunk[i].is_finite()&&(recorded_chunk[i]-live_chunk[i]).abs()<0.01)
}
fn quaternion(a:[f32;4],mut b:[f32;4],t:f64)->Option<[f32;4]> {
    fn unit(mut q:[f32;4])->Option<[f32;4]> {
        if !q.iter().all(|v|v.is_finite()){return None;}
        let n=q.iter().map(|v|(*v as f64).powi(2)).sum::<f64>().sqrt();
        if n<1e-6{return None;}for v in &mut q{*v=(*v as f64/n)as f32;}Some(q)
    }
    let a=unit(a)?;b=unit(b)?;
    let mut dot=a.iter().zip(b).map(|(x,y)|*x as f64*y as f64).sum::<f64>();
    if dot<0.0{dot=-dot;for v in &mut b{*v = -*v;}}
    let (wa,wb)=if dot>0.9995{(1.0-t,t)}else{
        let angle=dot.clamp(-1.0,1.0).acos();let den=angle.sin();
        (((1.0-t)*angle).sin()/den,(t*angle).sin()/den)
    };
    unit(std::array::from_fn(|i|(a[i]as f64*wa+b[i]as f64*wb)as f32))
}
pub fn evaluate(a:&Transform,b:&Transform,t:f64)->Option<Transform> {
    if !t.is_finite()||!a.iter().flatten().chain(b.iter().flatten()).all(|v|v.is_finite())
       ||!usable_quaternion(&a[0])||!usable_quaternion(&a[1])||!usable_quaternion(&b[0])||!usable_quaternion(&b[1]){return None;}
    let t=t.clamp(0.0,1.0);
    // Preserve recorded endpoints exactly, including engine padding/position W.
    if t==0.0{return Some(*a);}if t==1.0{return Some(*b);}
    let mut out=*a;
    out[0]=quaternion(a[0],b[0],t)?;out[1]=quaternion(a[1],b[1],t)?;
    for i in 0..3{out[2][i]=(a[2][i]as f64+(b[2][i]as f64-a[2][i]as f64)*t)as f32;}
    Some(out)
}
#[cfg(test)]mod tests {
    use super::*;
    #[test]fn master_clock_pause_seek_and_speed(){
     assert_eq!(source_time(1000,100,200,false,4.0),1000);
     assert_eq!(source_time(1000,100,200,true,0.5),1050);
     assert_eq!(source_time(1000,100,200,true,2.0),1200);
     assert_eq!(source_time(10,200,200,true,1.0),10); // backward seek is exact
     assert_eq!(source_time(10,200,100,true,1.0),10); // no timestamp underflow
    }
    #[test]fn rebase_matches_the_measured_origin_shift(){
        // Real frames from 213.erplay.world around an origin re-base: anchor x -48 -> -80, physics x 32.09 -> 0.15.
        let before=[32.09f32,-5.46,-4.44];let after_anchor=[-80.0,-104.0,-96.0,1.0];let before_anchor=[-48.0,-104.0,-96.0,1.0];
        let t=rebase(-1,before_anchor,-1,after_anchor).unwrap();
        assert!((before[0]+t[0]-0.09).abs()<0.01,"rebased x {}",before[0]+t[0]); // next real frame was 0.15: one frame of motion
        assert_eq!(t,[-32.0,0.0,0.0]);assert!(rebase(-1,before_anchor,7,after_anchor).is_none());assert!(rebase(-1,[f32::NAN;4],-1,after_anchor).is_none());}
    #[test]fn anchor_track_applies_the_anchor_in_force(){
        let mut tr=AnchorTrack::default();tr.push(0,-1,[-48.,-104.,-96.,1.]);tr.push(10,-1,[-48.,-104.,-96.,1.]);tr.push(20,-1,[-80.,-104.,-96.,1.]);
        assert_eq!(tr.entries.len(),2);assert_eq!(tr.translation(5,-1,[-80.,-104.,-96.,1.]),Some([-32.,0.,0.]));assert_eq!(tr.translation(25,-1,[-80.,-104.,-96.,1.]),Some([0.,0.,0.]));}
    #[test]fn endpoints_and_shortest_quaternion(){
        let a=[[0.,0.,0.,1.],[0.,0.,0.,1.],[1.,2.,3.,1.]];
        let b=[[0.,0.,0.,-1.],[0.,1.,0.,0.],[3.,4.,5.,1.]];
        assert_eq!(evaluate(&a,&b,0.).unwrap(),a);assert_eq!(evaluate(&a,&b,1.).unwrap(),b);
        let mid=evaluate(&a,&b,0.5).unwrap();assert_eq!(mid[2],[2.,3.,4.,1.]);
        for q in &mid[..2]{assert!((q.iter().map(|v|v*v).sum::<f32>()-1.).abs()<1e-5);}
    }
    #[test]fn reject_invalid_data(){let a=[[0.,0.,0.,1.];3];let mut b=a;b[2][0]=f32::NAN;assert!(evaluate(&a,&b,0.5).is_none());}
    #[test]fn writes_reject_invalid_endpoints_but_ignore_pose_padding(){
     let a=[[0.,0.,0.,1.];3];let mut b=a;b[0]=[0.;4];assert!(evaluate(&a,&b,1.).is_none());
     let mut pose:Vec<u8>=[0.,0.,0.,f32::NAN,0.,0.,0.,1.,1.,1.,1.,f32::NAN].into_iter().flat_map(f32::to_le_bytes).collect();
     assert!(pose_safe_to_apply(&pose));pose[16..32].fill(0);assert!(!pose_safe_to_apply(&pose));assert!(!pose_safe_to_apply(&[]));
    }
    #[test]fn chunk_anchor_cannot_cancel_root_motion(){
      let a=[[0.,0.,0.,1.],[0.,0.,0.,1.],[-12.,3.,0.,1.]];
      let b=[[0.,0.,0.,1.],[0.,0.,0.,1.],[11.,9.,28.,1.]];
      let anchor=[-16.,-104.,-80.,1.];assert!(same_root_space(1,anchor,1,anchor));
      assert_eq!(evaluate(&a,&b,0.5).unwrap()[2],[ -0.5,6.,14.,1.]);
      assert!(!same_root_space(1,anchor,2,anchor));
      assert!(!same_root_space(1,anchor,1,[0.;4]));
    }
}

/// Recorded hkQsTransform: translation vec4, XYZW quaternion, scale vec4.
/// Preserve padding and exact endpoints. Interpolate known semantic components only.
/// A bone whose data cannot be interpolated (non-finite values, zero quaternion) keeps `a` exactly;
/// returns how many bones did. None only for mismatched sizes or a non-finite `t`.
pub fn pose_into(a:&[u8],b:&[u8],t:f64,out:&mut [u8])->Option<usize> {
    if a.len()!=b.len()||a.len()!=out.len()||a.len()%48!=0||!t.is_finite(){return None;}
    let t=t.clamp(0.0,1.0);
    if t==0.0{out.copy_from_slice(a);return Some(0);}
    if t==1.0{out.copy_from_slice(b);return Some(0);}
    let mut kept=0;
    for ((ab,bb),out) in a.chunks_exact(48).zip(b.chunks_exact(48)).zip(out.chunks_exact_mut(48)) {
        let decode=|v:&[u8]|std::array::from_fn::<_,12,_>(|i|f32::from_le_bytes(v[i*4..i*4+4].try_into().unwrap()));
        let (a,b)=(decode(ab),decode(bb));
        let blend=||->Option<[f32;12]>{
            let mut result=a;
            result[4..8].copy_from_slice(&quaternion(a[4..8].try_into().ok()?,b[4..8].try_into().ok()?,t)?);
            for i in [0,1,2,8,9,10]{
                if !a[i].is_finite()||!b[i].is_finite(){return None;}
                result[i]=(a[i]as f64+(b[i]as f64-a[i]as f64)*t)as f32;
            }
            Some(result)
        };
        match blend(){
            Some(result)=>for(i,v)in result.iter().enumerate(){out[i*4..i*4+4].copy_from_slice(&v.to_le_bytes());},
            None=>{out.copy_from_slice(ab);kept+=1;}
        }
    }
    Some(kept)
}
#[cfg(test)]mod pose_tests {
    use super::*;
    fn encode(v:[f32;12])->Vec<u8>{v.iter().flat_map(|x|x.to_le_bytes()).collect()}
    #[test]fn pose_midpoint_and_endpoints(){
        let a=encode([0.,2.,4.,42.,0.,0.,0.,1.,1.,1.,1.,9.]);
        let b=encode([2.,4.,6.,99.,0.,1.,0.,0.,2.,2.,2.,8.]);
        let mut out=vec![0;48];pose_into(&a,&b,0.,&mut out).unwrap();assert_eq!(out,a);
        pose_into(&a,&b,1.,&mut out).unwrap();assert_eq!(out,b);
        pose_into(&a,&b,0.5,&mut out).unwrap();
        let v:Vec<f32>=out.chunks_exact(4).map(|x|f32::from_le_bytes(x.try_into().unwrap())).collect();
        assert_eq!(&v[..4],&[1.,3.,5.,42.]);assert_eq!(&v[8..],&[1.5,1.5,1.5,9.]);
        assert!((v[4..8].iter().map(|x|x*x).sum::<f32>()-1.).abs()<1e-5);
    }
    #[test]fn invalid_bone_keeps_earlier_frame(){
        let a=vec![0;48];let b=encode([1.,1.,1.,0.,0.,0.,0.,1.,1.,1.,1.,0.]);let mut out=vec![7;48];
        assert_eq!(pose_into(&a,&b,0.5,&mut out),Some(1));assert_eq!(out,a);
        assert!(pose_into(&a,&a,f64::NAN,&mut out).is_none());assert!(pose_into(&a[..47],&a[..47],0.5,&mut out[..47]).is_none());}
    #[test]fn one_bad_bone_does_not_stop_others(){
        let good_a=encode([0.,0.,0.,1.,0.,0.,0.,1.,1.,1.,1.,0.]);let good_b=encode([2.,0.,0.,1.,0.,0.,0.,1.,1.,1.,1.,0.]);
        let a=[good_a.clone(),vec![0;48]].concat();let b=[good_b,vec![0;48]].concat();let mut out=vec![0;96];
        assert_eq!(pose_into(&a,&b,0.5,&mut out),Some(1));
        assert_eq!(f32::from_le_bytes(out[0..4].try_into().unwrap()),1.0);assert_eq!(&out[48..],&a[48..]);}
}

// Skeleton hierarchy, learned from the recording itself (no game offsets): in Havok a bone's
// model-space transform is its parent's model-space transform times its own local transform
// (hkQsTransform::setMul), and a parent always has a lower index than its children. Interpolating
// each bone's model-space transform on its own can stretch or detach limbs between frames; rebuilding
// model space from the interpolated local pose keeps every bone attached.
pub const UNKNOWN_PARENT:i16=-2;
type Qs=[f32;12]; // translation xyzw, rotation xyzw, scale xyzw
fn qs(v:&[u8])->Qs{std::array::from_fn(|i|f32::from_le_bytes(v[i*4..i*4+4].try_into().unwrap()))}
fn rotate(q:[f32;4],v:[f32;3])->[f32;3]{
    let (x,y,z,w)=(q[0] as f64,q[1] as f64,q[2] as f64,q[3] as f64);let (vx,vy,vz)=(v[0] as f64,v[1] as f64,v[2] as f64);
    let (tx,ty,tz)=(2.0*(y*vz-z*vy),2.0*(z*vx-x*vz),2.0*(x*vy-y*vx));
    [(vx+w*tx+(y*tz-z*ty)) as f32,(vy+w*ty+(z*tx-x*tz)) as f32,(vz+w*tz+(x*ty-y*tx)) as f32]}
fn qmul(a:[f32;4],b:[f32;4])->[f32;4]{let (ax,ay,az,aw)=(a[0],a[1],a[2],a[3]);let (bx,by,bz,bw)=(b[0],b[1],b[2],b[3]);
    [aw*bx+ax*bw+ay*bz-az*by,aw*by-ax*bz+ay*bw+az*bx,aw*bz+ax*by-ay*bx+az*bw,aw*bw-ax*bx-ay*by-az*bz]}
/// parent (model) * child (local), keeping the child's padding words from `template`.
fn compose(parent:&Qs,local:&Qs,template:&Qs)->Qs{
    let mut out=*template;
    let scaled=[parent[8]*local[0],parent[9]*local[1],parent[10]*local[2]];let r=rotate([parent[4],parent[5],parent[6],parent[7]],scaled);
    for i in 0..3{out[i]=parent[i]+r[i];}
    out[4..8].copy_from_slice(&qmul([parent[4],parent[5],parent[6],parent[7]],[local[4],local[5],local[6],local[7]]));
    for i in 0..3{out[8+i]=parent[8+i]*local[8+i];}
    out}
fn close(a:&Qs,b:&Qs)->bool{
    let dt=(0..3).map(|i|((a[i]-b[i]) as f64).powi(2)).sum::<f64>().sqrt();
    let dot=(4..8).map(|i|a[i] as f64*b[i] as f64).sum::<f64>().abs();
    dt<0.001&&dot>0.99999&&(8..11).all(|i|(a[i]-b[i]).abs()<0.001)}
/// Parent index per bone (-1 root, UNKNOWN_PARENT when no lower bone explains it) for one frame.
pub fn parents_of(local:&[u8],model:&[u8])->Vec<i16>{
    let bones=local.len()/48;let l:Vec<Qs>=(0..bones).map(|i|qs(&local[i*48..])).collect();let m:Vec<Qs>=(0..bones).map(|i|qs(&model[i*48..])).collect();
    (0..bones).map(|c|{
        if !l[c].iter().chain(m[c].iter()).all(|v|v.is_finite()){return UNKNOWN_PARENT;}
        let matches:Vec<usize>=(0..c).filter(|&p|close(&compose(&m[p],&l[c],&m[c]),&m[c])).collect();
        if matches.len()==1{matches[0] as i16}else if matches.is_empty()&&close(&l[c],&m[c]){-1}else{UNKNOWN_PARENT}
    }).collect()}
/// Parents that hold on every given frame; a bone that disagrees anywhere becomes UNKNOWN_PARENT.
pub fn learn_parents<'a>(frames:impl Iterator<Item=(&'a [u8],&'a [u8])>)->Vec<i16>{
    let mut result:Option<Vec<i16>>=None;
    for (l,m) in frames{let p=parents_of(l,m);result=Some(match result{None=>p,Some(r)=>r.iter().zip(&p).map(|(a,b)|if a==b{*a}else{UNKNOWN_PARENT}).collect()});}
    result.unwrap_or_default()}
/// Model-space pose for an interpolated local pose. Bones with a known parent are rebuilt through the
/// hierarchy; others (and children of failed bones) use their own interpolated model transform.
/// Endpoints stay byte-exact. Returns how many bones fell back.
pub fn model_from_local(local:&[u8],parents:&[i16],model_a:&[u8],model_b:&[u8],t:f64,out:&mut [u8])->Option<usize>{
    let bones=local.len()/48;
    if parents.len()!=bones||model_a.len()!=local.len()||model_b.len()!=local.len()||out.len()!=local.len(){return None;}
    let mut fallback=pose_into(model_a,model_b,t,out)?; // independent result, also the exact endpoints
    if t<=0.0||t>=1.0{return Some(fallback);}
    let mut rebuilt=vec![false;bones];
    for c in 0..bones{
        let template=qs(&out[c*48..c*48+48]);let lc=qs(&local[c*48..c*48+48]);
        let value=match parents[c]{
            -1=>{let mut v=template;v[..11].copy_from_slice(&lc[..11]);v[3]=template[3];v[11]=template[11];Some(v)},
            p if p>=0&&(p as usize)<c&&rebuilt[p as usize]=>Some(compose(&qs(&out[p as usize*48..p as usize*48+48]),&lc,&template)),
            _=>None};
        match value{
            Some(v) if v.iter().all(|x|x.is_finite())=>{for(i,x)in v.iter().enumerate(){out[c*48+i*4..c*48+i*4+4].copy_from_slice(&x.to_le_bytes());}rebuilt[c]=true;}
            _=>{fallback+=usize::from(parents[c]!=UNKNOWN_PARENT);}
        }
    }
    Some(fallback)}
#[cfg(test)]mod hierarchy_tests {
    use super::*;
    fn bytes(v:&[Qs])->Vec<u8>{v.iter().flat_map(|q|q.iter().flat_map(|x|x.to_le_bytes())).collect()}
    fn axis(angle:f32)->[f32;4]{[0.,(angle/2.).sin(),0.,(angle/2.).cos()]}
    fn chain(a0:f32,a1:f32)->(Vec<u8>,Vec<u8>){
        // root at (0,1,0) rotated a0; child 0.5 forward rotated a1; grandchild 0.3 forward.
        let l=[[0.,1.,0.,0.,axis(a0)[0],axis(a0)[1],axis(a0)[2],axis(a0)[3],1.,1.,1.,0.],
               [0.5,0.,0.,0.,axis(a1)[0],axis(a1)[1],axis(a1)[2],axis(a1)[3],1.,1.,1.,0.],
               [0.3,0.,0.,0.,0.,0.,0.,1.,1.,1.,1.,0.]];
        let m0=l[0];let m1=compose(&m0,&l[1],&l[1]);let m2=compose(&m1,&l[2],&l[2]);
        (bytes(&l),bytes(&[m0,m1,m2]))}
    #[test]fn learns_parents(){let (l,m)=chain(0.3,0.9);assert_eq!(parents_of(&l,&m),vec![-1,0,1]);
        let (l2,m2)=chain(1.1,-0.4);assert_eq!(learn_parents([(&l[..],&m[..]),(&l2[..],&m2[..])].into_iter()),vec![-1,0,1]);}
    #[test]fn rebuilt_model_keeps_bone_lengths(){
        let (la,ma)=chain(0.0,0.0);let (lb,mb)=chain(1.5,1.5);let parents=parents_of(&la,&ma);
        let mut local=vec![0;la.len()];pose_into(&la,&lb,0.5,&mut local).unwrap();
        let mut model=vec![0;la.len()];assert_eq!(model_from_local(&local,&parents,&ma,&mb,0.5,&mut model),Some(0));
        let m:Vec<Qs>=(0..3).map(|i|qs(&model[i*48..])).collect();
        let len=|a:&Qs,b:&Qs|(0..3).map(|i|(a[i]-b[i]).powi(2)).sum::<f32>().sqrt();
        assert!((len(&m[0],&m[1])-0.5).abs()<1e-5);assert!((len(&m[1],&m[2])-0.3).abs()<1e-5);
        let mut endpoint=vec![0;la.len()];model_from_local(&la,&parents,&ma,&mb,0.0,&mut endpoint).unwrap();assert_eq!(endpoint,ma);}
}

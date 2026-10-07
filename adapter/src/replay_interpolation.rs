//! Pure interpolation of the known recorded physics transform. No game memory access.
pub type Transform=[[f32;4];3];
/// Playback in the same measured coordinate frame preserves the physics trajectory.
/// ChrIns.chunk_position is not a sampled moving root; never replace root XYZ with it.
/// Different origin translations need a verified conversion, not a guessed sign/offset.
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
    if !t.is_finite()||!a.iter().flatten().chain(b.iter().flatten()).all(|v|v.is_finite()){return None;}
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
    #[test]fn endpoints_and_shortest_quaternion(){
        let a=[[0.,0.,0.,1.],[0.,0.,0.,1.],[1.,2.,3.,1.]];
        let b=[[0.,0.,0.,-1.],[0.,1.,0.,0.],[3.,4.,5.,1.]];
        assert_eq!(evaluate(&a,&b,0.).unwrap(),a);assert_eq!(evaluate(&a,&b,1.).unwrap(),b);
        let mid=evaluate(&a,&b,0.5).unwrap();assert_eq!(mid[2],[2.,3.,4.,1.]);
        for q in &mid[..2]{assert!((q.iter().map(|v|v*v).sum::<f32>()-1.).abs()<1e-5);}
    }
    #[test]fn reject_invalid_data(){let a=[[0.,0.,0.,1.];3];let mut b=a;b[2][0]=f32::NAN;assert!(evaluate(&a,&b,0.5).is_none());}
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

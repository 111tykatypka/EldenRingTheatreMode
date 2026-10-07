//! Pure interpolation of the known recorded physics transform. No game memory access.
pub type Transform=[[f32;4];3];
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
}

/// Recorded hkQsTransform: translation vec4, XYZW quaternion, scale vec4.
/// Preserve padding and exact endpoints. Interpolate known semantic components only.
pub fn pose_into(a:&[u8],b:&[u8],t:f64,out:&mut [u8])->Option<()> {
    if a.len()!=b.len()||a.len()!=out.len()||a.len()%48!=0||!t.is_finite(){return None;}
    let t=t.clamp(0.0,1.0);
    if t==0.0{out.copy_from_slice(a);return Some(());}
    if t==1.0{out.copy_from_slice(b);return Some(());}
    for ((a,b),out) in a.chunks_exact(48).zip(b.chunks_exact(48)).zip(out.chunks_exact_mut(48)) {
        let decode=|v:&[u8]|std::array::from_fn::<_,12,_>(|i|f32::from_le_bytes(v[i*4..i*4+4].try_into().unwrap()));
        let a=decode(a);let b=decode(b);let mut result=a;
        let q=quaternion(a[4..8].try_into().ok()?,b[4..8].try_into().ok()?,t)?;
        result[4..8].copy_from_slice(&q);
        for i in [0,1,2,8,9,10]{
            if !a[i].is_finite()||!b[i].is_finite(){return None;}
            result[i]=(a[i]as f64+(b[i]as f64-a[i]as f64)*t)as f32;
        }
        for(i,v)in result.iter().enumerate(){out[i*4..i*4+4].copy_from_slice(&v.to_le_bytes());}
    }
    Some(())
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
    #[test]fn invalid_pose_rejected(){let a=vec![0;48];let mut out=vec![0;48];assert!(pose_into(&a,&a,0.5,&mut out).is_none());assert!(pose_into(&a,&a,f64::NAN,&mut out).is_none());assert!(pose_into(&a[..47],&a[..47],0.5,&mut out[..47]).is_none());}
}

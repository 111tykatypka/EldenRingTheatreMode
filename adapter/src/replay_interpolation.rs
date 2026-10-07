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

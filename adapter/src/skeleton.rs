//! Pointer-free skeleton identity and actual parent-index capture from the evaluated importer.
use crate::{companions,game_profile as p};
#[derive(Clone,Debug,PartialEq,Eq)]
pub struct Definition{pub actor_id:u32,pub model_id:u32,pub parents:Vec<i16>,pub fingerprint:u32}
pub fn valid_parents(parents:&[i16])->bool{
 if parents.is_empty()||parents.len()>4096{return false;}
 // Three-colour walk is linear even for a long parent chain; no recursion on game callbacks.
 let mut colors=vec![0u8;parents.len()];let mut path=Vec::new();
 for i in 0..parents.len(){if colors[i]==2{continue;}path.clear();let mut at=i;
  loop{if colors[at]==1{return false;}if colors[at]==2{break;}
   colors[at]=1;path.push(at);let parent=parents[at];if parent==-1{break;}
   if parent<0||parent as usize>=parents.len(){return false;}at=parent as usize;
  }for &at in &path{colors[at]=2;}
 }true
}
pub fn fingerprint(model:u32,parents:&[i16])->u32{
 let mut bytes=model.to_le_bytes().to_vec();bytes.extend((parents.len() as u32).to_le_bytes());
 for x in parents{bytes.extend(x.to_le_bytes());}crate::codec::crc32(&bytes)
}
pub fn read(chr:usize,actor_id:u32)->Option<Definition>{
 let n=crate::actors::bone_count(chr)?;
 let importer=companions::word(chr.checked_add(p::OFF_CHRINS_POSE_IMPORTER)?)?;
 let skeleton=companions::word(importer.checked_add(p::OFF_POSE_IMPORTER_SKELETON)?)?;
 let pointer=companions::word(skeleton.checked_add(p::OFF_HKA_SKELETON_PARENTS)?)?;
 let mut bytes=vec![0;n.checked_mul(2)?];if !companions::copy(pointer,&mut bytes){return None;}
 let parents:Vec<i16>=bytes.chunks_exact(2).map(|b|i16::from_le_bytes(b.try_into().unwrap())).collect();
 if !valid_parents(&parents){return None;}
 let model_id=companions::dword(unsafe{&raw const (*(chr as *const eldenring::cs::ChrIns)).character_id as usize})?;
 Some(Definition{actor_id,model_id,fingerprint:fingerprint(model_id,&parents),parents})
}
#[cfg(test)]mod tests{
 use super::*;
 #[test]fn hierarchy_rejects_cycles_and_invalid_indices(){assert!(valid_parents(&[-1,0,1,0]));for bad in [vec![],vec![0],vec![1,0],vec![-2],vec![-1,7]]{assert!(!valid_parents(&bad));}}
 #[test]fn identity_includes_model_and_every_parent(){assert_ne!(fingerprint(1,&[-1,0]),fingerprint(2,&[-1,0]));assert_ne!(fingerprint(1,&[-1,0]),fingerprint(1,&[-1,-1]));}
}

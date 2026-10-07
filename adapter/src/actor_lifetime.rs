//! Recorded observations, not engine lifecycle commands. No engine pointers or writes.
//! An enumeration gap is UNKNOWN, never proof of native despawn. Death uses the SDK flag,
//! not HP. Dying/corpse/phase states remain unavailable until their native sources are proven.
use std::collections::HashMap;

pub const KNOWN_BODY:u32=1;
pub const KNOWN_FLAGS:u32=2;
pub const KNOWN_HP:u32=4;
pub const KNOWN_POSE:u32=8;
pub const DEAD:u32=1;
pub const RENDER_ENABLED:u32=2;
pub const RECORD_BYTES:usize=56;
#[derive(Clone,Copy,Debug,Default,PartialEq,Eq)]
pub struct Observation{
 pub time:u64,pub id:u32,pub character_id:u32,pub npc_id:i32,pub model_id:u32,
 pub known:u32,pub flags:u32,pub backread:u32,pub cleanup:u32,pub hp:i32,pub max_hp:i32,
 /// 0 = observed in range; 1 = not enumerated/in range. NOT native Despawn.
 pub availability:u32,
 /// 0 = no gap; 1 = enumeration/radius gap; 2 = invalid/unavailable pose; 3 = topology change.
 pub reason:u32,
}
impl Observation{
 pub fn same_state(&self,other:&Self)->bool{let mut a=*self;let mut b=*other;a.time=0;b.time=0;a==b}
 pub fn unobserved(time:u64,id:u32)->Self{Self{time,id,availability:1,reason:1,..Default::default()}}
 pub fn valid(&self)->bool{self.id!=0&&self.known&!15==0&&self.flags&!3==0&&self.availability<=1&&self.reason<=3
  &&(self.availability==0||self.known==0)}
}
#[derive(Clone,Copy,Debug,PartialEq,Eq)]
pub enum State{Unknown,ObservedAlive,DeathFlagged}
pub fn state(o:&Observation)->State{
 if o.availability!=0||o.known&(KNOWN_BODY|KNOWN_FLAGS)!=(KNOWN_BODY|KNOWN_FLAGS){State::Unknown}
 else if o.flags&DEAD!=0{State::DeathFlagged}else{State::ObservedAlive}
}
/// Binary search in actor-local snapshots/events. Backward seek never mutates a cursor.
#[derive(Default)]
pub struct Timeline{pub tracks:HashMap<u32,Vec<Observation>>}
impl Timeline{
 pub fn from_records(records:Vec<Observation>)->Result<Self,String>{
  let mut out=Self::default();for r in records{if !r.valid(){return Err("invalid actor observation".into());}
   let track=out.tracks.entry(r.id).or_default();if track.last().is_some_and(|p|p.time>=r.time){return Err("actor observation times must increase".into());}track.push(r);}
  Ok(out)
 }
 pub fn at(&self,id:u32,t:u64)->Option<&Observation>{let v=self.tracks.get(&id)?;
  let i=v.partition_point(|o|o.time<=t);if i==0{None}else{v.get(i-1)}}
}
pub fn encode(records:&[Observation])->Vec<u8>{
 let mut out=Vec::with_capacity(records.len()*RECORD_BYTES);for r in records{out.extend(r.time.to_le_bytes());
  for word in [r.id,r.character_id,r.npc_id as u32,r.model_id,r.known,r.flags,r.backread,r.cleanup,r.hp as u32,r.max_hp as u32,r.availability,r.reason]{out.extend(word.to_le_bytes());}}
 out
}
pub fn decode(raw:&[u8],count:usize)->Option<Vec<Observation>>{
 if count.checked_mul(RECORD_BYTES)?!=raw.len(){return None;}
 raw.chunks_exact(RECORD_BYTES).map(|b|{let word=|i:usize|u32::from_le_bytes(b[8+i*4..12+i*4].try_into().unwrap());
  let r=Observation{time:u64::from_le_bytes(b[..8].try_into().unwrap()),id:word(0),character_id:word(1),npc_id:word(2)as i32,model_id:word(3),known:word(4),flags:word(5),backread:word(6),cleanup:word(7),hp:word(8)as i32,max_hp:word(9)as i32,availability:word(10),reason:word(11)};
  r.valid().then_some(r)}).collect()
}
#[cfg(test)]mod tests{
 use super::*;
 fn obs(t:u64,flags:u32)->Observation{Observation{time:t,id:1,known:KNOWN_BODY|KNOWN_FLAGS|KNOWN_HP,flags,hp:0,..Default::default()}}
 #[test]fn death_is_not_inferred_from_zero_hp(){assert_eq!(state(&obs(1,0)),State::ObservedAlive);assert_eq!(state(&obs(2,DEAD)),State::DeathFlagged);}
 #[test]fn backward_seek_restores_recorded_state_without_live_world(){let t=Timeline::from_records(vec![obs(1,0),obs(12,DEAD),Observation::unobserved(15,1)]).unwrap();
  assert!(t.at(1,0).is_none());assert_eq!(state(t.at(1,13).unwrap()),State::DeathFlagged);assert_eq!(state(t.at(1,5).unwrap()),State::ObservedAlive);assert_eq!(state(t.at(1,16).unwrap()),State::Unknown);}
 #[test]fn missing_is_not_despawn(){assert_eq!(state(&Observation::unobserved(9,1)),State::Unknown);}
 #[test]fn codec_rejects_bad_size_and_fields(){let x=vec![obs(1,DEAD),Observation::unobserved(2,1)];assert_eq!(decode(&encode(&x),2),Some(x));assert!(decode(&[0;55],1).is_none());let mut bad=obs(1,0);bad.id=0;assert!(decode(&encode(&[bad]),1).is_none());}
 #[test]fn ordering_is_per_actor(){let mut other=obs(1,0);other.id=2;assert!(Timeline::from_records(vec![obs(2,0),other,obs(3,DEAD)]).is_ok());assert!(Timeline::from_records(vec![obs(2,0),obs(2,DEAD)]).is_err());}
}

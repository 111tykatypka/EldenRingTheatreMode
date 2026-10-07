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
 /// HP known, a real maximum, and none left.
 pub fn hp_depleted(&self)->bool{self.known&KNOWN_HP!=0&&self.max_hp>0&&self.hp<=0}
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

// ---------------------------------------------------------------------------------------------
// Derived lifetime (NPC lifecycle pass). Everything below is computed from the recording alone: the
// live game, the save and the current world never decide whether a recorded actor exists at T.
// ---------------------------------------------------------------------------------------------
/// Where a recorded actor is in its own life at replay time T.
#[derive(Clone,Copy,Debug,PartialEq,Eq)]
pub enum Existence{
 /// Before the first observation of this actor.
 NotYet,
 /// Observed and not death-flagged.
 Alive,
 /// Observed with the SDK death flag set (death animation, ragdoll or corpse; the flag cannot tell them apart).
 Dead,
 /// Stopped being observed after having been death-flagged: the corpse went away (despawn is probable).
 Gone,
 /// Stopped being observed while alive: it left the recording radius or was unloaded. NOT proof of despawn.
 Left,
 /// Observed but flag state not known at T.
 Unknown,
}
/// Lifetime events found in the observations of one actor. Times are source-clock ns.
#[derive(Clone,Copy,Debug,PartialEq,Eq)]
pub struct Derived{pub first_seen:u64,pub death:Option<u64>,pub vanished:Option<u64>,pub last_observed:u64}
impl Timeline{
 /// Spawn (first observation), death (first death-flagged observation) and vanish (the terminal
 /// "not observed" record, if the recording ends with one) of an actor.
 pub fn derive(&self,id:u32)->Option<Derived>{
  let v=self.tracks.get(&id)?;let first=v.iter().find(|o|o.availability==0)?;
  // Death is the earlier of the SDK death flag and HP running out. Field evidence (2026-10-07 recording):
  // an enemy reached HP 0 while the death flag stayed false, so the flag alone misses real deaths.
  // HP 0 only counts when the actor has a maximum HP, so invincible or HP-less NPCs are never "dead".
  let death=v.iter().find(|o|o.availability==0&&(state(o)==State::DeathFlagged||o.hp_depleted())).map(|o|o.time);
  let last_observed=v.iter().rev().find(|o|o.availability==0)?.time;
  let vanished=v.iter().rev().find(|o|o.availability!=0&&o.time>last_observed).map(|o|o.time);
  Some(Derived{first_seen:first.time,death,vanished,last_observed})}
 /// The only function playback consults to decide existence; independent of seek direction.
 pub fn existence(&self,id:u32,t:u64)->Existence{
  let Some(d)=self.derive(id) else {return Existence::Unknown};
  if t<d.first_seen{return Existence::NotYet;}
  if let Some(v)=d.vanished{if t>=v{return if d.death.is_some_and(|x|x<=v){Existence::Gone}else{Existence::Left};}}
  // Death is monotonic in a recording: from its first signal on, the actor is dead until it is gone.
  if d.death.is_some_and(|x|t>=x){Existence::Dead}else{Existence::Alive}}
}
#[cfg(test)]mod lifecycle_tests{
 use super::*;
 fn hp(t:u64,hp:i32)->Observation{Observation{time:t*1_000_000_000,id:1,known:KNOWN_BODY|KNOWN_FLAGS|KNOWN_HP|KNOWN_POSE,flags:RENDER_ENABLED,hp,max_hp:100,..Default::default()}}
 #[test]fn hp_running_out_is_death_even_when_the_flag_never_sets(){
  // Real recording: hp reached 0 with death_flag=false. Seek back must still resurrect.
  let t=Timeline::from_records(vec![hp(0,100),hp(5,60),hp(10,0),hp(14,0)]).unwrap();
  assert_eq!(t.existence(1,7*1_000_000_000),Existence::Alive);assert_eq!(t.existence(1,12*1_000_000_000),Existence::Dead);assert_eq!(t.existence(1,3*1_000_000_000),Existence::Alive);}
 #[test]fn actors_without_max_hp_are_never_dead(){let mut o=hp(0,0);o.max_hp=0;let t=Timeline::from_records(vec![o]).unwrap();assert_eq!(t.existence(1,0),Existence::Alive);}
 const S:u64=1_000_000_000;
 fn o(t:u64,flags:u32)->Observation{Observation{time:t*S,id:1,known:KNOWN_BODY|KNOWN_FLAGS|KNOWN_HP|KNOWN_POSE,flags:flags|RENDER_ENABLED,hp:if flags&DEAD!=0{0}else{100},max_hp:100,..Default::default()}}
 /// The owner's scenario: alive at 0, hit at 9, dies at 12, corpse, gone at 30.
 fn enemy()->Timeline{Timeline::from_records(vec![o(0,0),o(3,0),o(6,0),o(9,0),o(12,DEAD),o(15,DEAD),o(24,DEAD),Observation::unobserved(30*S,1)]).unwrap()}
 #[test]fn derives_spawn_death_and_vanish(){let d=enemy().derive(1).unwrap();assert_eq!((d.first_seen,d.death,d.vanished,d.last_observed),(0,Some(12*S),Some(30*S),24*S));}
 #[test]fn existence_follows_the_recording(){let t=enemy();
  assert_eq!(t.existence(1,5*S),Existence::Alive);assert_eq!(t.existence(1,11*S+S/2),Existence::Alive);
  assert_eq!(t.existence(1,13*S),Existence::Dead);assert_eq!(t.existence(1,22*S),Existence::Dead);
  assert_eq!(t.existence(1,31*S),Existence::Gone);assert_eq!(t.existence(2,5*S),Existence::Unknown);}
 #[test]fn backward_seek_resurrects_in_the_data(){let t=enemy();
  // 22 s (dead) -> 8 s: alive again, then forward to 13 s: dead again, repeatedly, without any cursor.
  for _ in 0..3{assert_eq!(t.existence(1,22*S),Existence::Dead);assert_eq!(t.existence(1,8*S),Existence::Alive);assert_eq!(t.existence(1,13*S),Existence::Dead);}}
 #[test]fn actor_that_appears_later_is_not_there_before(){let t=Timeline::from_records(vec![o(12,0),o(15,0)]).unwrap();assert_eq!(t.existence(1,5*S),Existence::NotYet);assert_eq!(t.existence(1,13*S),Existence::Alive);}
 #[test]fn leaving_the_radius_alive_is_not_despawn(){let t=Timeline::from_records(vec![o(0,0),o(5,0),Observation::unobserved(8*S,1)]).unwrap();assert_eq!(t.existence(1,6*S),Existence::Alive);assert_eq!(t.existence(1,9*S),Existence::Left);}
 #[test]fn unobserved_before_the_recording_ends_with_death_means_gone(){let t=Timeline::from_records(vec![o(0,0),o(4,DEAD),Observation::unobserved(7*S,1)]).unwrap();assert_eq!(t.existence(1,8*S),Existence::Gone);}
}

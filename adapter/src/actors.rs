//! Enemies, NPCs and bosses in replays (Phase 2.2), with the same bone-pose method as the player.
//!
//! Recording (game thread, Draw_Pre, while the host records): every character within the recording
//! radius (GameProfile TM_VAL_ACTOR_RADIUS, default 100 m) gets a stable recording id (never a
//! pointer) the first time it is seen, with its handle, map entity id, NpcParam id and type. Each
//! frame it records root transform, origin/chunk anchor metadata, HP and
//! its full skeleton pose (local + model space; bone count read from its own hkaSkeleton).
//!
//! Playback, approach (a) "take over the real characters" (owner asked to confirm; recommended):
//! while the replay owns the player, each recorded actor is matched to the live character with the
//! same map entity id and NpcParam (or the same handle). Its AI is held with the game's own debug
//! flags (no move, no attack, written only after a structural check of the flag word), gravity is
//! turned off, and its bones and root are written exactly like the player's, interpolated, in
//! today's physics space. When the replay releases the body, every actor gets its flags and its own
//! position back.
//! Limits (reported in the summary): a character that no longer exists (e.g. a defeated boss) cannot
//! be shown this way, and characters that spawn later or died earlier are not hidden yet. Both need
//! spawned puppets, approach (b).
use crate::game_profile as profile;
use crate::world_file::{ActorFrame,ActorInfo,ActorTrack,EntityContext,Message,PlayerFrame};
use eldenring::cs::{ChrIns,CSChrPhysicsModule,WorldChrMan};
use fromsoftware_shared::FromStatic;
use std::collections::{HashMap,HashSet};
use std::sync::mpsc::SyncSender;

const QS:usize=48;
fn recording_radius()->f32{
 static RADIUS:std::sync::OnceLock<f32>=std::sync::OnceLock::new();
 *RADIUS.get_or_init(||std::env::var("THEATER_ACTOR_RADIUS").ok().and_then(|v|v.parse::<f32>().ok()).filter(|v|v.is_finite()&&*v>=0.0).unwrap_or(profile::VAL_ACTOR_RADIUS as f32))
}
fn read_ptr(a:usize)->usize{crate::companions::word(a).unwrap_or(0)}
fn read_i32(a:usize)->i32{crate::companions::dword(a).map(|v|v as i32).unwrap_or(0)}
fn object(p:usize)->bool{(0x10000000000..0x800000000000).contains(&p)&&p%8==0}
fn array(p:usize)->bool{object(p)&&p%16==0}

/// Bone count of a character's skeleton, only when three independent counts agree.
pub fn bone_count(chr:usize)->Option<usize>{
 let importer=read_ptr(chr+profile::OFF_CHRINS_POSE_IMPORTER);if !object(importer){return None;}
 let skel=read_ptr(importer+profile::OFF_POSE_IMPORTER_SKELETON);if !object(skel){return None;}
 let (a,b,c)=(read_i32(skel+profile::OFF_HKA_SKELETON_PARENT_COUNT),read_i32(skel+profile::OFF_HKA_SKELETON_BONE_COUNT),read_i32(skel+profile::OFF_HKA_SKELETON_REFPOSE_COUNT));
 (a==b&&b==c&&(1..=1024).contains(&a)).then_some(a as usize)}
fn pose_arrays(chr:usize)->Option<(usize,usize)>{
 let importer=read_ptr(chr+profile::OFF_CHRINS_POSE_IMPORTER);if !object(importer){return None;}
 let (l,m)=(read_ptr(importer+profile::OFF_POSE_IMPORTER_LOCAL),read_ptr(importer+profile::OFF_POSE_IMPORTER_MODEL));(array(l)&&array(m)).then_some((l,m))}
fn physics(chr:usize)->Option<usize>{let p=unsafe{&*(chr as *const ChrIns)}.modules.physics.as_ref() as *const CSChrPhysicsModule as usize;object(p).then_some(p)}
fn transform_fields(p:usize)->[usize;3]{let m=p as *const CSChrPhysicsModule;unsafe{[&raw const (*m).orientation as usize,&raw const (*m).interpolated_orientation as usize,&raw const (*m).position as usize]}}
type Transform=[[f32;4];3];
fn read_transform(chr:usize)->Option<Transform>{let p=physics(chr)?;Some(transform_fields(p).map(|a|unsafe{std::ptr::read_volatile(a as *const [f32;4])}))}
fn write_transform(chr:usize,t:&Transform){if let Some(p)=physics(chr){for(a,v)in transform_fields(p).iter().zip(t){unsafe{std::ptr::write_volatile(*a as *mut [f32;4],*v)}}}}
fn bool_flag(chr:usize,pick:fn(*const CSChrPhysicsModule)->usize)->Option<usize>{let p=physics(chr)?;let a=pick(p as *const CSChrPhysicsModule);(unsafe{std::ptr::read_volatile(a as *const u8)}<=1).then_some(a)}
fn gravity_flag(chr:usize)->Option<usize>{bool_flag(chr,|m|unsafe{&raw const (*m).gravity_disabled as usize})}
fn proxy_flag(chr:usize)->Option<usize>{bool_flag(chr,|m|unsafe{&raw const (*m).chr_proxy_pos_update_requested as usize})}
fn set_flag(a:Option<usize>,on:bool){if let Some(a)=a{unsafe{std::ptr::write_volatile(a as *mut u8,on as u8)}}}
/// The ChrIns debug flag word, only when the word before it holds the expected callback.
fn debug_flags(chr:usize)->Option<usize>{
 let base=unsafe{crate::GetModuleHandleW(std::ptr::null())} as usize;
 (base!=0&&read_ptr(chr+profile::OFF_CHRINS_DEBUG_CALLBACK)==base+profile::VAL_CHRINS_DEBUG_CALLBACK_RVA).then_some(chr+profile::OFF_CHRINS_DEBUG_FLAGS)}
fn handle_of(c:&ChrIns)->u64{c.field_ins_handle.selector.0 as u64|((i32::from(c.field_ins_handle.block_id) as u32 as u64)<<32)}
fn live_snapshot(diagnostic:bool)->(Vec<usize>,HashSet<usize>){
 let Ok(world)=(unsafe{WorldChrMan::instance()}) else {return (Vec::new(),HashSet::new())};
 let player=world.main_player.as_ref().map(|p|&p.chr_ins as *const _ as usize);
 let buddies=match crate::companions::buddies(world,diagnostic){Ok(v)=>v,Err(e)=>{
  static WARN:std::sync::Once=std::sync::Once::new();WARN.call_once(||{let msg=format!("COMPANIONS_UNAVAILABLE: {e}; distance-list actors still recorded");crate::log_game(&msg);crate::bone_replay::status(&msg);});Vec::new()}};
 let set:HashSet<usize>=buddies.iter().copied().collect();
 let mut bodies:Vec<usize>=world.chr_inses_by_distance.iter().map(|e|e.chr_ins.as_ptr() as usize).chain(buddies).filter(|a|Some(*a)!=player).collect();
 bodies.sort_unstable();bodies.dedup();(bodies,set)}
fn live()->Vec<usize>{live_snapshot(false).0}

// ------------------------------------------------------------------------------------------ record
/// Per-recording identities and rate control; lives while the host records.
struct Identity{info:ActorInfo,announced:bool,category:u32,seen:u64,skeleton:Option<crate::skeleton::Definition>}
pub struct Recorder{ids:HashMap<(u64,u32,i32),Identity>,next:u32,frame:u64,warned:bool,next_context:u64,dropped:u64,companion_ids:HashSet<u32>,next_diagnostic:u64}
impl Recorder{
 pub fn new()->Self{Self{ids:HashMap::new(),next:1,frame:0,warned:false,next_context:0,dropped:0,companion_ids:HashSet::new(),next_diagnostic:0}}
 /// Samples nearby characters; `player` is the main player's ChrIns, `tx` the world file writer.
 pub fn sample(&mut self,now:u64,player:usize,tx:&SyncSender<Message>){
  self.frame+=1;
  let me=unsafe{&*(player as *const ChrIns)};let origin=me.modules.physics.position;
  let radius=recording_radius(); // 0 means every discovered loaded body; no forced far-actor decimation.
  let mut batch=Vec::new();let mut infos=Vec::new();let mut context=Vec::new();let mut accepted=Vec::new();let mut skeletons=Vec::new();
  let snapshot=now>=self.next_context;let diagnostic=now>=self.next_diagnostic;
  if diagnostic{self.next_diagnostic=now.saturating_add(1_000_000_000);}
  let (bodies,buddies)=live_snapshot(diagnostic);let mut body_ids=HashMap::new();
  for chr in bodies{
   let c=unsafe{&*(chr as *const ChrIns)};if c.field_ins_handle.is_empty(){continue;}
   // SDK documents character_id/npc_id 8000 as Torrent. Diagnose before pose/range filters;
   // a model identifier alone does not authorize playback ownership or classify a track.
   if diagnostic&&(buddies.contains(&chr)||c.character_id==8000||c.npc_id==8000){
    let importer=read_ptr(chr+profile::OFF_CHRINS_POSE_IMPORTER);
    let skel=if object(importer){read_ptr(importer+profile::OFF_POSE_IMPORTER_SKELETON)}else{0};
    let counts=if object(skel){Some([read_i32(skel+profile::OFF_HKA_SKELETON_PARENT_COUNT),read_i32(skel+profile::OFF_HKA_SKELETON_BONE_COUNT),read_i32(skel+profile::OFF_HKA_SKELETON_REFPOSE_COUNT)])}else{None};
    crate::log_game(&format!("COMPANION_CANDIDATE: handle=0x{:X} character_id={} npc_id={} npc_param={} buddy={} ride={:?} importer=0x{importer:X} skeleton=0x{skel:X} skeleton_counts={counts:?} pose_arrays={} transform={}",handle_of(c),c.character_id,c.npc_id,c.npc_param_id,buddies.contains(&chr),crate::companions::ride(chr),pose_arrays(chr).is_some(),read_transform(chr).is_some()));
   }
   let p=c.modules.physics.position;let d=((p.0-origin.0).powi(2)+(p.1-origin.1).powi(2)+(p.2-origin.2).powi(2)).sqrt();
   if !d.is_finite()||(radius>0.0&&d>radius){continue;}
   let key=(handle_of(c),c.event_entity_id,c.npc_param_id);
   // Reappearance after an observation gap gets a new recording identity, even if a handle was reused.
   if self.ids.get(&key).is_some_and(|i|now.saturating_sub(i.seen)>500_000_000){self.ids.remove(&key);}
   if !self.ids.contains_key(&key){let Some(next)=self.next.checked_add(1) else {crate::log_game("ACTOR_ERROR: recording identity space exhausted");continue;};
    self.ids.insert(key,Identity{info:ActorInfo{id:self.next,handle:key.0,entity:key.1,npc_param:key.2,chr_type:c.chr_type as u32,first_seen:now},announced:false,category:0,seen:now,skeleton:None});self.next=next;}
   let identity=self.ids.get_mut(&key).unwrap();identity.seen=now;let id=identity.info.id;
   let (Some(n),Some((local,model)),Some(transform))=(bone_count(chr),pose_arrays(chr),read_transform(chr)) else {
    if !self.warned{self.warned=true;crate::log_game("ACTORS: a character's skeleton could not be read (counts disagree); it is skipped");}continue};
   let pose=|a:usize|unsafe{std::slice::from_raw_parts(a as *const u8,n*QS)}.to_vec();
   let ride=snapshot.then(||crate::companions::ride(chr)).flatten();
   if snapshot||!identity.announced{identity.category=crate::companions::category(chr,buddies.contains(&chr),ride);}
   let Some(definition)=crate::skeleton::read(chr,id) else{continue;};
   if identity.skeleton.as_ref().is_some_and(|d|d!=&definition){
    if !self.warned{self.warned=true;crate::log_game("ACTOR_CAPTURE_UNAVAILABLE: skeleton changed; incompatible pose samples skipped");}continue;
   }
   if !identity.announced{infos.push(identity.info);accepted.push(key);identity.skeleton=Some(definition.clone());skeletons.push(definition);}
   body_ids.insert(key.0,id);
   if snapshot||!identity.announced{let r=ride.unwrap_or_default();context.push(EntityContext{time:now,id,category:identity.category,ride_flags:r.flags,ride_state:r.state,ride_param:r.param,mount_id:0});}
   let data=&c.modules.data;
   batch.push((id,ActorFrame{body:PlayerFrame{time:now,transform,matrix:[0.0;16],local:pose(local),model:pose(model),place:crate::arrival::place(chr),equip:Default::default()},hp:data.hp,max_hp:data.max_hp}));
  }
  if snapshot{let r=crate::companions::ride(player).unwrap_or_default();
   // Current pair-node handle, not last_mounted's stale pointer; only resolve an observed live body.
   let mount_id=if r.flags&crate::companions::MOUNTED!=0{body_ids.get(&r.counter_party).copied().unwrap_or(0)}else{0};
   context.push(EntityContext{time:now,id:0,category:0,ride_flags:r.flags,ride_state:r.state,ride_param:r.param,mount_id});}
  let count=batch.len() as u64;
  let companion_context:Vec<_>=context.iter().filter(|c|c.id!=0&&c.category!=0).copied().collect();
  if !batch.is_empty()||!context.is_empty(){match tx.try_send(Message::ActorBatch{infos,frames:batch,context,skeletons}){
   Ok(())=>{for key in accepted{if let Some(i)=self.ids.get_mut(&key){i.announced=true;}}
    if snapshot{self.next_context=now+250_000_000;}
    for c in companion_context{if self.companion_ids.insert(c.id){let msg=format!("COMPANION_RECORDED: id={} category=0x{:X} ride_flags=0x{:X} state={} param={} (buddy-set/mount/summon evidence, not a guessed model id)",c.id,c.category,c.ride_flags,c.ride_state,c.ride_param);crate::log_game(&msg);crate::bone_replay::status(&msg);}}}
   Err(_)=>{self.dropped+=count;}}}}
 pub fn count(&self)->usize{(self.next-1) as usize}
 pub fn drops(&self)->u64{self.dropped}
 pub fn companions(&self)->usize{self.companion_ids.len()}
}

// ---------------------------------------------------------------------------------------- playback
struct Controlled{chr:usize,handle:u64,flags:Option<u32>,gravity:Option<bool>,transform:Transform}
pub struct Player{tracks:Vec<(ActorInfo,ActorTrack,Vec<i16>)>,controlled:HashMap<u32,Controlled>,next_match:u64,local:Vec<u8>,model:Vec<u8>,logged:bool,categories:HashMap<u32,u32>,warned:HashSet<u32>,skeletons:HashMap<u32,crate::skeleton::Definition>}
impl Player{
 pub fn new(actors:Vec<(ActorInfo,ActorTrack)>,context:&[EntityContext],skeletons:&HashMap<u32,crate::skeleton::Definition>)->Self{
  let categories=context.iter().filter(|c|c.id!=0).fold(HashMap::<u32,u32>::new(),|mut m,c|{*m.entry(c.id).or_default()|=c.category;m});
  let tracks=actors.into_iter().map(|(info,mut t)|{
   let n=t.len();let picks:Vec<PlayerFrame>=(0..3).filter_map(|k|t.get(k*(n.saturating_sub(1))/2).map(|f|f.body.clone())).collect();
   let parents=skeletons.get(&info.id).map(|d|d.parents.clone()).unwrap_or_else(||crate::replay_interpolation::learn_parents(picks.iter().map(|f|(&f.local[..],&f.model[..]))));(info,t,parents)}).collect();
  Self{tracks,controlled:HashMap::new(),next_match:0,local:Vec::new(),model:Vec::new(),logged:false,categories,warned:HashSet::new(),skeletons:skeletons.clone()}}
 pub fn len(&self)->usize{self.tracks.len()}
 fn find(info:&ActorInfo,taken:&[usize],category:u32,unique_recorded:bool)->Option<usize>{
  let (bodies,buddies)=live_snapshot(false);let mut candidates=Vec::new();
  for a in bodies.into_iter().filter(|a|!taken.contains(a)){let c=unsafe{&*(a as *const ChrIns)};if c.npc_param_id!=info.npc_param{continue;}
   if (info.entity!=0&&c.event_entity_id==info.entity)||(info.entity==0&&handle_of(c)==info.handle){return Some(a);}
   if info.entity==0&&unique_recorded{let observed=crate::companions::category(a,buddies.contains(&a),crate::companions::ride(a));
    if role_matches(category,observed){candidates.push(a);}}}
  // No ordinal guesses for identical spirit ashes: a different handle is accepted only uniquely.
  (candidates.len()==1).then(||candidates[0])}
 /// Writes existing bodies at master replay time, in their verified original coordinate frame.
 pub fn write(&mut self,t:u64,now:u64,_offset:[f32;3],interpolate:bool){
  let alive=live();
  if now>=self.next_match{self.next_match=now+500_000_000;
   let mut taken:Vec<usize>=self.controlled.values().map(|c|c.chr).collect();let mut newly=0;
   for (info,track,_) in &self.tracks{if self.controlled.contains_key(&info.id)||!active_at(&track.times,t){continue;}
    let unique_recorded=self.tracks.iter().filter(|(other,_,_)|other.npc_param==info.npc_param).count()==1;
    if let Some(chr)=Self::find(info,&taken,self.categories.get(&info.id).copied().unwrap_or(0),unique_recorded){let c=unsafe{&*(chr as *const ChrIns)};
     let Some(transform)=read_transform(chr) else {continue};
     let flags=debug_flags(chr).map(|a|unsafe{std::ptr::read_volatile(a as *const u32)});
     let gravity=gravity_flag(chr).map(|a|unsafe{std::ptr::read_volatile(a as *const u8)}==1);
     if flags.is_none()||gravity.is_none(){if self.warned.insert(info.id){crate::log_game(&format!("ACTOR_UNAVAILABLE: id={} control flags/gravity could not be validated; no pose/root writes",info.id));}continue;}
     self.controlled.insert(info.id,Controlled{chr,handle:handle_of(c),flags,gravity,transform});taken.push(chr);newly+=1;}}
   if newly>0||!self.logged{self.logged=true;crate::log_game(&format!("ACTORS: {} of {} recorded characters found in the world and held by the replay",self.controlled.len(),self.tracks.len()));}}
  for (info,track,parents) in &mut self.tracks{
   let Some(ctl)=self.controlled.get(&info.id) else {continue};let chr=ctl.chr;
   // The character must still be the same one (a reload can reuse the address).
   if !alive.contains(&chr)||handle_of(unsafe{&*(chr as *const ChrIns)})!=ctl.handle{self.controlled.remove(&info.id);continue;}
   if debug_flags(chr).is_none(){if let Some(c)=self.controlled.remove(&info.id){restore(&c);}continue;}
   if self.skeletons.get(&info.id).is_some_and(|d|crate::skeleton::read(chr,info.id).as_ref()!=Some(d)){
    if let Some(c)=self.controlled.remove(&info.id){restore(&c);}
    if self.warned.insert(info.id){crate::log_game(&format!("ACTOR_UNAVAILABLE: id={} skeleton identity differs; no pose applied",info.id));}continue;
   }
   let n=track.len();if !active_at(&track.times,t){
    if let Some(c)=self.controlled.remove(&info.id){restore(&c);}continue;}
   let i=track.times.partition_point(|x|*x<=t).saturating_sub(1).min(n-1);
   let (Some(a),Some(b))=(track.get(i).cloned(),track.get((i+1).min(n-1)).cloned()) else {continue};
   let span=b.body.time.saturating_sub(a.body.time);
   let jump=(0..3).map(|k|(b.body.transform[2][k]-a.body.transform[2][k]).powi(2)).sum::<f32>().sqrt();
   let alpha=if !interpolate||span==0||span>500_000_000||jump>1.5{0.0}else{(t-a.body.time) as f64/span as f64};
   if ![&a.body.local,&a.body.model,&b.body.local,&b.body.model].into_iter().all(|v|crate::replay_interpolation::pose_safe_to_apply(v)){
    if let Some(c)=self.controlled.remove(&info.id){restore(&c);}
    if self.warned.insert(info.id){crate::log_game(&format!("ACTOR_UNAVAILABLE: id={} invalid pose coordinates/quaternion; released",info.id));}continue;
   }
   let Some(root)=crate::replay_interpolation::evaluate(&a.body.transform,&b.body.transform,alpha) else {
    if let Some(c)=self.controlled.remove(&info.id){restore(&c);}continue;
   };
   let here=crate::arrival::place(chr);
   if a.body.place.block!=-1&&![a.body.place,b.body.place].iter().all(|p|crate::replay_interpolation::same_root_space(p.origin,p.global,here.origin,here.global)){
    if let Some(c)=self.controlled.remove(&info.id){restore(&c);}
    if self.warned.insert(info.id){crate::log_game(&format!("ACTOR_UNAVAILABLE: id={} coordinate origin differs; no guessed root rebase",info.id));}continue;
   }
   let bones=a.body.local.len()/QS;
   let mut pose_written=false;
   if bone_count(chr)==Some(bones)&&b.body.local.len()==a.body.local.len(){
    if let Some((local,model))=pose_arrays(chr){
     self.local.resize(bones*QS,0);self.model.resize(bones*QS,0);
     if crate::replay_interpolation::pose_into(&a.body.local,&b.body.local,alpha,&mut self.local).is_some()
      &&crate::replay_interpolation::model_from_local(&self.local,parents,&a.body.model,&b.body.model,alpha,&mut self.model).is_some(){
      unsafe{std::ptr::copy_nonoverlapping(self.local.as_ptr(),local as *mut u8,bones*QS);std::ptr::copy_nonoverlapping(self.model.as_ptr(),model as *mut u8,bones*QS);}pose_written=true;}}}
   if !pose_written{if let Some(c)=self.controlled.remove(&info.id){restore(&c);}if self.warned.insert(info.id){crate::log_game(&format!("ACTOR_UNAVAILABLE: id={} pose mismatch; released instead of root-only sliding",info.id));}continue;}
   write_transform(chr,&root);set_flag(proxy_flag(chr),true);set_flag(gravity_flag(chr),true);
   if let (Some(a),Some(_))=(debug_flags(chr),ctl.flags){unsafe{let v=std::ptr::read_volatile(a as *const u32);std::ptr::write_volatile(a as *mut u32,v|(profile::VAL_DEBUG_FLAG_NO_MOVE|profile::VAL_DEBUG_FLAG_NO_ATTACK) as u32);}}
  }}
 /// Gives every held character its flags, gravity and position back.
 pub fn release(&mut self){
  let alive=live();let n=self.controlled.len();
  for (_,c) in self.controlled.drain(){
   if !alive.contains(&c.chr)||handle_of(unsafe{&*(c.chr as *const ChrIns)})!=c.handle{continue;}
   restore(&c);}
  if n>0{crate::log_game(&format!("ACTORS: {n} characters given back to the game"));}}
}

// Observational gaps are not a proven spawn/despawn event; never hold an actor through a long gap.
fn active_at(times:&[u64],t:u64)->bool{
 let Some(first)=times.first() else{return false;};if t<*first||t>*times.last().unwrap(){return false;}
 let i=times.partition_point(|x|*x<=t).saturating_sub(1);
 t.saturating_sub(times[i])<=500_000_000
}
fn role_matches(recorded:u32,observed:u32)->bool{
 let semantic=recorded&(crate::companions::RIDDEN_BODY|crate::companions::NPC_SUMMON|crate::companions::WHITE_PHANTOM);
 semantic!=0&&observed&recorded==recorded
}
fn restore(c:&Controlled){
 if let (Some(a),Some(f))=(debug_flags(c.chr),c.flags){unsafe{let now=std::ptr::read_volatile(a as *const u32);let mask=(profile::VAL_DEBUG_FLAG_NO_MOVE|profile::VAL_DEBUG_FLAG_NO_ATTACK) as u32;std::ptr::write_volatile(a as *mut u32,(now&!mask)|(f&mask));}}
 if let Some(g)=c.gravity{set_flag(gravity_flag(c.chr),g);}write_transform(c.chr,&c.transform);set_flag(proxy_flag(c.chr),true);
}
#[cfg(test)]mod tests{
 use super::*;
 #[test]fn no_hold_outside_lifetime_or_across_gaps(){let t=[10,20,900_000_000];assert!(!active_at(&t,9));assert!(active_at(&t,15));assert!(!active_at(&t,600_000_000));assert!(active_at(&t,900_000_000));assert!(!active_at(&t,900_000_001));assert!(!active_at(&[],10));}
 #[test]fn buddy_membership_is_not_mount_or_summon_identity(){assert!(!role_matches(1,1));assert!(!role_matches(3,1));assert!(role_matches(3,3));assert!(!role_matches(4,8));assert!(role_matches(4,4));}
}

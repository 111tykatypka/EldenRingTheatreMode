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
//! NPC lifecycle pass: the recording, not the live world, decides whether a recorded actor exists at
//! replay time T (actor_lifetime::Timeline::existence). plan() maps that to a representation:
//! LIVE (drive the real body), PUPPET (a stand-in made by the game's own debug character creator,
//! opt-in, experimental) or ABSENT. A live body that is dead or hidden while the recording says
//! "alive" is never revived or edited; a puppet is requested instead, or ACTOR_RECONSTRUCTION_UNAVAILABLE
//! is logged and playback continues. No HP, death, reward or event-flag writes happen anywhere here.
use crate::game_profile as profile;
use crate::actor_lifetime::{self as lifetime,Observation};
use crate::actor_lifetime::Existence;
use crate::world_file::{ActorFrame,ActorInfo,ActorMeta,ActorTrack,EntityContext,Message,PlayerFrame};
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
/// MSVC RTTI class name of an object (vtable[-1] -> complete object locator -> type descriptor).
fn rtti_name(obj:usize)->Option<String>{
 let vt=read_ptr(obj);if !object(vt){return None;}
 let col=read_ptr(vt-8);if !object(col)||crate::companions::dword(col)?!=1{return None;}
 let (type_rva,self_rva)=(crate::companions::dword(col+0xC)? as usize,crate::companions::dword(col+0x14)? as usize);
 let image=col.checked_sub(self_rva)?;let mut b=[0u8;96];if !crate::companions::copy(image+type_rva+0x10,&mut b){return None;}
 let end=b.iter().position(|c|*c==0)?;std::str::from_utf8(&b[..end]).ok().map(str::to_owned)}
/// The NpcThinkParam id, only for EnemyIns bodies (the only class that carries one); -1 otherwise.
fn think_param(chr:usize)->i32{
 if rtti_name(chr).is_some_and(|n|n.starts_with(".?AVEnemyIns@")){crate::companions::dword(chr+std::mem::offset_of!(eldenring::cs::EnemyIns,npc_think_param)).map(|v|v as i32).unwrap_or(-1)}else{-1}}
// Documented ChrIns flag bytes (SDK): 1c5 bit3 enable_render, bit4 is_invincible, bit7 death_flag;
// 1c6 bit0 has_dropped_item, bit1 has_dropped_runes.
fn flag_byte(chr:usize,which:u8)->Option<usize>{let c=chr as *const ChrIns;let a=unsafe{match which{5=>&raw const (*c).chr_flags1c5 as usize,_=>&raw const (*c).chr_flags1c6 as usize}};crate::companions::copy(a,&mut [0u8]).then_some(a)}
fn read_byte(a:usize)->u8{let mut b=[0u8];crate::companions::copy(a,&mut b);b[0]}
fn write_bits(chr:usize,which:u8,set:u8,clear:u8){if let Some(a)=flag_byte(chr,which){unsafe{let v=std::ptr::read_volatile(a as *const u8);std::ptr::write_volatile(a as *mut u8,(v|set)&!clear);}}}
const RENDER:u8=1<<3;const INVINCIBLE:u8=1<<4;const DROPPED_ITEM:u8=1;const DROPPED_RUNES:u8=2;
/// Live death/render state of a body, from the documented flag bytes.
fn live_state(chr:usize)->Option<(bool,bool)>{let a=flag_byte(chr,5)?;let v=read_byte(a);Some((v&(1<<7)!=0,v&RENDER!=0))}
fn set_render(chr:usize,on:bool){if on{write_bits(chr,5,RENDER,0)}else{write_bits(chr,5,0,RENDER)}}
/// What represents a recorded actor at replay time T.
#[derive(Clone,Copy,Debug,PartialEq,Eq)]
pub enum Plan{
 /// Drive the live body (its pose and root are overwritten; AI held).
 Drive,
 /// A stand-in is needed; the text says why the live world cannot provide the actor.
 Puppet(&'static str),
 /// The actor does not exist at T in the recording: nothing may be shown for it.
 Absent,
 /// Cannot be decided or shown safely.
 Unavailable(&'static str)}
/// Pure decision: recorded existence + the live body's (dead, render) state, if a live body was found.
pub fn plan(existence:Existence,live:Option<(bool,bool)>)->Plan{
 match existence{
  Existence::NotYet|Existence::Gone|Existence::Left=>Plan::Absent,
  Existence::Unknown=>Plan::Unavailable("lifetime unknown at this time"),
  Existence::Alive=>match live{Some((false,true))=>Plan::Drive,Some(_)=>Plan::Puppet("live body is dead or hidden but the recording says alive"),None=>Plan::Puppet("no live body in the world")},
  // A corpse pose is only a pose: a live body (alive or dead) can show it; a missing or hidden one cannot.
  Existence::Dead=>match live{Some((_,true))=>Plan::Drive,Some(_)=>Plan::Puppet("live body is hidden or despawned"),None=>Plan::Puppet("no live body in the world")}}}
fn puppets_allowed(options:u32)->bool{options&2!=0||std::env::var("THEATER_PUPPETS").as_deref()==Ok("1")}
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
pub struct Recorder{ids:HashMap<(u64,u32,i32),Identity>,next:u32,frame:u64,warned:bool,next_context:u64,dropped:u64,companion_ids:HashSet<u32>,next_diagnostic:u64,observations:HashMap<u32,Observation>}
impl Recorder{
 pub fn new()->Self{Self{ids:HashMap::new(),next:1,frame:0,warned:false,next_context:0,dropped:0,companion_ids:HashSet::new(),next_diagnostic:0,observations:HashMap::new()}}
 /// Samples nearby characters; `player` is the main player's ChrIns, `tx` the world file writer.
 pub fn sample(&mut self,now:u64,player:usize,tx:&SyncSender<Message>){
  self.frame+=1;
  let me=unsafe{&*(player as *const ChrIns)};let origin=me.modules.physics.position;
  let radius=recording_radius(); // 0 means every discovered loaded body; no forced far-actor decimation.
  let mut batch=Vec::new();let mut infos=Vec::new();let mut context=Vec::new();let mut accepted=Vec::new();let mut skeletons=Vec::new();let mut observed=HashMap::new();let mut metas=Vec::new();
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
   let flags=c.chr_flags1c5;let data=c.modules.data.as_ptr();
   let (hp,max_hp)=if data.is_null(){(None,None)}else{(crate::companions::dword(unsafe{&raw const (*data).hp as usize}),crate::companions::dword(unsafe{&raw const (*data).max_hp as usize}))};
   let hp_known=if hp.is_some()&&max_hp.is_some(){lifetime::KNOWN_HP}else{0};
   observed.insert(id,Observation{time:now,id,character_id:c.character_id,npc_id:c.npc_id,model_id:c.character_id,known:lifetime::KNOWN_BODY|lifetime::KNOWN_FLAGS|hp_known,flags:(flags.death_flag() as u32)*lifetime::DEAD|(flags.enable_render() as u32)*lifetime::RENDER_ENABLED,backread:c.backread_state,cleanup:c.chr_set_cleanup,hp:hp.unwrap_or(0)as i32,max_hp:max_hp.unwrap_or(0)as i32,reason:2,..Default::default()});
   let (Some(n),Some((local,model)),Some(transform))=(bone_count(chr),pose_arrays(chr),read_transform(chr)) else {
    if !self.warned{self.warned=true;crate::log_game("ACTORS: a character's skeleton could not be read (counts disagree); it is skipped");}continue};
   let pose=|a:usize|unsafe{std::slice::from_raw_parts(a as *const u8,n*QS)}.to_vec();
   let ride=snapshot.then(||crate::companions::ride(chr)).flatten();
   if snapshot||!identity.announced{identity.category=crate::companions::category(chr,buddies.contains(&chr),ride);}
   let Some(definition)=crate::skeleton::read(chr,id) else{continue;};
   if identity.skeleton.as_ref().is_some_and(|d|d!=&definition){
    if let Some(o)=observed.get_mut(&id){o.reason=3;}
    if !self.warned{self.warned=true;crate::log_game("ACTOR_CAPTURE_UNAVAILABLE: skeleton changed; incompatible pose samples skipped");}continue;
   }
   if !identity.announced{metas.push(ActorMeta{id,character_id:c.character_id,npc_id:c.npc_id,npc_param:c.npc_param_id,think_param:think_param(chr),chr_type:c.chr_type as u32,category:identity.category});infos.push(identity.info);accepted.push(key);identity.skeleton=Some(definition.clone());skeletons.push(definition);}
   if let Some(o)=observed.get_mut(&id){o.known|=lifetime::KNOWN_POSE;o.reason=0;}
   body_ids.insert(key.0,id);
   if snapshot||!identity.announced{let r=ride.unwrap_or_default();context.push(EntityContext{time:now,id,category:identity.category,ride_flags:r.flags,ride_state:r.state,ride_param:r.param,mount_id:0});}
   let data=&c.modules.data;
   batch.push((id,ActorFrame{body:PlayerFrame{time:now,transform,matrix:[0.0;16],local:pose(local),model:pose(model),place:crate::arrival::place(chr),equip:Default::default()},hp:data.hp,max_hp:data.max_hp}));
  }
  if snapshot{let r=crate::companions::ride(player).unwrap_or_default();
   // Current pair-node handle, not last_mounted's stale pointer; only resolve an observed live body.
   let mount_id=if r.flags&crate::companions::MOUNTED!=0{body_ids.get(&r.counter_party).copied().unwrap_or(0)}else{0};
   context.push(EntityContext{time:now,id:0,category:0,ride_flags:r.flags,ride_state:r.state,ride_param:r.param,mount_id});}
  let current_ids:HashSet<u32>=self.ids.values().map(|i|i.info.id).collect();
  let catalog:HashSet<u32>=self.ids.values().filter(|i|i.announced).map(|i|i.info.id).chain(infos.iter().map(|i|i.id)).collect();
  let mut observations:Vec<_>=catalog.into_iter().map(|id|observed.remove(&id).unwrap_or_else(||Observation::unobserved(now,id))).filter(|o|self.observations.get(&o.id).is_none_or(|old|!o.same_state(old)||now.saturating_sub(old.time)>=1_000_000_000)).collect();
  // Retired generations still get a missing observation; they are never reused as new actors.
  for (&id,old) in &self.observations{if !current_ids.contains(&id)&&old.availability==0{observations.push(Observation::unobserved(now,id));}}
  observations.sort_by_key(|o|o.id);let committed=observations.clone();
  let count=batch.len() as u64;
  let companion_context:Vec<_>=context.iter().filter(|c|c.id!=0&&c.category!=0).copied().collect();
  if !batch.is_empty()||!context.is_empty()||!observations.is_empty(){match tx.try_send(Message::ActorBatch{infos,frames:batch,context,skeletons,observations}){
   Ok(())=>{for o in committed{if self.observations.get(&o.id).is_none_or(|old|!o.same_state(old)){crate::log_game(&format!("ACTOR_OBSERVATION: id={} state={:?} available={} death_flag={} hp={} pose={} reason={} time={}",o.id,lifetime::state(&o),o.availability,o.flags&lifetime::DEAD!=0,o.hp,o.known&lifetime::KNOWN_POSE!=0,o.reason,o.time));}self.observations.insert(o.id,o);}
    for key in accepted{if let Some(i)=self.ids.get_mut(&key){i.announced=true;}}
    if !metas.is_empty(){let _=tx.try_send(Message::ActorMeta(metas));}
    if snapshot{self.next_context=now+250_000_000;}
    for c in companion_context{if self.companion_ids.insert(c.id){let msg=format!("COMPANION_RECORDED: id={} category=0x{:X} ride_flags=0x{:X} state={} param={} (buddy-set/mount/summon evidence, not a guessed model id)",c.id,c.category,c.ride_flags,c.ride_state,c.ride_param);crate::log_game(&msg);crate::bone_replay::status(&msg);}}}
   Err(_)=>{self.dropped+=count;}}}}
 pub fn count(&self)->usize{(self.next-1) as usize}
 pub fn drops(&self)->u64{self.dropped}
 pub fn companions(&self)->usize{self.companion_ids.len()}
}

// ---------------------------------------------------------------------------------------- playback
/// Representation of a recorded actor during playback (shown in the diagnostics line).
#[derive(Clone,Copy,Debug,PartialEq,Eq)]
#[allow(dead_code)]
pub enum Representation{Live,Puppet,Missing}
struct Controlled{chr:usize,handle:u64,flags:Option<u32>,gravity:Option<bool>,transform:Transform,puppet:bool,saved_1c5:u8}
/// A puppet request in flight: the game's debug creator consumes it asynchronously.
#[derive(Clone,Copy)]
struct Request{id:u32,at:u64,prev_last:usize,npc_param:i32}
pub struct Player{tracks:Vec<(ActorInfo,ActorTrack,Vec<i16>)>,controlled:HashMap<u32,Controlled>,next_match:u64,local:Vec<u8>,model:Vec<u8>,logged:bool,categories:HashMap<u32,u32>,warned:HashSet<u32>,skeletons:HashMap<u32,crate::skeleton::Definition>,lifetime:lifetime::Timeline,
 meta:HashMap<u32,ActorMeta>,options:u32,request:Option<Request>,tries:HashMap<u32,(u32,u64)>,next_log:u64,last_existence:HashMap<u32,Existence>}
const MAX_PUPPETS:usize=8;
impl Player{
 pub fn new(actors:Vec<(ActorInfo,ActorTrack)>,context:&[EntityContext],skeletons:&HashMap<u32,crate::skeleton::Definition>,lifetime:lifetime::Timeline,meta:HashMap<u32,ActorMeta>)->Self{
  let categories=context.iter().filter(|c|c.id!=0).fold(HashMap::<u32,u32>::new(),|mut m,c|{*m.entry(c.id).or_default()|=c.category;m});
  let tracks=actors.into_iter().map(|(info,mut t)|{
   let n=t.len();let picks:Vec<PlayerFrame>=(0..3).filter_map(|k|t.get(k*(n.saturating_sub(1))/2).map(|f|f.body.clone())).collect();
   let parents=skeletons.get(&info.id).map(|d|d.parents.clone()).unwrap_or_else(||crate::replay_interpolation::learn_parents(picks.iter().map(|f|(&f.local[..],&f.model[..]))));(info,t,parents)}).collect();
  Self{tracks,controlled:HashMap::new(),next_match:0,local:Vec::new(),model:Vec::new(),logged:false,categories,warned:HashSet::new(),skeletons:skeletons.clone(),lifetime,meta,options:0,request:None,tries:HashMap::new(),next_log:0,last_existence:HashMap::new()}}
 pub fn len(&self)->usize{self.tracks.len()}
 pub fn set_options(&mut self,options:u32){self.options=options;}
 fn find(info:&ActorInfo,taken:&[usize],category:u32,unique_recorded:bool)->Option<usize>{
  let (bodies,buddies)=live_snapshot(false);let mut candidates=Vec::new();
  for a in bodies.into_iter().filter(|a|!taken.contains(a)){let c=unsafe{&*(a as *const ChrIns)};if c.npc_param_id!=info.npc_param{continue;}
   if (info.entity!=0&&c.event_entity_id==info.entity)||(info.entity==0&&handle_of(c)==info.handle){return Some(a);}
   if info.entity==0&&unique_recorded{let observed=crate::companions::category(a,buddies.contains(&a),crate::companions::ride(a));
    if role_matches(category,observed){candidates.push(a);}}}
  // No ordinal guesses for identical spirit ashes: a different handle is accepted only uniquely.
  (candidates.len()==1).then(||candidates[0])}
 fn log_transition(&mut self,id:u32,ex:Existence,t:u64){
  if self.last_existence.insert(id,ex)!=Some(ex){crate::log_game(&format!("ACTOR_LIFETIME: id={id} -> {ex:?} at replay time {:.2} s",t as f64/1e9));}}
 /// Asks the game's own debug character creator for a stand-in (opt-in). One request at a time.
 fn request_puppet(&mut self,info:&ActorInfo,now:u64,position:[f32;3],why:&'static str){
  if self.request.is_some()||self.controlled.values().filter(|c|c.puppet).count()>=MAX_PUPPETS{return;}
  if let Some((tries,not_before))=self.tries.get(&info.id){if *tries>=2||now<*not_before{return;}}
  let Some(m)=self.meta.get(&info.id).copied() else {if self.warned.insert(info.id){crate::log_game(&format!("ACTOR_RECONSTRUCTION_UNAVAILABLE: id={} recorded before construction metadata existed",info.id));}return;};
  if m.character_id==0||m.npc_param==0{return;}
  let Ok(world)=(unsafe{WorldChrMan::instance_mut()}) else {return};
  let creator=&*world.debug_chr_creator as *const eldenring::cs::CSDebugChrCreator;
  // The spawn byte must read as a bool and be idle before a request is written.
  let spawn=unsafe{&raw const (*creator).spawn as usize};if crate::companions::copy(spawn,&mut [0u8])==false||read_byte(spawn)!=0{return;}
  let prev_last=unsafe{(*creator).last_created_chr}.map(|p|p.as_ptr() as usize).unwrap_or(0);
  world.spawn_debug_character(&eldenring::cs::ChrDebugSpawnRequest{chr_id:m.character_id as i32,chara_init_param_id:-1,npc_param_id:m.npc_param,npc_think_param_id:m.think_param,event_entity_id:0,talk_id:0,is_player:false,pos_x:position[0],pos_y:position[1],pos_z:position[2]});
  let entry=self.tries.entry(info.id).or_insert((0,0));entry.0+=1;entry.1=now+5_000_000_000;
  self.request=Some(Request{id:info.id,at:now,prev_last,npc_param:m.npc_param});
  crate::log_game(&format!("PUPPET_REQUESTED: id={} chr=c{:04} npc_param={} think={} ({why}); entity id 0, so no map scripts, rewards or progression are tied to it",info.id,m.character_id,m.npc_param,m.think_param));}
 /// Looks for the body the creator made for the outstanding request and takes it over safely.
 fn poll_request(&mut self,now:u64){
  let Some(r)=self.request else {return};
  if now.saturating_sub(r.at)>3_000_000_000{crate::log_game(&format!("PUPPET_TIMEOUT: id={} no new character appeared within 3 s; no retry for 5 s",r.id));self.request=None;return;}
  let Ok(world)=(unsafe{WorldChrMan::instance()}) else {return};
  let creator=&*world.debug_chr_creator as *const eldenring::cs::CSDebugChrCreator;
  let spawn=unsafe{&raw const (*creator).spawn as usize};if read_byte(spawn)!=0{return;} // still queued
  let Some(last)=unsafe{(*creator).last_created_chr}.map(|p|p.as_ptr() as usize) else {return};
  if last==r.prev_last||self.controlled.values().any(|c|c.chr==last){return;}
  let live_now=live();if !live_now.contains(&last){return;}
  let c=unsafe{&*(last as *const ChrIns)};
  if c.npc_param_id!=r.npc_param||c.field_ins_handle.is_empty()||c.event_entity_id!=0{
   crate::log_game(&format!("PUPPET_REJECTED: id={} created body does not match the request (npc_param {} vs {}, entity {})",r.id,c.npc_param_id,r.npc_param,c.event_entity_id));self.request=None;return;}
  let (Some(transform),Some(flags),Some(gravity))=(read_transform(last),debug_flags(last).map(|a|unsafe{std::ptr::read_volatile(a as *const u32)}),gravity_flag(last).map(|a|unsafe{std::ptr::read_volatile(a as *const u8)}==1)) else {
   crate::log_game(&format!("PUPPET_REJECTED: id={} control flags could not be validated; the body is left alone",r.id));self.request=None;return;};
  // Safety first: invincible (it cannot be killed, so it cannot reward), rewards already marked as given.
  write_bits(last,5,INVINCIBLE,0);write_bits(last,6,DROPPED_ITEM|DROPPED_RUNES,0);
  self.controlled.insert(r.id,Controlled{chr:last,handle:handle_of(c),flags:Some(flags),gravity:Some(gravity),transform,puppet:true,saved_1c5:0});
  self.request=None;crate::log_game(&format!("PUPPET_ADOPTED: id={} body 0x{last:X}; invincible, rewards pre-marked, AI held",r.id));}
 /// Writes recorded actors at master replay time `t`. `_offset` is kept for the caller; positions are
 /// used in their verified original coordinate frame (see same_root_space).
 pub fn write(&mut self,t:u64,now:u64,_offset:[f32;3],interpolate:bool){
  let alive=live();let puppets_on=puppets_allowed(self.options);
  self.poll_request(now);
  // Every actor's existence at T comes from the recording only.
  let existence:HashMap<u32,Existence>=self.tracks.iter().map(|(i,_,_)|(i.id,self.lifetime.existence(i.id,t))).collect();
  for (id,ex) in existence.clone(){self.log_transition(id,ex,t);}
  if now>=self.next_match{self.next_match=now+500_000_000;
   let mut taken:Vec<usize>=self.controlled.values().map(|c|c.chr).collect();let mut newly=0;
   let wanted:Vec<ActorInfo>=self.tracks.iter().filter(|(i,tr,_)|matches!(existence[&i.id],Existence::Alive|Existence::Dead)&&!self.controlled.contains_key(&i.id)&&active_at(&tr.times,t)).map(|(i,_,_)|*i).collect();
   for info in wanted{
    let unique_recorded=self.tracks.iter().filter(|(other,_,_)|other.npc_param==info.npc_param).count()==1;
    let found=Self::find(&info,&taken,self.categories.get(&info.id).copied().unwrap_or(0),unique_recorded);
    let live_state=found.and_then(live_state);
    match plan(existence[&info.id],live_state){
     Plan::Drive=>{let chr=found.unwrap();let c=unsafe{&*(chr as *const ChrIns)};
      let Some(transform)=read_transform(chr) else {continue};
      let flags=debug_flags(chr).map(|a|unsafe{std::ptr::read_volatile(a as *const u32)});
      let gravity=gravity_flag(chr).map(|a|unsafe{std::ptr::read_volatile(a as *const u8)}==1);
      if flags.is_none()||gravity.is_none(){if self.warned.insert(info.id){crate::log_game(&format!("ACTOR_UNAVAILABLE: id={} control flags/gravity could not be validated; no pose/root writes",info.id));}continue;}
      let saved=flag_byte(chr,5).map(read_byte).unwrap_or(0);write_bits(chr,5,INVINCIBLE,0); // a held body cannot be hurt or killed meanwhile
      self.controlled.insert(info.id,Controlled{chr,handle:handle_of(c),flags,gravity,transform,puppet:false,saved_1c5:saved});taken.push(chr);newly+=1;}
     Plan::Puppet(why)=>{
      if puppets_on{let here=read_transform(live_player().unwrap_or(0)).map(|p|p[2]).unwrap_or([0.0;4]);self.request_puppet(&info,now,[here[0]+2.0,here[1],here[2]],why);}
      else if self.warned.insert(info.id){crate::log_game(&format!("ACTOR_RECONSTRUCTION_UNAVAILABLE: id={} npc_param={} {why}; replay puppets are off (Settings > Replay world, or THEATER_PUPPETS=1)",info.id,info.npc_param));}}
     Plan::Unavailable(why)=>{if self.warned.insert(info.id){crate::log_game(&format!("ACTOR_UNAVAILABLE: id={} {why}",info.id));}}
     Plan::Absent=>{}}}
   if newly>0||!self.logged{self.logged=true;crate::log_game(&format!("ACTORS: {} of {} recorded characters held by the replay ({} puppets)",self.controlled.len(),self.tracks.len(),self.controlled.values().filter(|c|c.puppet).count()));}}
  if now>=self.next_log&&!self.controlled.is_empty(){self.next_log=now+5_000_000_000;
   for (id,c) in &self.controlled{crate::log_game(&format!("ACTOR_DIAG: id={id} representation={:?} existence={:?} body=0x{:X} ai_isolation=no-move+no-attack+invincible",if c.puppet{Representation::Puppet}else{Representation::Live},existence.get(id),c.chr));}}
  for (info,track,parents) in &mut self.tracks{
   let Some(ctl)=self.controlled.get(&info.id) else {continue};let chr=ctl.chr;let is_puppet=ctl.puppet;
   // The character must still be the same one (a reload can reuse the address).
   if !alive.contains(&chr)||handle_of(unsafe{&*(chr as *const ChrIns)})!=ctl.handle{self.controlled.remove(&info.id);continue;}
   if debug_flags(chr).is_none(){if let Some(c)=self.controlled.remove(&info.id){restore(&c);}continue;}
   // Recording says it does not exist right now: a puppet is hidden (never deleted mid-replay, so a seek
   // back can show it again); a live body is given back.
   let ex=existence[&info.id];
   if matches!(ex,Existence::NotYet|Existence::Gone|Existence::Left|Existence::Unknown){
    if is_puppet{set_render(chr,false);}else if let Some(c)=self.controlled.remove(&info.id){restore(&c);}continue;}
   if is_puppet{set_render(chr,true);}
   else if let Some((dead,render))=live_state(chr){ // a live body that stopped matching the recording is released, never revived
    if plan(ex,Some((dead,render)))!=Plan::Drive&&ex==Existence::Alive{if let Some(c)=self.controlled.remove(&info.id){restore(&c);}continue;}}
   if self.skeletons.get(&info.id).is_some_and(|d|crate::skeleton::read(chr,info.id).as_ref()!=Some(d)){
    if let Some(c)=self.controlled.remove(&info.id){restore(&c);}
    if self.warned.insert(info.id){crate::log_game(&format!("ACTOR_UNAVAILABLE: id={} skeleton identity differs; no pose applied",info.id));}continue;
   }
   let n=track.len();if !active_at(&track.times,t){
    if is_puppet{set_render(chr,false);}else if let Some(c)=self.controlled.remove(&info.id){restore(&c);}continue;}
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
 /// Gives every held live body its flags, gravity and position back, and asks the game to unload puppets.
 pub fn release(&mut self){
  let alive=live();let n=self.controlled.len();let mut puppets=0;
  self.request=None;
  for (_,c) in self.controlled.drain(){
   if !alive.contains(&c.chr)||handle_of(unsafe{&*(c.chr as *const ChrIns)})!=c.handle{continue;}
   if c.puppet{puppets+=1;unload_puppet(c.chr);}else{restore(&c);}}
  if n>0{crate::log_game(&format!("ACTORS: {n} characters given back to the game ({puppets} puppets asked to unload)"));}}
}
fn live_player()->Option<usize>{let w=unsafe{WorldChrMan::instance()}.ok()?;w.main_player.as_ref().map(|p|&p.chr_ins as *const _ as usize)}
/// Native cleanup for a debug-created stand-in: the SDK documents ChrDebugFlags.force_unloaded (bit 10) as
/// "set base_transparency_modifier to -1 and change chrSetEntry status to Unloading". Whether that fully
/// removes a debug-created body is UNVERIFIED; the log line asks the tester to confirm.
fn unload_puppet(chr:usize){
 set_render(chr,false);write_bits(chr,5,INVINCIBLE,0);
 if let Some(a)=debug_flags(chr){unsafe{let v=std::ptr::read_volatile(a as *const u32);std::ptr::write_volatile(a as *mut u32,v|(1<<10)|(profile::VAL_DEBUG_FLAG_NO_MOVE|profile::VAL_DEBUG_FLAG_NO_ATTACK) as u32);}}
 crate::log_game(&format!("PUPPET_UNLOAD_REQUESTED: body 0x{chr:X} (force_unloaded); verify it disappears within seconds"));}

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
 if c.saved_1c5&INVINCIBLE==0{write_bits(c.chr,5,0,INVINCIBLE);}
 if let (Some(a),Some(f))=(debug_flags(c.chr),c.flags){unsafe{let now=std::ptr::read_volatile(a as *const u32);let mask=(profile::VAL_DEBUG_FLAG_NO_MOVE|profile::VAL_DEBUG_FLAG_NO_ATTACK) as u32;std::ptr::write_volatile(a as *mut u32,(now&!mask)|(f&mask));}}
 if let Some(g)=c.gravity{set_flag(gravity_flag(c.chr),g);}write_transform(c.chr,&c.transform);set_flag(proxy_flag(c.chr),true);
}
#[cfg(test)]mod tests{
 use super::*;
 #[test]fn plan_follows_the_recording_not_the_live_world(){
  // Recorded alive but the live body is dead/despawned: never revive it, ask for a stand-in.
  assert_eq!(plan(Existence::Alive,Some((true,true))),Plan::Puppet("live body is dead or hidden but the recording says alive"));
  assert_eq!(plan(Existence::Alive,None),Plan::Puppet("no live body in the world"));
  assert_eq!(plan(Existence::Alive,Some((false,true))),Plan::Drive);
  assert_eq!(plan(Existence::Dead,Some((true,true))),Plan::Drive);assert_eq!(plan(Existence::Dead,Some((false,true))),Plan::Drive);
  for e in [Existence::NotYet,Existence::Gone,Existence::Left]{assert_eq!(plan(e,Some((false,true))),Plan::Absent);assert_eq!(plan(e,None),Plan::Absent);}
  assert!(matches!(plan(Existence::Unknown,None),Plan::Unavailable(_)));}
 #[test]fn no_hold_outside_lifetime_or_across_gaps(){let t=[10,20,900_000_000];assert!(!active_at(&t,9));assert!(active_at(&t,15));assert!(!active_at(&t,600_000_000));assert!(active_at(&t,900_000_000));assert!(!active_at(&t,900_000_001));assert!(!active_at(&[],10));}
 #[test]fn buddy_membership_is_not_mount_or_summon_identity(){assert!(!role_matches(1,1));assert!(!role_matches(3,1));assert!(role_matches(3,3));assert!(!role_matches(4,8));assert!(role_matches(4,4));}
}

//! Enemies, NPCs and bosses in replays (Phase 2.2), with the same bone-pose method as the player.
//!
//! Recording (game thread, Draw_Pre, while the host records): every character within the recording
//! radius (GameProfile TM_VAL_ACTOR_RADIUS, default 100 m) gets a stable recording id (never a
//! pointer) the first time it is seen, with its handle, map entity id, NpcParam id and type. Each
//! frame (every 3rd frame beyond TM_VAL_ACTOR_NEAR) it records root transform, global position, HP and
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
use crate::world_file::{ActorFrame,ActorInfo,ActorTrack,Message,PlayerFrame};
use eldenring::cs::{ChrIns,CSChrPhysicsModule,WorldChrMan};
use fromsoftware_shared::FromStatic;
use std::collections::HashMap;
use std::sync::mpsc::SyncSender;

const QS:usize=48;
fn read_ptr(a:usize)->usize{if a<0x10000{0}else{unsafe{std::ptr::read_volatile(a as *const usize)}}}
fn read_i32(a:usize)->i32{if a<0x10000{0}else{unsafe{std::ptr::read_volatile(a as *const i32)}}}
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
fn live()->Vec<usize>{
 let Ok(world)=(unsafe{WorldChrMan::instance()}) else {return Vec::new()};
 let player=world.main_player.as_ref().map(|p|&p.chr_ins as *const _ as usize);
 world.chr_inses_by_distance.iter().map(|e|e.chr_ins.as_ptr() as usize).filter(|a|Some(*a)!=player).collect()}

// ------------------------------------------------------------------------------------------ record
/// Per-recording identities and rate control; lives while the host records.
pub struct Recorder{ids:HashMap<(u64,u32,i32),u32>,next:u32,frame:u64,warned:bool}
impl Recorder{
 pub fn new()->Self{Self{ids:HashMap::new(),next:1,frame:0,warned:false}}
 /// Samples nearby characters; `player` is the main player's ChrIns, `tx` the world file writer.
 pub fn sample(&mut self,now:u64,player:usize,tx:&SyncSender<Message>){
  self.frame+=1;
  let me=unsafe{&*(player as *const ChrIns)};let origin=me.modules.physics.position;
  let (radius,near)=(profile::VAL_ACTOR_RADIUS as f32,profile::VAL_ACTOR_NEAR as f32);
  let mut batch=Vec::new();
  for chr in live(){
   let c=unsafe{&*(chr as *const ChrIns)};if c.field_ins_handle.is_empty(){continue;}
   let p=c.modules.physics.position;let d=((p.0-origin.0).powi(2)+(p.1-origin.1).powi(2)+(p.2-origin.2).powi(2)).sqrt();
   if !d.is_finite()||d>radius{continue;}
   if d>near&&self.frame%3!=0{continue;}
   let key=(handle_of(c),c.event_entity_id,c.npc_param_id);
   let id=*self.ids.entry(key).or_insert_with(||{let id=self.next;self.next+=1;
    let _=tx.try_send(Message::ActorInfo(ActorInfo{id,handle:key.0,entity:key.1,npc_param:key.2,chr_type:c.chr_type as u32,first_seen:now}));id});
   let (Some(n),Some((local,model)),Some(transform))=(bone_count(chr),pose_arrays(chr),read_transform(chr)) else {
    if !self.warned{self.warned=true;crate::log_game("ACTORS: a character's skeleton could not be read (counts disagree); it is skipped");}continue};
   let pose=|a:usize|unsafe{std::slice::from_raw_parts(a as *const u8,n*QS)}.to_vec();
   let data=&c.modules.data;
   batch.push((id,ActorFrame{body:PlayerFrame{time:now,transform,matrix:[0.0;16],local:pose(local),model:pose(model),place:crate::arrival::place(chr),equip:Default::default()},hp:data.hp,max_hp:data.max_hp}));
  }
  if !batch.is_empty(){let _=tx.try_send(Message::Actors(batch));}}
 pub fn count(&self)->usize{self.ids.len()}
}

// ---------------------------------------------------------------------------------------- playback
struct Controlled{chr:usize,handle:u64,flags:Option<u32>,gravity:Option<bool>,transform:Transform}
pub struct Player{tracks:Vec<(ActorInfo,ActorTrack,Vec<i16>)>,controlled:HashMap<u32,Controlled>,next_match:u64,local:Vec<u8>,model:Vec<u8>,logged:bool}
impl Player{
 pub fn new(actors:Vec<(ActorInfo,ActorTrack)>)->Self{
  let tracks=actors.into_iter().map(|(info,mut t)|{
   let n=t.len();let picks:Vec<PlayerFrame>=(0..3).filter_map(|k|t.get(k*(n.saturating_sub(1))/2).map(|f|f.body.clone())).collect();
   let parents=crate::replay_interpolation::learn_parents(picks.iter().map(|f|(&f.local[..],&f.model[..])));(info,t,parents)}).collect();
  Self{tracks,controlled:HashMap::new(),next_match:0,local:Vec::new(),model:Vec::new(),logged:false}}
 pub fn len(&self)->usize{self.tracks.len()}
 fn find(info:&ActorInfo,taken:&[usize])->Option<usize>{
  live().into_iter().filter(|a|!taken.contains(a)).find(|a|{let c=unsafe{&*(*a as *const ChrIns)};
   c.npc_param_id==info.npc_param&&((info.entity!=0&&c.event_entity_id==info.entity)||(info.entity==0&&handle_of(c)==info.handle))})}
 /// Writes every recorded actor that exists at replay time `t`; `offset` is today's (global - physics).
 pub fn write(&mut self,t:u64,now:u64,offset:[f32;3],interpolate:bool){
  let alive=live();
  if now>=self.next_match{self.next_match=now+500_000_000;
   let taken:Vec<usize>=self.controlled.values().map(|c|c.chr).collect();let mut newly=0;
   for (info,_,_) in &self.tracks{if self.controlled.contains_key(&info.id){continue;}
    if let Some(chr)=Self::find(info,&taken){let c=unsafe{&*(chr as *const ChrIns)};
     let Some(transform)=read_transform(chr) else {continue};
     let flags=debug_flags(chr).map(|a|unsafe{std::ptr::read_volatile(a as *const u32)});
     let gravity=gravity_flag(chr).map(|a|unsafe{std::ptr::read_volatile(a as *const u8)}==1);
     self.controlled.insert(info.id,Controlled{chr,handle:handle_of(c),flags,gravity,transform});newly+=1;}}
   if newly>0||!self.logged{self.logged=true;crate::log_game(&format!("ACTORS: {} of {} recorded characters found in the world and held by the replay",self.controlled.len(),self.tracks.len()));}}
  for (info,track,parents) in &mut self.tracks{
   let Some(ctl)=self.controlled.get(&info.id) else {continue};let chr=ctl.chr;
   // The character must still be the same one (a reload can reuse the address).
   if !alive.contains(&chr)||handle_of(unsafe{&*(chr as *const ChrIns)})!=ctl.handle{self.controlled.remove(&info.id);continue;}
   let n=track.len();if n==0||t<track.times[0]||t>track.times[n-1]{continue;}
   let i=track.times.partition_point(|x|*x<=t).saturating_sub(1).min(n-1);
   let (Some(a),Some(b))=(track.get(i).cloned(),track.get((i+1).min(n-1)).cloned()) else {continue};
   let span=b.body.time.saturating_sub(a.body.time);
   let jump=(0..3).map(|k|(b.body.transform[2][k]-a.body.transform[2][k]).powi(2)).sum::<f32>().sqrt();
   let alpha=if !interpolate||span==0||span>500_000_000||jump>1.5{0.0}else{(t-a.body.time) as f64/span as f64};
   let Some(mut root)=crate::replay_interpolation::evaluate(&a.body.transform,&b.body.transform,alpha) else {continue};
   if a.body.place.block!=-1{let w=alpha as f32;for k in 0..3{root[2][k]=a.body.place.global[k]+(b.body.place.global[k]-a.body.place.global[k])*w-offset[k];}}
   let bones=a.body.local.len()/QS;
   if bone_count(chr)==Some(bones)&&b.body.local.len()==a.body.local.len(){
    if let Some((local,model))=pose_arrays(chr){
     self.local.resize(bones*QS,0);self.model.resize(bones*QS,0);
     if crate::replay_interpolation::pose_into(&a.body.local,&b.body.local,alpha,&mut self.local).is_some()
      &&crate::replay_interpolation::model_from_local(&self.local,parents,&a.body.model,&b.body.model,alpha,&mut self.model).is_some(){
      unsafe{std::ptr::copy_nonoverlapping(self.local.as_ptr(),local as *mut u8,bones*QS);std::ptr::copy_nonoverlapping(self.model.as_ptr(),model as *mut u8,bones*QS);}}}}
   write_transform(chr,&root);set_flag(proxy_flag(chr),true);set_flag(gravity_flag(chr),true);
   if let (Some(a),Some(_))=(debug_flags(chr),ctl.flags){unsafe{let v=std::ptr::read_volatile(a as *const u32);std::ptr::write_volatile(a as *mut u32,v|(profile::VAL_DEBUG_FLAG_NO_MOVE|profile::VAL_DEBUG_FLAG_NO_ATTACK) as u32);}}
  }}
 /// Gives every held character its flags, gravity and position back.
 pub fn release(&mut self){
  let alive=live();let n=self.controlled.len();
  for (_,c) in self.controlled.drain(){
   if !alive.contains(&c.chr)||handle_of(unsafe{&*(c.chr as *const ChrIns)})!=c.handle{continue;}
   if let (Some(a),Some(f))=(debug_flags(c.chr),c.flags){unsafe{std::ptr::write_volatile(a as *mut u32,f)};}
   if let Some(g)=c.gravity{set_flag(gravity_flag(c.chr),g);}
   write_transform(c.chr,&c.transform);set_flag(proxy_flag(c.chr),true);}
  if n>0{crate::log_game(&format!("ACTORS: {n} characters given back to the game"));}}
}

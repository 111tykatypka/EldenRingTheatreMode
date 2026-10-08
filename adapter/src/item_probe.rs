//! Research probe (read-only, log only): what changes on the player while an item such as a flask is used.
//! Logs the item the animation queued for use, the three item sfx ids the game keeps, and the set of active
//! special effects (with the visual-effect ids of each new one). Nothing here writes to the game; it exists to
//! find out which state a replay would have to reproduce for the flask model and its effects.
use eldenring::cs::ChrIns;
use std::mem::offset_of;

#[derive(Default)]
pub struct Probe{sfx_total:Option<u32>,sfx_logs:u32,anim:Option<i32>,window:Option<Window>,item:Option<u32>,sfx:Option<[i32;3]>,effects:Option<Vec<i32>>,loc:Option<[u8;crate::weapon_loc::BYTES]>}
/// After an item is queued for use: for a few seconds, 10 times a second, report which bytes of the player's
/// assembly, action-flag module, model instance and the front of the ChrIns itself changed (offset old->new).
/// The weapon hiding while a flask is drunk must be one of them, since weapon_loc does not change then.
struct Window{until:f64,next:f64,regions:Vec<(String,usize,Vec<u8>)>}
/// The player's model-instance object and what it points to one level down (weapon / armor model instances).
fn asm_children(chr:usize)->Vec<(String,usize,usize)>{
 let Some(root)=crate::companions::word(chr+offset_of!(eldenring::cs::PlayerIns,chr_asm)+0x10).filter(|p|*p>0x10000) else {return vec![]};
 let mut out=vec![("AsmModelIns".to_string(),root,0x800usize)];let mut seen=vec![root];
 for off in (0..0x400usize).step_by(8){
  let Some(p)=crate::companions::word(root+off).filter(|p|*p>0x10000&&*p<0x7FFF_FFFF_FFFF&&!seen.contains(p)) else {continue};
  let mut probe=[0u8;8];if !crate::companions::copy(p,&mut probe){continue;}
  seen.push(p);out.push((format!("AsmChild+{off:#x}"),p,0x200));if out.len()>=26{break;}}
 out}
fn region_list(chr:usize)->Vec<(String,usize,usize)>{
 let mut v=vec![("ChrIns".to_string(),chr,0x500usize)];
 let ptr=|o:usize|crate::companions::word(chr+o).filter(|p|*p>0x10000);
 if let Some(a)=ptr(offset_of!(eldenring::cs::PlayerIns,chr_asm)){v.push(("ChrAsm".to_string(),a,std::mem::size_of::<eldenring::cs::ChrAsm>()));}
 if let Some(a)=crate::weapon_loc::module(chr){v.push(("ActionFlag".to_string(),a,0x258));}
 if let Some(a)=ptr(offset_of!(ChrIns,chr_model_ins)){
  // The flask in the hand is a model attached to a hand dummy polygon; its attachment must show up somewhere below the model
  // instance: a pointer that appears while the sip lasts (a bigger window than before, plus the objects it points to).
  v.push(("ModelIns".to_string(),a,0x1000));
  let mut seen=vec![a];
  for off in (0..0x1000usize).step_by(8){
   let Some(p)=crate::companions::word(a+off).filter(|p|*p>0x10000&&*p<0x7FFF_FFFF_FFFF&&!seen.contains(p)) else {continue};
   let mut probe=[0u8;8];if !crate::companions::copy(p,&mut probe){continue;}
   seen.push(p);v.push((format!("ModelIns+{off:#x}"),p,0x200));if seen.len()>40{break;}}}
v.extend(asm_children(chr));
 // Character modules by their slot in ChrInsModuleContainer (8 bytes each): time act 3, sfx 22, vfx 23, model param modifier 26.
 if let Some(container)=ptr(offset_of!(ChrIns,modules)){
  for (name,slot,size) in [("TimeAct",3usize,0x100usize),("SfxModule",22,0x200),("VfxModule",23,0x200),("ModelParamModifier",26,0x60)]{
   let Some(m)=crate::companions::word(container+slot*8).filter(|p|*p>0x10000) else {continue};
   v.push((name.to_string(),m,size));
   let mut seen=vec![m];
   for off in (0..size).step_by(8){
    let Some(p)=crate::companions::word(m+off).filter(|p|*p>0x10000&&*p<0x7FFF_FFFF_FFFF&&!seen.contains(p)) else {continue};
    let mut probe=[0u8;8];if !crate::companions::copy(p,&mut probe){continue;}
    seen.push(p);v.push((format!("{name}+{off:#x}"),p,0x300));if seen.len()>10{break;}}}}
 v}
/// Heap objects reachable from a character's model instance, as (offset path, address): two levels, enough to see model parts being
/// attached or removed. Research probe for enemy weapon swaps.
pub fn model_pointers(chr:usize)->Vec<(u32,u64)>{
 let mut out=Vec::new();
 let Some(root)=crate::companions::word(chr+offset_of!(ChrIns,chr_model_ins)).filter(|p|*p>0x10000) else {return out};
 let ok=|p:usize|p>0x10000&&p<0x7FFF_FFFF_FFFF&&{let mut b=[0u8;8];crate::companions::copy(p,&mut b)};
 for off in (0..0x400usize).step_by(8){
  let Some(p)=crate::companions::word(root+off).map(|p|p as usize).filter(|p|ok(*p)) else {continue};
  out.push((off as u32,p as u64));
  for o2 in (0..0x100usize).step_by(8){
   if let Some(q)=crate::companions::word(p+o2).map(|q|q as usize).filter(|q|ok(*q)){out.push((((off as u32)<<12)|o2 as u32,q as u64));}
   if out.len()>300{return out;}}}
 out}
/// Ragdoll research: (ChrCtrl ragdoll state byte, ragdoll object address, up to 12 heap pointers inside that object).
pub fn ragdoll_info(chr:usize)->Option<(u8,usize,Vec<(u32,u64)>)>{
 let ctrl=crate::companions::word(chr+offset_of!(ChrIns,chr_ctrl)).filter(|p|*p>0x10000)? as usize;
 let mut b=[0u8;1];if !crate::companions::copy(ctrl+0x128,&mut b){return None;}
 let ragdoll=crate::companions::word(ctrl+0x28).unwrap_or(0) as usize;
 let mut ptrs=Vec::new();
 if ragdoll>0x10000{for off in (0..0x200usize).step_by(8){if let Some(p)=crate::companions::word(ragdoll+off).filter(|p|*p>0x10000&&*p<0x7FFF_FFFF_FFFF){let mut t=[0u8;8];if crate::companions::copy(p as usize,&mut t){ptrs.push((off as u32,p as u64));if ptrs.len()>=12{break;}}}}}
 Some((b[0],ragdoll,ptrs))}
fn snapshot(chr:usize)->Vec<(String,usize,Vec<u8>)>{
 region_list(chr).into_iter().filter_map(|(n,a,l)|{let mut b=vec![0u8;l];crate::companions::copy(a,&mut b).then_some((n,a,b))}).collect()}
impl Probe{
 fn watch(&mut self,chr:usize,seconds:f64){
  let Some(w)=&mut self.window else {return};
  if seconds>w.until{self.window=None;crate::log_game("ITEM_DIFF: window closed");return;}
  if seconds<w.next{return;}w.next=seconds+0.1;
  let now=snapshot(chr);
  for (name,addr,new) in &now{
   let Some((_,_,old))=w.regions.iter().find(|(n,a,_)|n==name&&a==addr) else {continue};
   let diffs:Vec<String>=old.iter().zip(new).enumerate().filter(|(_,(a,b))|a!=b).take(40).map(|(i,(a,b))|format!("{i:#x}:{a:02X}->{b:02X}")).collect();
   if !diffs.is_empty(){crate::log_game(&format!("ITEM_DIFF t={seconds:.2}s {name}: {}",diffs.join(" ")));}
   // A pointer that appears or disappears (0 <-> address) is the signature of an object being attached or removed.
   let ptr=|v:u64|(0x1_0000_0000_00..0x7FFF_FFFF_FFFF).contains(&v);
   let flips:Vec<String>=old.chunks_exact(8).zip(new.chunks_exact(8)).enumerate().filter_map(|(i,(a,b))|{let (a,b)=(u64::from_le_bytes(a.try_into().unwrap()),u64::from_le_bytes(b.try_into().unwrap()));
    ((a==0&&ptr(b))||(ptr(a)&&b==0)).then(||format!("{:#x}:{a:#x}->{b:#x}",i*8))}).take(20).collect();
   if !flips.is_empty(){crate::log_game(&format!("ITEM_PTR t={seconds:.2}s {name}: {}",flips.join(" ")));}}
  w.regions=now;}
 /// Call once per recorded frame; `seconds` is time since recording start for the log lines.
 pub fn sample(&mut self,chr:usize,seconds:f64){
  let item=crate::companions::dword(chr+offset_of!(ChrIns,tae_queued_use_item));
  let sfx=[offset_of!(ChrIns,item_use_cast_sfx_id),offset_of!(ChrIns,item_use_fire_sfx_id),offset_of!(ChrIns,item_use_effect_sfx_id)].map(|o|crate::companions::dword(chr+o));
  let sfx=match sfx{[Some(a),Some(b),Some(c)]=>Some([a as i32,b as i32,c as i32]),_=>None};
  if item!=self.item||sfx!=self.sfx{
   crate::log_game(&format!("ITEM_PROBE t={seconds:.2}s: queued_use_item={} sfx cast/fire/effect={:?}",item.map(|v|format!("0x{v:08X}")).unwrap_or("?".into()),sfx));
   if self.item.is_some()&&item.is_some_and(|v|v!=0&&v!=u32::MAX){self.window=Some(Window{until:seconds+6.0,next:seconds,regions:snapshot(chr)});crate::log_game("ITEM_DIFF: window opened (6 s, 10 Hz)");}
   self.item=item;self.sfx=sfx;}
  self.watch(chr,seconds);
  // How many visual effects exist in the world blocks right now (WorldSfxMan: block list at +0x30, count at +0x28, stride 0x78,
  // each block\'s total_sfx_count at +0x5C; layout from the SDK structs). Logged when the total changes: a rise at a hit, a spell or
  // a death marks an effect being created. Read-only; says how many, not which.
  if self.sfx_logs<600{if let Ok(m)=unsafe{eldenring::cs::WorldSfxMan::instance()}{
   let base=m as *const _ as usize;
   if let (Some(count),Some(list))=(crate::companions::dword(base+0x28),crate::companions::word(base+0x30)){
    if count<=256&&list>0x10000{let total:u32=(0..count as usize).filter_map(|i|crate::companions::dword(list+i*0x78+0x5C)).filter(|n|*n<100_000).sum();
     if self.sfx_total!=Some(total){self.sfx_logs+=1;crate::log_game(&format!("SFX_COUNT t={seconds:.2}s: {total} effects in {count} world blocks (was {:?})",self.sfx_total));self.sfx_total=Some(total);}}}}}
  // The animation the game's time-act module is playing (id and play time), logged when the id changes.
  if let Some(container)=crate::companions::word(chr+offset_of!(ChrIns,modules)).filter(|p|*p>0x10000){
   if let Some(t)=crate::companions::word(container+3*8).filter(|p|*p>0x10000){
    if let Some(read)=crate::companions::dword(t+0xC4){let q=t+0x20+(read as usize%10)*16;
     if let (Some(id),Some(len))=(crate::companions::dword(q),crate::companions::dword(q+12)){let id=id as i32;
      if self.anim!=Some(id){crate::log_game(&format!("TIMEACT_PROBE t={seconds:.2}s: animation {id} (length {:.2} s)",f32::from_bits(len)));self.anim=Some(id);}}}}}
  if let Some(ids)=effect_ids(chr){
   if self.effects.as_ref()!=Some(&ids){
    let old=self.effects.clone().unwrap_or_default();
    let added:Vec<String>=ids.iter().filter(|i|!old.contains(i)).map(|i|format!("{i}{}",vfx_of(chr,*i))).collect();
    let removed:Vec<i32>=old.iter().copied().filter(|i|!ids.contains(i)).collect();
    if self.effects.is_some(){crate::log_game(&format!("SPEFFECT_PROBE t={seconds:.2}s: added {added:?} removed {removed:?} (now {} active)",ids.len()));}
    self.effects=Some(ids);}}
  if let Some(l)=crate::weapon_loc::read(chr){
   if self.loc!=Some(l){crate::log_game(&format!("WEAPON_LOCATION_PROBE t={seconds:.2}s: lh {:?} rh {:?} overridden={}",[(l[0],l[1] as i8),(l[2],l[3] as i8),(l[4],l[5] as i8),(l[6],l[7] as i8)],[(l[8],l[9] as i8),(l[10],l[11] as i8),(l[12],l[13] as i8),(l[14],l[15] as i8)],l[16]));self.loc=Some(l);}}}
}
/// Param ids of the active special effects, sorted; None when the structure does not read as one.
fn effect_ids(chr:usize)->Option<Vec<i32>>{
 let c=unsafe{&*(chr as *const ChrIns)};let p=c.special_effect.as_ptr();
 if (p as usize)<0x10000||crate::companions::word(p as usize+offset_of!(eldenring::cs::SpecialEffect,owner))!=Some(chr){return None;}
 let se=unsafe{&*p};let mut ids:Vec<i32>=se.entries().take(256).map(|e|e.param_id).collect();ids.sort_unstable();Some(ids)}
fn vfx_of(chr:usize,id:i32)->String{
 let c=unsafe{&*(chr as *const ChrIns)};let se=unsafe{&*c.special_effect.as_ptr()};
 for e in se.entries().take(256){if e.param_id==id{
  if let Some(p)=e.param_data{let p=unsafe{p.as_ref()};return format!(" state={} vfx[{},{},{},{}]",p.state_info(),p.vfx_id(),p.vfx_id1(),p.vfx_id2(),p.vfx_id3());}}}
 String::new()}

// ---------------------------------------------------------------------------------------------------------------
// Special effects that matter to what is drawn. The CheatEngine table's stateInfo list names state 184 "Hide Weapon",
// and the game drinks a flask / uses an item by switching such an effect on. Both uses below go through the game's
// own `apply_speffect` / `remove_speffect` and only for effects whose param changes no HP, FP or stamina.
use eldenring::cs::{ChrInsExt,EquipParamGoods,SoloParamRepository,SpEffectParam};
use fromsoftware_shared::FromStatic;
use std::sync::{Mutex,OnceLock};

const HIDE_WEAPON_STATE:u16=184;
const SCAN_MAX:u32=700_000;
/// Every SpEffect param id whose state info is "Hide Weapon" (found once, by scanning the param table).
fn hide_set()->&'static Vec<i32>{
 static SET:OnceLock<Vec<i32>>=OnceLock::new();
 SET.get_or_init(||{
  let started=std::time::Instant::now();let mut v=Vec::new();let mut rows=0u32;let mut hist=std::collections::BTreeMap::<u16,u32>::new();
  if let Ok(repo)=unsafe{SoloParamRepository::instance()}{for id in 0..=SCAN_MAX{if let Some(p)=repo.get::<SpEffectParam>(id){rows+=1;*hist.entry(p.state_info()).or_default()+=1;if p.state_info()==HIDE_WEAPON_STATE{v.push(id as i32);}}}}
  // What the effects seen around a flask / whistle really are (state info, visual effect ids, and whether they touch HP).
  if let Ok(repo)=unsafe{SoloParamRepository::instance()}{
   for id in [26u32,39,45,81,100,101,106,141,202,430,4289,9607,9610,19996,19997,19998,19999,90301,100000,100001,100002,100006,100007,100170,100240,100300,100390,100690,102368,501000,501025,501050]{
    if let Some(p)=repo.get::<SpEffectParam>(id){crate::log_game(&format!("SPEFFECT_ROW id={id} state_info={} vfx[{},{},{},{},{},{},{}] hp rate/point {}/{} endurance {} icon {}",p.state_info(),p.vfx_id(),p.vfx_id1(),p.vfx_id2(),p.vfx_id3(),p.vfx_id4(),p.vfx_id5(),p.vfx_id6(),p.change_hp_rate(),p.change_hp_point(),p.effect_endurance(),p.icon_id()));}else{crate::log_game(&format!("SPEFFECT_ROW id={id}: no such row"));}}
   for g in [130u32,1001,1051]{if let Some(p)=repo.get::<EquipParamGoods>(g){crate::log_game(&format!("GOODS_ROW id={g} ref_id_default={} use_anim={} sfx_variation={}",p.ref_id_default(),p.goods_use_anim(),p.sfx_variation_id()));}}}
  crate::log_game(&format!("SPEFFECT_PARAM_SCAN: {rows} rows in ids 0..={SCAN_MAX}; state info histogram {:?}",hist));
  crate::log_game(&format!("HIDE_WEAPON_EFFECTS: {} special effects have state info {HIDE_WEAPON_STATE} ({:?}); scan of ids 0..={SCAN_MAX} took {:?}",v.len(),&v[..v.len().min(40)],started.elapsed()));v})}
/// The hide-weapon effect currently active on `chr`, 0 for none.
pub fn active_hide(chr:usize)->i32{
 let set=hide_set();effect_ids(chr).and_then(|ids|ids.into_iter().find(|i|set.contains(i))).unwrap_or(0)}
/// Effects we added ourselves (to take exactly those back).
static APPLIED:Mutex<i32>=Mutex::new(0);
fn pure_visual(p:&eldenring::param::SP_EFFECT_PARAM_ST)->bool{
 p.change_hp_rate()==0.0&&p.change_hp_point()==0&&p.change_mp_rate()==0.0&&p.change_mp_point()==0&&p.change_stamina_rate()==0.0&&p.change_stamina_point()==0}
fn apply(chr:usize,id:i32){let c=unsafe{&mut *(chr as *mut ChrIns)};ChrInsExt::apply_speffect(c,id,true);}
fn remove(chr:usize,id:i32){let c=unsafe{&mut *(chr as *mut ChrIns)};ChrInsExt::remove_speffect(c,id);}
/// Makes the player's hide-weapon state equal the recorded one (`want` = recorded effect id, 0 = none).
pub fn set_hide(chr:usize,want:i32){
 let now=active_hide(chr);let mut applied=APPLIED.lock().unwrap();
 if want==now{return;}
 if *applied!=0&&*applied!=want{let old=*applied;remove(chr,old);*applied=0;crate::log_game(&format!("HIDE_WEAPON: removed effect {old}"));}
 if want==0{return;}
 let ok=unsafe{SoloParamRepository::instance()}.ok().and_then(|r|r.get::<SpEffectParam>(want as u32)).is_some_and(|p|p.state_info()==HIDE_WEAPON_STATE&&pure_visual(p));
 if !ok{static WARN:std::sync::Once=std::sync::Once::new();WARN.call_once(||crate::log_game(&format!("HIDE_WEAPON: effect {want} is not a pure hide-weapon effect; not applied")));return;}
 apply(chr,want);*applied=want;crate::log_game(&format!("HIDE_WEAPON: applied effect {want}"));}
/// Takes back an effect we applied (call when the replay ends).
pub fn clear_hide(chr:usize){let mut applied=APPLIED.lock().unwrap();if *applied!=0{let id=*applied;remove(chr,id);*applied=0;crate::log_game(&format!("HIDE_WEAPON: removed effect {id} at the end of the replay"));}}
/// The game\'s own Spectral Steed Whistle (goods 130): applies the special effect that item uses. Only if that effect
/// changes no HP / FP / stamina. Returns what happened for the log.
pub fn summon_torrent(chr:usize)->Result<i32,String>{
 let repo=unsafe{SoloParamRepository::instance()}.map_err(|_|"param repository not available".to_string())?;
 let goods=repo.get::<EquipParamGoods>(130).ok_or("goods 130 (Spectral Steed Whistle) not found")?;
 let id=goods.ref_id_default();if id<=0{return Err(format!("whistle references special effect {id}"));}
 let p=repo.get::<SpEffectParam>(id as u32).ok_or_else(||format!("whistle special effect {id} not found"))?;
 crate::log_game(&format!("TORRENT_WHISTLE: goods 130 -> special effect {id} (state info {}, change hp {}/{} mp {}/{} stamina {}/{}, endurance {})",p.state_info(),p.change_hp_rate(),p.change_hp_point(),p.change_mp_rate(),p.change_mp_point(),p.change_stamina_rate(),p.change_stamina_point(),p.effect_endurance()));
 if !pure_visual(p){return Err(format!("special effect {id} changes HP/FP/stamina; not applied"));}
 apply(chr,id);Ok(id)}

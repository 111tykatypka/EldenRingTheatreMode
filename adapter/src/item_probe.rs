//! Research probe (read-only, log only): what changes on the player while an item such as a flask is used.
//! Logs the item the animation queued for use, the three item sfx ids the game keeps, and the set of active
//! special effects (with the visual-effect ids of each new one). Nothing here writes to the game; it exists to
//! find out which state a replay would have to reproduce for the flask model and its effects.
use eldenring::cs::ChrIns;
use std::mem::offset_of;

#[derive(Default)]
pub struct Probe{window:Option<Window>,item:Option<u32>,sfx:Option<[i32;3]>,effects:Option<Vec<i32>>,loc:Option<[u8;crate::weapon_loc::BYTES]>}
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
 if let Some(a)=ptr(offset_of!(ChrIns,chr_model_ins)){v.push(("ModelIns".to_string(),a,0x300));}
 v.extend(asm_children(chr));v}
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
   if !diffs.is_empty(){crate::log_game(&format!("ITEM_DIFF t={seconds:.2}s {name}: {}",diffs.join(" ")));}}
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

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
struct Window{until:f64,next:f64,regions:Vec<(&'static str,usize,Vec<u8>)>}
fn region_list(chr:usize)->Vec<(&'static str,usize,usize)>{
 let mut v=vec![("ChrIns",chr,0x500usize)];
 let ptr=|o:usize|crate::companions::word(chr+o).filter(|p|*p>0x10000);
 if let Some(a)=ptr(offset_of!(eldenring::cs::PlayerIns,chr_asm)){v.push(("ChrAsm",a,std::mem::size_of::<eldenring::cs::ChrAsm>()));}
 if let Some(a)=crate::weapon_loc::module(chr){v.push(("ActionFlag",a,0x258));}
 if let Some(a)=ptr(offset_of!(ChrIns,chr_model_ins)){v.push(("ModelIns",a,0x300));}
 v}
fn snapshot(chr:usize)->Vec<(&'static str,usize,Vec<u8>)>{
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
  if let Some(p)=e.param_data{let p=unsafe{p.as_ref()};return format!(" vfx[{},{},{},{}]",p.vfx_id(),p.vfx_id1(),p.vfx_id2(),p.vfx_id3());}}}
 String::new()}

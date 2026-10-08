//! Research probe (read-only, log only): what changes on the player while an item such as a flask is used.
//! Logs the item the animation queued for use, the three item sfx ids the game keeps, and the set of active
//! special effects (with the visual-effect ids of each new one). Nothing here writes to the game; it exists to
//! find out which state a replay would have to reproduce for the flask model and its effects.
use eldenring::cs::ChrIns;
use std::mem::offset_of;

#[derive(Default)]
pub struct Probe{item:Option<u32>,sfx:Option<[i32;3]>,effects:Option<Vec<i32>>,loc:Option<[u8;crate::weapon_loc::BYTES]>}
impl Probe{
 /// Call once per recorded frame; `seconds` is time since recording start for the log lines.
 pub fn sample(&mut self,chr:usize,seconds:f64){
  let item=crate::companions::dword(chr+offset_of!(ChrIns,tae_queued_use_item));
  let sfx=[offset_of!(ChrIns,item_use_cast_sfx_id),offset_of!(ChrIns,item_use_fire_sfx_id),offset_of!(ChrIns,item_use_effect_sfx_id)].map(|o|crate::companions::dword(chr+o));
  let sfx=match sfx{[Some(a),Some(b),Some(c)]=>Some([a as i32,b as i32,c as i32]),_=>None};
  if item!=self.item||sfx!=self.sfx{
   crate::log_game(&format!("ITEM_PROBE t={seconds:.2}s: queued_use_item={} sfx cast/fire/effect={:?}",item.map(|v|format!("0x{v:08X}")).unwrap_or("?".into()),sfx));
   self.item=item;self.sfx=sfx;}
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

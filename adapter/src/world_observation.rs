//! Read-only world/warp prerequisite observation using the pinned reflected singleton.
//! Never use the ambiguous legacy CSLuaEventManager signature or invoke warp.
use eldenring::cs::CSLuaEventManImp;
use fromsoftware_shared::FromStatic;
#[derive(Default)]pub struct Capture{next:u64}
impl Capture{
 pub fn tick(&mut self,now:u64){
  if now<self.next||!crate::grounding::active(now){return;}self.next=now.saturating_add(100_000_000);
  match unsafe{CSLuaEventManImp::instance()}{
   Ok(m)=>{let s=m.lua_event_script_imitation.as_ref();crate::grounding::world(now,true,s.map(|v|v.lua_warp_bonfire_entity_id),s.map(|v|v.is_wait_reentry_to_map),Some(m.lua_event_proxy.is_load_wait));},
   Err(_)=>crate::grounding::world(now,false,None,None,None),
  }
 }
}
#[cfg(test)]mod tests{
 use eldenring::cs::{CSLuaEventManImp,CSLuaEventScriptImitation,CSChrBehaviorModule,ChrIns,ChrCtrl};
 #[test]fn typed_layout_crosschecks_reference_offsets(){
  assert_eq!(std::mem::offset_of!(CSLuaEventManImp,lua_event_proxy),8);
  assert_eq!(std::mem::offset_of!(CSLuaEventManImp,lua_event_script_imitation),0x18);
  assert_eq!(std::mem::offset_of!(CSLuaEventScriptImitation,lua_warp_bonfire_entity_id),0x1c);
  assert_eq!(std::mem::offset_of!(CSChrBehaviorModule,animation_speed),0x17c8);
  assert_eq!(std::mem::offset_of!(ChrIns,debug_flags),0x530); // SDK declaration only; exact native debug field is +538, see native_debug_flags.
  assert_eq!(std::mem::offset_of!(ChrCtrl,chr_proxy_flags),0xfc);
 }
}

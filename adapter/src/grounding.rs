//! Diagnostic sampling only: no guessed vertical correction or proxy writes.
use eldenring::cs::PlayerIns;
#[derive(Default)]pub struct Capture{next:u64}
impl Capture{
 pub fn tick(&mut self,now:u64){if now<self.next{return;}self.next=now+1_000_000_000;
  if let Ok(p)=unsafe{PlayerIns::local_player()}{let chr=&p.chr_ins;let physics=&chr.modules.physics;let ctrl=&chr.chr_ctrl;
   crate::log_game(&format!("GROUNDING timestamp_ns={now} phase=PostPhysics replay_active={} physics_y={} last_update_y={} physics_model_y={} model_y={} vertical_offset={} solid_ground={} touching={} falling={} block={} proxy_flags={:?} proxy_y=UNAVAILABLE ground_y=UNAVAILABLE; no Y correction",crate::replay_runtime::active(),physics.position.1,physics.last_update_position.1,ctrl.physics_model_matrix.3.1,ctrl.model_matrix.3.1,ctrl.vertical_position_offset,physics.standing_on_solid_ground,physics.touching_solid_ground,physics.is_falling,i32::from(chr.block_id),ctrl.chr_proxy_flags));
  }
 }
}

//! Explicit bounded scene-anchored return, read checks before one transform write.
use crate::{control_protocol::Packet,transform_probe::{Transform,distance}};
use eldenring::cs::{WorldChrMan,FieldInsHandle,FieldInsSelector,BlockId};
use fromsoftware_shared::FromStatic;
pub fn bounded(live:Transform,target:Transform,expected:[f32;3],actual:[f32;3],same_block:bool)->bool{
 same_block&&live.valid()&&target.valid()&&expected.iter().chain(actual.iter()).all(|v|v.is_finite())&&distance(live,target)<=20.0&&
 (live.position[1]-target.position[1]).abs()<=2.0&&expected.iter().zip(actual).map(|(a,b)|f64::from(*a-b).powi(2)).sum::<f64>()<=0.0625&&
 target.position.iter().zip(expected).map(|(a,b)|f64::from(*a-b).powi(2)).sum::<f64>()<=900.0
}
pub fn validate(packet:Packet)->Result<(), &'static str>{
 let world=unsafe{WorldChrMan::instance()}.map_err(|_|"WORLD_UNAVAILABLE")?;
 let player=world.main_player.as_ref().ok_or("PLAYER_UNAVAILABLE")?;
 let handle=FieldInsHandle{selector:FieldInsSelector(packet.applied_sequence as u32),block_id:BlockId::from((packet.applied_sequence>>32) as i32)};
 let anchor=world.chr_ins_by_handle(&handle).ok_or("RECORDED_ANCHOR_NOT_LOADED")?;
 if anchor.event_entity_id!=packet.detail||anchor.npc_param_id!=packet.replay_detail as i32||anchor.chr_type as u32!=packet.state||anchor.block_id!=handle.block_id{return Err("ANCHOR_IDENTITY_MISMATCH");}
 let p=&player.chr_ins.modules.physics;let a=&anchor.modules.physics;
 let live=Transform{position:[p.position.0,p.position.1,p.position.2],quaternion:[p.orientation.0,p.orientation.1,p.orientation.2,p.orientation.3]};
 let target=Transform{position:packet.position,quaternion:packet.quaternion};let expected=[packet.player_action.animation_time,packet.player_action.animation_length,packet.player_action.playback_rate];
 if !bounded(live,target,expected,[a.position.0,a.position.1,a.position.2],player.chr_ins.block_id==anchor.block_id){return Err("SCENE_OR_ANCHOR_OR_DISPLACEMENT_MISMATCH");}
 crate::log_game(&format!("REPLAY_RETURN_SCENE_OK entity={} block={} displacement={:.4} target={:?}; one explicit write; scene inference requires live validation",packet.detail,i32::from(anchor.block_id),distance(live,target),target));Ok(())
}
#[cfg(test)]mod tests{use super::*;#[test]fn return_checks_scene_anchor_and_bounds(){let t=Transform{position:[0.;3],quaternion:[0.,0.,0.,1.]};let live=Transform{position:[10.,0.,0.],..t};assert!(bounded(live,t,[1.,0.,0.],[1.,0.,0.],true));assert!(!bounded(live,t,[1.,0.,0.],[1.,0.,0.],false));assert!(!bounded(live,t,[1.,0.,0.],[2.,0.,0.],true));assert!(!bounded(Transform{position:[21.,0.,0.],..t},t,[1.,0.,0.],[1.,0.,0.],true));assert!(!bounded(Transform{position:[0.,3.,0.],..t},t,[1.,0.,0.],[1.,0.,0.],true));assert!(!bounded(live,t,[f32::NAN,0.,0.],[1.,0.,0.],true));}}

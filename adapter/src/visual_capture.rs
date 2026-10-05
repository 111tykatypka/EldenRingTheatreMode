//! Bounded, explicitly serialized public SDK fields. Read-only, no native codec calls.
use eldenring::cs::{ChrIns,PlayerIns};
#[derive(Clone,Copy,PartialEq)]
pub struct Visual {pub id:u64,pub time:u64,pub fields:[u32;12],pub equipment:[i32;22],pub face:[u8;288]}
pub fn actor(id:u64,time:u64,chr:&ChrIns)->Visual{
 let data=&chr.modules.data;let p=&chr.modules.physics;
 let health=data.hp>=0&&data.max_hp>=0;
 let mut fields=[0;12];fields[0]=1;fields[1]=chr.character_id;fields[2]=data.hp as u32;fields[3]=data.max_hp as u32;fields[4]=1|4|if health{2}else{0};fields[10]=chr.item_use_effect_sfx_id as u32;
 fields[11]=(p.standing_on_solid_ground as u32)|((p.is_falling as u32)<<1)|((p.touching_solid_ground as u32)<<2);
 Visual{id,time,fields,equipment:[0;22],face:[0;288]}
}
pub fn player(time:u64,p:&PlayerIns)->Visual{
 let mut s=actor(0,time,&p.chr_ins);let asm=&p.chr_asm;let slots=&asm.equipment.selected_slots;
 s.equipment=asm.equipment_param_ids;s.fields[5]=slots.left_weapon_slot;s.fields[6]=slots.right_weapon_slot;s.fields[7]=asm.equipment.arm_style as u32;
 if s.fields[5]<3&&s.fields[6]<3&&s.fields[7]<=3{s.fields[4]|=8;}
 let data=unsafe{p.player_game_data.as_ref()};s.fields[8]=data.gender as u32;s.fields[9]=data.archetype as u32;
 let face=&data.face_data.face_data_buffer;
 s.face[..4].copy_from_slice(&face.magic);s.face[4..8].copy_from_slice(&face.version.to_le_bytes());s.face[8..12].copy_from_slice(&face.buffer_size.to_le_bytes());s.face[12..].copy_from_slice(&face.buffer);
 if face.buffer_size<=288&&face.magic!=[0;4]{s.fields[4]|=16;}
 s
}
impl Visual {pub fn encode(self)->[u8;440]{let mut b=[0;440];b[..8].copy_from_slice(&self.id.to_le_bytes());b[8..16].copy_from_slice(&self.time.to_le_bytes());for(i,v)in self.fields.into_iter().enumerate(){b[16+i*4..20+i*4].copy_from_slice(&v.to_le_bytes());}for(i,v)in self.equipment.into_iter().enumerate(){b[64+i*4..68+i*4].copy_from_slice(&v.to_le_bytes());}b[152..].copy_from_slice(&self.face);b}}
#[cfg(test)]mod tests{use super::*;#[test]fn visual_wire_is_explicit_and_pointer_free(){let mut s=Visual{id:42,time:100,fields:[0;12],equipment:[0;22],face:[0;288]};s.fields[0]=1;s.equipment[0]=-1;s.face[287]=255;let b=s.encode();assert_eq!(b.len(),440);assert_eq!(u64::from_le_bytes(b[..8].try_into().unwrap()),42);assert_eq!(&b[64..68],&(-1i32).to_le_bytes());assert_eq!(b[439],255);}}

//! Equipment, weapon slot and grip state for replays (Phase 1.5).
//!
//! The player's render assembly (PlayerIns.chr_asm, "how the character is rendered") holds the arm
//! style (empty / one-handed / two-handed left or right), the active weapon, arrow and bolt slots, the
//! inventory handles and the param IDs of all 22 equipment pieces. A recording keeps it every frame;
//! playback writes the recorded assembly before the pose each frame and puts the player's own back
//! afterwards. Seeking backwards across a change therefore reverts it, because each frame carries
//! the full state (no deltas to undo).
//!
//! Read as raw u32/i32 through SDK field offsets, never as Rust enums, and only written when the
//! values look like a real assembly. Whether the game keeps the written assembly until drawing, or
//! rebuilds it from the inventory, is measured during playback and logged.
use eldenring::cs::{ChrAsm,ChrAsmEquipment,PlayerIns};
use std::mem::offset_of;

pub const SLOTS:usize=22;
pub const BYTES:usize=4+6*4+SLOTS*4*2; // arm style, 6 selected slots, handles, param ids

#[derive(Clone,Copy,Debug,PartialEq,Eq)]
pub struct Equip{pub arm_style:u32,pub slots:[u32;6],pub handles:[u32;SLOTS],pub params:[i32;SLOTS]}
impl Default for Equip{fn default()->Self{Self{arm_style:u32::MAX,slots:[0;6],handles:[0;SLOTS],params:[-1;SLOTS]}}}
impl Equip{
 /// u32::MAX arm style marks "not recorded" (older files).
 pub fn recorded(&self)->bool{self.arm_style!=u32::MAX}
 pub fn plausible(&self)->bool{self.arm_style<=3&&self.slots[..2].iter().all(|s|*s<=2)&&self.slots[2..].iter().all(|s|*s<=1)&&self.params.iter().all(|p|*p>=-1)}
 pub fn encode(&self,out:&mut Vec<u8>){
  out.extend_from_slice(&self.arm_style.to_le_bytes());for v in self.slots{out.extend_from_slice(&v.to_le_bytes());}
  for v in self.handles{out.extend_from_slice(&v.to_le_bytes());}for v in self.params{out.extend_from_slice(&v.to_le_bytes());}}
 pub fn decode(b:&[u8])->Self{
  let u=|i:usize|u32::from_le_bytes(b[i*4..i*4+4].try_into().unwrap());
  Self{arm_style:u(0),slots:std::array::from_fn(|k|u(1+k)),handles:std::array::from_fn(|k|u(7+k)),params:std::array::from_fn(|k|u(7+SLOTS+k) as i32)}}
}
fn asm_address(player:usize)->Option<usize>{
 let p=player as *const PlayerIns;let asm=unsafe{&*(*p).chr_asm} as *const ChrAsm as usize;
 (asm>0x10000).then_some(asm)}
fn fields(asm:usize)->(usize,usize,usize,usize){
 let equipment=asm+offset_of!(ChrAsm,equipment);
 (equipment+offset_of!(ChrAsmEquipment,arm_style),equipment+offset_of!(ChrAsmEquipment,selected_slots),asm+offset_of!(ChrAsm,gaitem_handles),asm+offset_of!(ChrAsm,equipment_param_ids))}
/// `player` is the main PlayerIns (the same address as its ChrIns).
pub fn read(player:usize)->Option<Equip>{
 let asm=asm_address(player)?;let (a,s,h,p)=fields(asm);
 let e=unsafe{Equip{arm_style:std::ptr::read_volatile(a as *const u32),slots:std::ptr::read_volatile(s as *const [u32;6]),
  handles:std::ptr::read_volatile(h as *const [u32;SLOTS]),params:std::ptr::read_volatile(p as *const [i32;SLOTS])}};
 e.plausible().then_some(e)}
/// Writes a recorded assembly; refuses anything that does not look like one.
pub fn write(player:usize,e:&Equip)->bool{
 if !e.recorded()||!e.plausible(){return false;}
 let Some(asm)=asm_address(player) else {return false};
 if read(player).is_none(){return false;} // the live one must also look right (layout check)
 let (a,s,h,p)=fields(asm);
 unsafe{std::ptr::write_volatile(a as *mut u32,e.arm_style);std::ptr::write_volatile(s as *mut [u32;6],e.slots);
  std::ptr::write_volatile(h as *mut [u32;SLOTS],e.handles);std::ptr::write_volatile(p as *mut [i32;SLOTS],e.params);}
 true}

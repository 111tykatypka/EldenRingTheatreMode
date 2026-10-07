//! Prints public SDK field offsets for read-only research; never opens the game.
use eldenring::cs::{WorldChrMan,ChrIns,PlayerIns,ChrInsModuleContainer,CSChrPhysicsModule};
fn main(){
 println!("buddy={:#x} player={:#x}",std::mem::offset_of!(WorldChrMan,summon_buddy_chr_set),std::mem::offset_of!(WorldChrMan,main_player));
 println!("modules={:#x} handle={:#x} npc={:#x} model={:#x} chunk={:#x} origin={:#x}",std::mem::offset_of!(ChrIns,modules),std::mem::offset_of!(ChrIns,field_ins_handle),std::mem::offset_of!(ChrIns,npc_param_id),std::mem::offset_of!(ChrIns,character_id),std::mem::offset_of!(ChrIns,chunk_position),std::mem::offset_of!(ChrIns,block_origin));
 println!("physics={:#x} pos={:#x}",std::mem::offset_of!(ChrInsModuleContainer,physics),std::mem::offset_of!(CSChrPhysicsModule,position));
 println!("player_block={:#x} current_block={:#x}",std::mem::offset_of!(PlayerIns,block_position),std::mem::offset_of!(PlayerIns,current_block_id));
}

// Test harness: world_file + codec with stand-ins for game-facing modules.
#[path="../../adapter/src/codec.rs"] mod codec;
#[path="../../adapter/src/world_file.rs"] mod world_file;
mod arrival{#[derive(Clone,Copy,Debug,Default,PartialEq)]pub struct Place{pub block:i32,pub origin:i32,pub global:[f32;4]}}
mod equipment{pub const SLOTS:usize=22;pub const BYTES:usize=4+6*4+SLOTS*4*2;
 #[derive(Clone,Copy,Debug,PartialEq,Eq)]pub struct Equip{pub arm_style:u32,pub slots:[u32;6],pub handles:[u32;SLOTS],pub params:[i32;SLOTS]}}
mod bone_replay{pub fn status(_:&str){}}
fn log_game(m:&str){eprintln!("{m}");}
fn main(){}

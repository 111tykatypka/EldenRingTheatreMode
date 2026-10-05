//! Exact 2.7.0.0 constructor: RVA 3e7409 initializes flags at +538.
//! +530 is a callback pointer (RVA 3f8fd0), NOT debug flags. noMove/noAttack consumers cross-checked; writes only in the explicit short
//! ownership probe with live structural guard. noUpdate remains blocked.
use eldenring::cs::ChrIns;
#[repr(C)]struct ExactPrefix{prefix:[u8;0x530],callback:usize,flags:u32}
pub fn read(chr:&ChrIns)->u32{
 // ChrIns is reacquired by the caller on the native callback and the full
 // executable identity is guarded. This view stays inside the bound object.
 unsafe{std::ptr::read_volatile(&(*(chr as *const ChrIns as *const ExactPrefix)).flags)}
}
/// Strong live structural check in addition to the full disk/profile guard.
pub fn permitted(chr:&ChrIns)->bool {
 let base=unsafe{crate::GetModuleHandleW(std::ptr::null())} as usize;
 let view=chr as *const ChrIns as *const ExactPrefix;
 base!=0&&unsafe{std::ptr::read_volatile(&(*view).callback)}==base+0x3f8fd0&&chr.chr_ctrl.owner.as_ptr()==chr as *const _ as *mut _
}
/// Experimental two-second probe only. +539/noUpdate is not yet enabled.
pub fn change_owned(chr:&mut ChrIns,mask:u32,value:u32)->bool {
 if mask==0||mask&!0x30!=0||!permitted(chr){return false;}
 let view=chr as *mut ChrIns as *mut ExactPrefix;
 unsafe{let current=std::ptr::read_volatile(&(*view).flags);std::ptr::write_volatile(&mut(*view).flags,(current&!mask)|(value&mask));}true
}
#[cfg(test)]mod tests{use super::*;#[test]fn exact_view_matches_static_constructor_offset(){assert_eq!(std::mem::offset_of!(ExactPrefix,flags),0x538);assert!(std::mem::size_of::<ChrIns>()>=0x53c);}}

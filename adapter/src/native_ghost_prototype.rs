//! No world mutation here. The C++ bridge consumes one-shot commands ONLY at
//! the original local-replay callsite / original world-removal drain.
#[cfg(feature="native-replay-ghost-create-remove")]
pub fn initialize() {
    use eldenring::cs::{ChrIns, PlayerIns, ChrInsModuleContainer, CSChrPhysicsModule};
    use std::{ffi::{c_char,CStr},mem::offset_of};
    unsafe extern "C" { fn tm_native_ghost_start(log:extern "C" fn(*const c_char),layout:*const usize)->i32; }
    extern "C" fn log(message:*const c_char) {
        if !message.is_null() { crate::log_game(&unsafe{CStr::from_ptr(message)}.to_string_lossy()); }
    }
    let layout=[offset_of!(PlayerIns,replay_recorder),offset_of!(ChrIns,modules),
        offset_of!(ChrInsModuleContainer,physics),offset_of!(CSChrPhysicsModule,position),
        offset_of!(CSChrPhysicsModule,orientation),offset_of!(ChrIns,chr_type),
        offset_of!(PlayerIns,current_block_id),offset_of!(ChrIns,chr_model_ins),
        offset_of!(ChrInsModuleContainer,behavior),offset_of!(ChrInsModuleContainer,time_act)];
    let result=unsafe{tm_native_ghost_start(log,layout.as_ptr())};
    crate::log_game(&format!("NATIVE_GHOST: bridge initialization={result}; feature=native-replay-ghost-create-remove; runtime UNVERIFIED"));
}
#[cfg(not(feature="native-replay-ghost-create-remove"))]
pub fn initialize() {}

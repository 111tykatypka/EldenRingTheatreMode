//! VFX inspection and opt-in native preview run only in the existing game callback.
use eldenring::cs::CSSfxImp;
use fromsoftware_shared::FromStatic;
unsafe extern "C" {
    fn tm_particles_inspection_requested() -> i32;
    fn tm_particles_inspect(active: i32, manager: usize);
}
pub fn tick(active: bool) {
    if unsafe { tm_particles_inspection_requested() } == 0 { return; }
    let manager = if active {
        unsafe { CSSfxImp::instance() }.map(|sfx| sfx as *const CSSfxImp as usize).unwrap_or(0)
    } else { 0 };
    unsafe { tm_particles_inspect(active as i32, manager); }
}

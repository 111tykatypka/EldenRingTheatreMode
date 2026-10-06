//! Replay ghosts drawn as normal characters (no phantom transparency, glow or tint).
//!
//! The engine renders replay ghosts through PhantomParam. Once the regulation params are loaded,
//! every PhantomParam row is rewritten in memory, once per game session, to values that leave the
//! character's own materials untouched. No game file is changed and
//! there is no toggle. Runs on the game thread only.
#[cfg(feature="native-replay-ghost-create-remove")]
mod imp {
    use eldenring::cs::{PhantomParam, SoloParamRepository, WorldChrMan};
    use eldenring::param::PHANTOM_PARAM_ST;
    use fromsoftware_shared::FromStatic;
    use std::sync::atomic::{AtomicBool, Ordering};

    static APPLIED:AtomicBool=AtomicBool::new(false);

    fn neutral(row:&mut PHANTOM_PARAM_ST) {
        // blend_rate 0 means no phantom shading is blended over the normal material at all;
        // the remaining values are neutral as well in case a shader path ignores blend_rate.
        row.set_blend_rate(0.0);row.set_alpha(1.0);row.set_glow_scale(0.0);
        row.set_edge_color_a(0.0);row.set_front_color_a(0.0);row.set_light_color_a(0.0);
        row.set_is_edge_subtract(0);row.set_is_front_subtract(0);
        row.set_diff_mul_color_r(255);row.set_diff_mul_color_g(255);row.set_diff_mul_color_b(255);row.set_diff_mul_color_a(1.0);
        row.set_spec_mul_color_r(255);row.set_spec_mul_color_g(255);row.set_spec_mul_color_b(255);row.set_spec_mul_color_a(1.0);
    }

    pub fn tick() {
        if APPLIED.load(Ordering::Relaxed) { return; }
        // The SDK panics on params that are not loaded yet; a loaded main player implies they are.
        let player_ready=unsafe{WorldChrMan::instance()}.ok().map(|w|w.main_player.is_some()).unwrap_or(false);
        if !player_ready { return; }
        let Ok(repo)=(unsafe{SoloParamRepository::instance_mut()}) else { return; };
        APPLIED.store(true,Ordering::Relaxed);
        // Live test: neutralizing only the NetworkParam replay rows (910/930) left the ghost
        // transparent, so the replay ghost takes its phantom look from another row. Every
        // PhantomParam row is set to the normal look. Side effect, offline only: spirit ashes and
        // other phantoms also render as normal characters (what the owner's reference mod does).
        let mut changed=Vec::new();
        for (id,row) in repo.rows_mut::<PhantomParam>() { neutral(row); changed.push(id); }
        if changed.is_empty() { crate::log_game("GHOST_APPEARANCE_ERROR: PhantomParam has no rows; ghosts keep the default look"); return; }
        crate::log_game(&format!("GHOST_APPEARANCE: {} PhantomParam rows set to normal character look: {:?}",changed.len(),changed));
    }
}
#[cfg(feature="native-replay-ghost-create-remove")]
pub use imp::tick;
#[cfg(not(feature="native-replay-ghost-create-remove"))]
pub fn tick() {}

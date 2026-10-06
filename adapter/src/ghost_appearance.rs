//! Replay ghosts drawn as normal characters (no phantom transparency, glow or tint).
//!
//! The engine renders replay ghosts through the PhantomParam rows named by
//! NetworkParam.replay_bonfire_phantom_param_id(_for_codename) (910 and 930 on 1.17). Once the
//! regulation params are loaded, those rows are rewritten in memory, once per game session, to
//! values that leave the character's own materials untouched. No game file is changed and
//! there is no toggle. Runs on the game thread only.
#[cfg(feature="native-replay-ghost-create-remove")]
mod imp {
    use eldenring::cs::{NetworkParam, PhantomParam, SoloParamRepository, WorldChrMan};
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
        let mut ids=Vec::new();
        if let Some(network)=repo.get::<NetworkParam>(0) {
            for id in [network.replay_bonfire_phantom_param_id(),network.replay_bonfire_phantom_param_id_for_codename()] {
                if id>=0 { ids.push(id as u32); }
            }
        }
        ids.sort_unstable();ids.dedup();
        APPLIED.store(true,Ordering::Relaxed);
        if ids.is_empty() { crate::log_game("GHOST_APPEARANCE_ERROR: NetworkParam has no replay phantom rows; ghosts keep the default look"); return; }
        for id in ids {
            match repo.get_mut::<PhantomParam>(id) {
                Some(row)=>{ let before=format!("{row:?}"); neutral(row);
                    crate::log_game(&format!("GHOST_APPEARANCE: PhantomParam[{id}] set to normal character look; was {before}")); }
                None=>crate::log_game(&format!("GHOST_APPEARANCE_ERROR: PhantomParam[{id}] not found")),
            }
        }
    }
}
#[cfg(feature="native-replay-ghost-create-remove")]
pub use imp::tick;
#[cfg(not(feature="native-replay-ghost-create-remove"))]
pub fn tick() {}

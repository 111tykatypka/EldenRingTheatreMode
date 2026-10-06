//! Experimental "normal look" for the native replay ghost (F9, game thread only).
//!
//! Replay ghosts draw through PhantomParam rows (edge/front glow, tinted diffuse, alpha).
//! The owner pointed at a regulation.bin mod that removes spirit glow by editing PhantomParam.
//! This does the same thing in memory, reversibly, without touching any game file:
//! F9 rewrites the rows the replay ghost can use to neutral values and keeps the originals;
//! F9 again restores them byte for byte. Nothing is applied until F9 is pressed.
//!
//! Candidate rows: NetworkParam.replay_bonfire_phantom_param_id(_for_codename), plus
//! the ghost's own ChrIns.phantom_param_override when the C++ bridge reports one. Which row the
//! engine actually uses for a ReplayGhostIns is not statically proven; the log says what changed.
#[cfg(feature="native-replay-ghost-create-remove")]
mod imp {
    use eldenring::cs::{NetworkParam, PhantomParam, SoloParamRepository, WorldChrMan};
    use eldenring::param::PHANTOM_PARAM_ST;
    use fromsoftware_shared::FromStatic;
    use std::sync::Mutex;

    unsafe extern "C" { fn tm_native_ghost_phantom_override()->i32; }
    #[link(name="user32")]
    unsafe extern "system" {
        fn GetAsyncKeyState(key:i32)->i16;
        fn GetForegroundWindow()->isize;
        fn GetWindowThreadProcessId(window:isize,pid:*mut u32)->u32;
    }
    #[link(name="kernel32")]
    unsafe extern "system" { fn GetCurrentProcessId()->u32; }
    const VK_F9:i32=0x78;

    struct State { described:bool, key_down:bool, saved:Vec<(u32,PHANTOM_PARAM_ST)> }
    static STATE:Mutex<State>=Mutex::new(State{described:false,key_down:false,saved:Vec::new()});

    fn foreground()->bool {
        let mut pid=0u32;
        unsafe{GetWindowThreadProcessId(GetForegroundWindow(),&mut pid)};
        pid==unsafe{GetCurrentProcessId()}
    }

    fn candidates(repo:&SoloParamRepository)->Vec<u32> {
        let mut ids=Vec::new();
        if let Some(common)=repo.get::<NetworkParam>(0) {
            for id in [common.replay_bonfire_phantom_param_id(),common.replay_bonfire_phantom_param_id_for_codename()] {
                if id>=0 { ids.push(id as u32); }
            }
        }
        let ghost=unsafe{tm_native_ghost_phantom_override()};
        if ghost>=0 { ids.push(ghost as u32); }
        ids.sort_unstable();ids.dedup();
        ids
    }

    fn neutral(row:&mut PHANTOM_PARAM_ST) {
        // No rim/front glow, no light tint, untinted diffuse and specular, fully opaque.
        row.set_edge_color_a(0.0);row.set_front_color_a(0.0);row.set_light_color_a(0.0);
        row.set_glow_scale(0.0);row.set_is_edge_subtract(0);row.set_is_front_subtract(0);
        row.set_diff_mul_color_r(255);row.set_diff_mul_color_g(255);row.set_diff_mul_color_b(255);row.set_diff_mul_color_a(1.0);
        row.set_spec_mul_color_r(255);row.set_spec_mul_color_g(255);row.set_spec_mul_color_b(255);row.set_spec_mul_color_a(1.0);
        row.set_alpha(1.0);
    }

    pub fn tick() {
        // Params are only safe to read once regulation is loaded; a loaded main player implies that.
        let player_ready=unsafe{WorldChrMan::instance()}.ok().map(|w|w.main_player.is_some()).unwrap_or(false);
        if !player_ready { return; }
        let mut state=STATE.lock().unwrap_or_else(|e|e.into_inner());
        let Ok(repo)=(unsafe{SoloParamRepository::instance_mut()}) else { return; };
        if !state.described {
            state.described=true;
            let ids=candidates(repo);
            crate::log_game(&format!("GHOST_LOOK: candidate PhantomParam rows {ids:?} (replay_bonfire ids; ghost override added once a ghost exists); F9 toggles neutral look"));
            for id in ids { if let Some(row)=repo.get::<PhantomParam>(id) { crate::log_game(&format!("GHOST_LOOK: PhantomParam[{id}] original {row:?}")); } }
        }
        let down=foreground()&&(unsafe{GetAsyncKeyState(VK_F9)} as u16&0x8000)!=0;
        let pressed=down&&!state.key_down;
        state.key_down=down;
        if !pressed { return; }
        if state.saved.is_empty() {
            let ids=candidates(repo);
            for id in ids {
                if let Some(row)=repo.get_mut::<PhantomParam>(id) {
                    state.saved.push((id,row.clone()));
                    neutral(row);
                    crate::log_game(&format!("GHOST_LOOK: PhantomParam[{id}] set neutral (original saved)"));
                } else { crate::log_game(&format!("GHOST_LOOK: PhantomParam[{id}] not found; skipped")); }
            }
            if state.saved.is_empty(){crate::log_game("GHOST_LOOK: no rows changed");}
        } else {
            for (id,original) in state.saved.drain(..) {
                if let Some(row)=repo.get_mut::<PhantomParam>(id) { *row=original; crate::log_game(&format!("GHOST_LOOK: PhantomParam[{id}] restored")); }
            }
        }
    }
}
#[cfg(feature="native-replay-ghost-create-remove")]
pub use imp::tick;
#[cfg(not(feature="native-replay-ghost-create-remove"))]
pub fn tick() {}

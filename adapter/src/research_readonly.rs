//! Opt-in static-to-runtime evidence. No virtual calls, actor writes or retained pointers.
use eldenring::cs::{WorldChrMan, ReplayRecorder, ChrCtrl, PlayerIns};
use fromsoftware_shared::FromStatic;
use std::mem::{offset_of, size_of};

// Static RTTI -> COL -> vtable -> constructor, exact guarded 2.7.0.0 disk image.
// A match identifies a candidate class instance, NOT a callable ABI or frame codec.
const RECORDER_VTABLE_RVA: usize = 0x2a4aa40;
pub struct Capture { enabled: bool, next: u64, base: usize }
impl Capture {
    pub fn new() -> Self {
        let enabled = std::env::var_os("LOCALAPPDATA")
            .and_then(|p| std::fs::read_to_string(std::path::PathBuf::from(p)
                .join("EldenRingTheaterMode/Research.readonly.ini")).ok())
            .is_some_and(|s| s.lines().any(|l| l.trim() == "enabled=1"));
        let base = unsafe { crate::GetModuleHandleW(std::ptr::null()) } as usize;
        if enabled {
            crate::log_game(&format!("RESEARCH_READONLY=ON pinned=3c8c1d7 profile=EldenRing_1_17 base=0x{base:X}; observation only, 1Hz; NO ENGINE CALLS OR WRITES"));
            crate::log_game(&format!("RESEARCH_LAYOUT world_main=0x{:X} distance_vector=0x{:X} priority_vector=0x{:X} recorder_prefix_size=0x{:X} recorder_owner=0x{:X} recorder_frame=0x{:X} proxy_flags=0x{:X}; SDK layout evidence only",offset_of!(WorldChrMan,main_player),offset_of!(WorldChrMan,chr_inses_by_distance),offset_of!(WorldChrMan,chr_inses_by_update_priority),size_of::<ReplayRecorder>(),offset_of!(ReplayRecorder,owning_player),offset_of!(ReplayRecorder,frame_counter),offset_of!(ChrCtrl,chr_proxy_flags)));
        }
        Self { enabled, next: 0, base }
    }
    pub fn tick(&mut self, now: u64) {
        if !self.enabled || now < self.next { return; }
        self.next=now.saturating_add(1_000_000_000);
        let Ok(world)=(unsafe{WorldChrMan::instance()}) else {
            crate::log_game("RESEARCH_ENUM world=UNAVAILABLE");return;
        };
        crate::log_game(&format!("RESEARCH_ENUM timestamp_ns={now} distance_len={} priority_len={} player_capacity={} ghost_capacity={} chr_set_holder_count={}; counts are collection observations, NOT active/spawn counts",world.chr_inses_by_distance.len(),world.chr_inses_by_update_priority.len(),world.player_chr_set.capacity,world.ghost_chr_set.capacity,world.chr_set_holder_count));
        let Some(player)=world.main_player.as_ref() else {
            crate::log_game("RESEARCH_RECORDER player=UNAVAILABLE");return;
        };
        let Some(recorder)=player.replay_recorder.as_ref() else {
            crate::log_game("RESEARCH_RECORDER present=false");return;
        };
        let rva=recorder.vftable.checked_sub(self.base);
        if self.base==0 || rva!=Some(RECORDER_VTABLE_RVA) {
            crate::log_game(&format!("RESEARCH_RECORDER class=MISMATCH observed_rva={rva:?}; counters not read"));return;
        }
        let owner_matches=std::ptr::eq(recorder.owning_player.as_ptr() as *const PlayerIns,&**player as *const PlayerIns);
        crate::log_game(&format!("RESEARCH_RECORDER class=VTABLE_MATCH owner_matches={owner_matches} max_frame_rate={} frame_counter={} frame_duration={} oldest=({},{},{}) rotation={} block={}; codec=UNKNOWN, runtime behavior=UNVERIFIED",recorder.max_frame_rate,recorder.frame_counter,recorder.frame_duration,recorder.position.0,recorder.position.1,recorder.position.2,recorder.rotation,i32::from(recorder.block_id)));
    }
}

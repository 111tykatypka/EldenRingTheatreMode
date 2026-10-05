use eldenring::cs::{FieldInsHandle, PlayerIns};

/// Only the two documented local ChrDebugFlags. No device/global input patches.
#[derive(Default)]
pub struct LocalInputLock { saved:Option<(FieldInsHandle,bool,bool)> }
impl LocalInputLock {
    pub fn apply(&mut self, player:&mut PlayerIns) {
        let chr=&mut player.chr_ins;
        if self.saved.is_none(){self.saved=Some((chr.field_ins_handle,chr.debug_flags.disabled_movement(),chr.debug_flags.disabled_secondary_actions()));crate::log_game("REPLAY_INPUT_LOCK=ON; local ChrDebugFlags movement/secondary only");}
        chr.debug_flags.set_disabled_movement(true);chr.debug_flags.set_disabled_secondary_actions(true);
    }
    pub fn restore(&mut self) {
        let Some((handle,movement,secondary))=self.saved.take() else{return;};
        // Reacquire, never dereference a saved pointer; restore only the same handle.
        if let Ok(player)=unsafe{PlayerIns::local_player_mut()} {if player.chr_ins.field_ins_handle==handle {
            player.chr_ins.debug_flags.set_disabled_movement(movement);player.chr_ins.debug_flags.set_disabled_secondary_actions(secondary);
            crate::log_game("REPLAY_INPUT_LOCK=RESTORED; owned bits only");return;
        }}
        crate::log_game("REPLAY_INPUT_LOCK=OWNER_GONE; no stale pointer/new-player writes");
    }
    pub fn discard_lost_owner(&mut self){self.saved=None;}
}

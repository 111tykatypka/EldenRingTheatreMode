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
    pub fn same_owner(&self,player:&PlayerIns)->bool{self.saved.as_ref().is_none_or(|(handle,_,_)|*handle==player.chr_ins.field_ins_handle)}
    pub fn restore_player(&mut self,player:&mut PlayerIns){if let Some((handle,movement,secondary))=self.saved.take(){if player.chr_ins.field_ins_handle==handle{player.chr_ins.debug_flags.set_disabled_movement(movement);player.chr_ins.debug_flags.set_disabled_secondary_actions(secondary);crate::log_game("REPLAY_INPUT_LOCK=RESTORED; owned bits only");}}}
    pub fn restore(&mut self) {
        if self.saved.is_none(){return;}
        if let Ok(player)=unsafe{PlayerIns::local_player_mut()}{self.restore_player(player);return;}
        // Retain handle + original bits through loading; retry on a later callback. Never retain a pointer.
    }
    pub fn discard_lost_owner(&mut self){self.saved=None;}
}

#[cfg(test)]mod tests{use eldenring::cs::ChrDebugFlags;
#[test]fn restore_owned_flags_preserves_other_bits(){for movement in [false,true]{for secondary in [false,true]{let mut f=ChrDebugFlags(0x80000000);f.set_disabled_movement(movement);f.set_disabled_secondary_actions(secondary);let before=f.0;f.set_disabled_movement(true);f.set_disabled_secondary_actions(true);f.0|=1;f.set_disabled_movement(movement);f.set_disabled_secondary_actions(secondary);assert_eq!(f.0,before|1);}}}}

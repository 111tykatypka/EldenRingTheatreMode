use eldenring::cs::{FieldInsHandle, PlayerIns};
use std::sync::atomic::{AtomicU64,Ordering};
static OWNED_HANDLE:AtomicU64=AtomicU64::new(0);
static LAST_WRITE:AtomicU64=AtomicU64::new(0);
const ACTION_MASK:u64=(1u64<<35)-1;
fn handle(p:&PlayerIns)->u64{p.chr_ins.field_ins_handle.selector.0 as u64|((i32::from(p.chr_ins.field_ins_handle.block_id)as u32 as u64)<<32)}

/// Local documented flags and owned action-mask bits. No device/global patches.
#[derive(Default)]
pub struct LocalInputLock { saved:Option<(FieldInsHandle,bool,bool,u64)> }
impl LocalInputLock {
    pub fn apply(&mut self, player:&mut PlayerIns) {
        let chr=&mut player.chr_ins;
        if self.saved.is_none(){self.saved=Some((chr.field_ins_handle,false,false,chr.modules.action_request.disabled_action_inputs.0));crate::log_game("REPLAY_INPUT_LOCK=ON; EXPERIMENTAL normalized action masking; debug flags BLOCKED (layout conflict); earlier PreBehaviorSafe callback");}
        // Debug flag writes blocked: pinned SDK 0x530 conflicts with Freecam reference 0x538.
        chr.modules.action_request.disabled_action_inputs.0|=ACTION_MASK;
        OWNED_HANDLE.store(handle(player),Ordering::Release);LAST_WRITE.store(crate::monotonic_ns(),Ordering::Release);
    }
    pub fn same_owner(&self,player:&PlayerIns)->bool{self.saved.as_ref().is_none_or(|(handle,_,_,_)|*handle==player.chr_ins.field_ins_handle)}
    pub fn restore_player(&mut self,player:&mut PlayerIns){if let Some((handle,_movement,_secondary,disabled))=self.saved.take(){LAST_WRITE.store(0,Ordering::Release);if player.chr_ins.field_ins_handle==handle{let mask=&mut player.chr_ins.modules.action_request.disabled_action_inputs.0;*mask=(*mask&!ACTION_MASK)|(disabled&ACTION_MASK);crate::log_game("REPLAY_INPUT_LOCK=RESTORED; owned bits only");}}}
    pub fn restore(&mut self) {
        if self.saved.is_none(){return;}
        if let Ok(player)=unsafe{PlayerIns::local_player_mut()}{self.restore_player(player);return;}
        // Retain handle + original bits through loading; retry on a later callback. Never retain a pointer.
    }
    pub fn discard_lost_owner(&mut self){self.saved=None;LAST_WRITE.store(0,Ordering::Release);}
}

#[cfg(test)]mod tests{use eldenring::cs::ChrDebugFlags;
#[test]fn restore_owned_flags_preserves_other_bits(){for movement in [false,true]{for secondary in [false,true]{let mut f=ChrDebugFlags(0x80000000);f.set_disabled_movement(movement);f.set_disabled_secondary_actions(secondary);let before=f.0;f.set_disabled_movement(true);f.set_disabled_secondary_actions(true);f.0|=1;f.set_disabled_movement(movement);f.set_disabled_secondary_actions(secondary);assert_eq!(f.0,before|1);}}}}

/// Experimental producer-stage neutralization. No OS device interception.
pub fn early_tick(now:u64){
 let last=LAST_WRITE.load(Ordering::Acquire);if last==0||now.saturating_sub(last)>250_000_000||!crate::replay_runtime::player_writes_enabled()||!crate::replay_runtime::input_owned(now){return;}
 let Ok(p)=(unsafe{PlayerIns::local_player_mut()})else{return;};if handle(p)!=OWNED_HANDLE.load(Ordering::Acquire){return;}
 if !neutralize(&mut p.chr_ins){crate::replay_runtime::stop(6);}
}
pub fn neutralize(chr:&mut eldenring::cs::ChrIns)->bool{
 let r=&mut chr.modules.action_request;
 if r.action_request_queue.input_entries.len()>256||r.action_request_queue.cancel_entries.len()>256{return false;}
 r.action_requests.0=0;r.previous_action_requests.0=0;r.new_action_presses.0=0;r.released_actions.0=0;r.cancel_ready_actions.0=0;r.queued_action_inputs.0=0;
 r.readback_new_presses.0=0;r.readback_cancel_ready.0=0;r.readback_queued_inputs.0=0;
 r.movement_request_duration=0.0;r.movement_request_flags.0&=!7;
 for e in r.action_request_queue.input_entries.iter_mut(){e.actions.0&=!((1u64<<38)-1);}
 for e in r.action_request_queue.cancel_entries.iter_mut(){e.actions.0&=!((1u64<<38)-1);}
 // Analog manipulator vectors are private/unavailable; don't claim they were cleared.
 true
}

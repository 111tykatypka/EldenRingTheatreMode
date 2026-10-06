# Player fidelity capture field catalog — schema 1

Offsets below are compiler-derived from the exact pinned SDK, not independent proof of the runtime game layout. New reads use ReadProcessMemory and module-owner validation; semantics require live correlation. No raw pointers are serialized. All tracks sample every available ChrIns_PostPhysics callback (target about 60 Hz, measured rate displayed). All records are continuous snapshots, not sparse semantic labels.

| Track | Field | Source structure/path | SDK offset | Representation | Confidence |
|---|---|---|---|---|---|
| PhysicsTrack | `position` | `CSChrPhysicsModule.position` | `0x70` | f32, 4 words | RUNTIME_READ_VERIFIED |
| PhysicsTrack | `last_update_position` | `CSChrPhysicsModule.last_update_position` | `0x80` | f32, 4 words | REFERENCE |
| PhysicsTrack | `interpolated_orientation` | `CSChrPhysicsModule.interpolated_orientation` | `0x60` | f32, 4 words | REFERENCE |
| PhysicsTrack | `additional_rotation` | `CSChrPhysicsModule.additional_rotation` | `0x190` | f32, 4 words | REFERENCE |
| PhysicsTrack | `orientation_euler` | `CSChrPhysicsModule.orientation_euler` | `0x2D0` | f32, 4 words | REFERENCE |
| PhysicsTrack | `rotation_multiplier` | `CSChrPhysicsModule.rotation_multiplier` | `0x1C0` | f32, 1 words | REFERENCE |
| PhysicsTrack | `motion_multiplier` | `CSChrPhysicsModule.motion_multiplier` | `0x1C4` | f32, 1 words | REFERENCE |
| PhysicsTrack | `gravity_multiplier` | `CSChrPhysicsModule.gravity_multiplier` | `0x1CC` | f32, 1 words | REFERENCE |
| PhysicsTrack | `chr_push_up_factor` | `CSChrPhysicsModule.chr_push_up_factor` | `0x104` | f32, 1 words | REFERENCE |
| PhysicsTrack | `default_max_turn_rate` | `CSChrPhysicsModule.default_max_turn_rate` | `0x314` | f32, 1 words | REFERENCE |
| PhysicsTrack | `hit_height` | `CSChrPhysicsModule.hit_height` | `0x2F0` | f32, 1 words | REFERENCE |
| PhysicsTrack | `hit_radius` | `CSChrPhysicsModule.hit_radius` | `0x2F4` | f32, 1 words | REFERENCE |
| PhysicsTrack | `weight` | `CSChrPhysicsModule.weight` | `0x300` | f32, 1 words | REFERENCE |
| PhysicsTrack | `chr_proxy_pos_update_requested` | `CSChrPhysicsModule.chr_proxy_pos_update_requested` | `0x91` | u8, 1 words | REFERENCE |
| PhysicsTrack | `standing_on_solid_ground` | `CSChrPhysicsModule.standing_on_solid_ground` | `0x92` | u8, 1 words | REFERENCE |
| PhysicsTrack | `touching_solid_ground` | `CSChrPhysicsModule.touching_solid_ground` | `0x93` | u8, 1 words | REFERENCE |
| PhysicsTrack | `is_falling` | `CSChrPhysicsModule.is_falling` | `0x1D0` | u8, 1 words | REFERENCE |
| PhysicsTrack | `is_touching_ground` | `CSChrPhysicsModule.is_touching_ground` | `0x1D1` | u8, 1 words | REFERENCE |
| PhysicsTrack | `gravity_disabled` | `CSChrPhysicsModule.gravity_disabled` | `0x1D5` | u8, 1 words | REFERENCE |
| PhysicsTrack | `fade_out_gravity_disabled` | `CSChrPhysicsModule.fade_out_gravity_disabled` | `0x1D3` | u8, 1 words | REFERENCE |
| PhysicsTrack | `flying_character_fall_requested` | `CSChrPhysicsModule.flying_character_fall_requested` | `0x1D9` | u8, 1 words | REFERENCE |
| PhysicsTrack | `use_world_y_alignment_logic` | `CSChrPhysicsModule.use_world_y_alignment_logic` | `0x1DB` | u8, 1 words | REFERENCE |
| PhysicsTrack | `is_surface_constrained` | `CSChrPhysicsModule.is_surface_constrained` | `0x1DC` | u8, 1 words | REFERENCE |
| PhysicsTrack | `adjust_to_hi_collision` | `CSChrPhysicsModule.adjust_to_hi_collision` | `0xCC` | u8, 1 words | REFERENCE |
| PhysicsTrack | `move_type_flags` | `CSChrPhysicsModule.move_type_flags` | `0x320` | u8, 1 words | REFERENCE |
| PhysicsTrack | `physics_model_matrix` | `ChrCtrl.physics_model_matrix` | `0x1B0` | f32, 16 words | REFERENCE |
| PhysicsTrack | `model_matrix` | `ChrCtrl.model_matrix` | `0x230` | f32, 16 words | REFERENCE |
| PhysicsTrack | `additional_orientation_quat` | `ChrCtrl.additional_orientation_quat` | `0x2C0` | f32, 4 words | REFERENCE |
| PhysicsTrack | `vertical_position_offset` | `ChrCtrl.vertical_position_offset` | `0x2D0` | f32, 1 words | REFERENCE |
| PhysicsTrack | `scale_size_x` | `ChrCtrl.scale_size_x` | `0x2D4` | f32, 1 words | REFERENCE |
| PhysicsTrack | `scale_size_y` | `ChrCtrl.scale_size_y` | `0x2D8` | f32, 1 words | REFERENCE |
| PhysicsTrack | `scale_size_z` | `ChrCtrl.scale_size_z` | `0x2DC` | f32, 1 words | REFERENCE |
| PhysicsTrack | `offset_y` | `ChrCtrl.offset_y` | `0x2E0` | f32, 1 words | REFERENCE |
| PhysicsTrack | `ragdoll_revive_time` | `ChrCtrl.ragdoll_revive_time` | `0x12C` | f32, 1 words | REFERENCE |
| PhysicsTrack | `foot_ik_error_height_limit` | `ChrCtrl.foot_ik_error_height_limit` | `0x308` | f32, 1 words | REFERENCE |
| PhysicsTrack | `disable_move` | `ChrCtrl.disable_move` | `0xE9` | u8, 1 words | REFERENCE |
| PhysicsTrack | `height_correction_request` | `ChrCtrl.height_correction_request` | `0x301` | u8, 1 words | REFERENCE |
| PhysicsTrack | `chr_ragdoll_state` | `ChrCtrl.chr_ragdoll_state` | `0x128` | u8, 1 words | REFERENCE |
| PhysicsTrack | `flags` | `ChrCtrl.flags` | `0xF0` | u32, 1 words | REFERENCE |
| PhysicsTrack | `flags_copy` | `ChrCtrl.flags_copy` | `0xF4` | u32, 1 words | REFERENCE |
| PhysicsTrack | `chr_proxy_flags` | `ChrCtrl.chr_proxy_flags` | `0xFC` | u32, 1 words | REFERENCE |
| PhysicsTrack | `fall_timer` | `CSChrFallModule.fall_timer` | `0x18` | f32, 1 words | REFERENCE |
| PhysicsTrack | `force_max_fall_height` | `CSChrFallModule.force_max_fall_height` | `0x1D` | u8, 1 words | REFERENCE |
| PhysicsTrack | `disable_fall_motion` | `CSChrFallModule.disable_fall_motion` | `0x1E` | u8, 1 words | REFERENCE |
| PhysicsTrack | `chr_hit_height` | `CSChrPhysicsModule.chr_hit_height` | `0x2E0` | f32, 1 words | REFERENCE |
| PhysicsTrack | `chr_hit_radius` | `CSChrPhysicsModule.chr_hit_radius` | `0x2E4` | f32, 1 words | REFERENCE |
| PhysicsTrack | `step_disp_interpolate_time` | `CSChrPhysicsModule.step_disp_interpolate_time` | `0x3E8` | f32, 1 words | REFERENCE |
| PhysicsTrack | `step_disp_interpolate_trigger_value` | `CSChrPhysicsModule.step_disp_interpolate_trigger_value` | `0x3EC` | f32, 1 words | REFERENCE |
| PhysicsTrack | `is_enable_step_disp_interpolate` | `CSChrPhysicsModule.is_enable_step_disp_interpolate` | `0x3E6` | u8, 1 words | REFERENCE |
| PhysicsTrack | `is_watcher_stones` | `CSChrPhysicsModule.is_watcher_stones` | `0x1E1` | u8, 1 words | REFERENCE |
| PhysicsTrack | `physics_transform_matrix_squared` | `ChrCtrl.physics_transform_matrix_squared` | `0x1F0` | f32, 16 words | REFERENCE |
| PhysicsTrack | `model_matrix_squared` | `ChrCtrl.model_matrix_squared` | `0x270` | f32, 16 words | REFERENCE |
| PhysicsTrack | `foot_ik_error_on_gain` | `ChrCtrl.foot_ik_error_on_gain` | `0x30C` | f32, 1 words | REFERENCE |
| PhysicsTrack | `foot_ik_error_off_gain` | `ChrCtrl.foot_ik_error_off_gain` | `0x310` | f32, 1 words | REFERENCE |
| PhysicsTrack | `forward_undulation_limit_radians` | `ChrCtrl.forward_undulation_limit_radians` | `0x32C` | f32, 1 words | REFERENCE |
| PhysicsTrack | `backward_undulation_limit_radians` | `ChrCtrl.backward_undulation_limit_radians` | `0x330` | f32, 1 words | REFERENCE |
| PhysicsTrack | `side_undulation` | `ChrCtrl.side_undulation` | `0x334` | f32, 1 words | REFERENCE |
| PhysicsTrack | `undulation_correction_gain` | `ChrCtrl.undulation_correction_gain` | `0x338` | f32, 1 words | REFERENCE |
| PhysicsTrack | `weight_type` | `ChrCtrl.weight_type` | `0x18C` | u32, 1 words | REFERENCE |
| PhysicsTrack | `is_undulation` | `ChrCtrl.is_undulation` | `0x328` | u8, 1 words | REFERENCE |
| PhysicsTrack | `use_ik_normal_by_undulation` | `ChrCtrl.use_ik_normal_by_undulation` | `0x329` | u8, 1 words | REFERENCE |
| PhysicsTrack | `hit_group_and_navimesh` | `ChrCtrl.hit_group_and_navimesh` | `0x3A9` | u8, 1 words | REFERENCE |
| AnimationTrack | `read_idx` | `CSChrTimeActModule.read_idx` | `0xC4` | u32, 1 words | REFERENCE |
| AnimationTrack | `write_idx` | `CSChrTimeActModule.write_idx` | `0xC0` | u32, 1 words | REFERENCE |
| AnimationTrack | `queue_0_anim_id` | `CSChrTimeActModule.anim_queue[0].anim_id` | `0x20` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_0_play_time` | `CSChrTimeActModule.anim_queue[0].play_time` | `0x24` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_0_anim_length` | `CSChrTimeActModule.anim_queue[0].anim_length` | `0x2C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_1_anim_id` | `CSChrTimeActModule.anim_queue[1].anim_id` | `0x30` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_1_play_time` | `CSChrTimeActModule.anim_queue[1].play_time` | `0x34` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_1_anim_length` | `CSChrTimeActModule.anim_queue[1].anim_length` | `0x3C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_2_anim_id` | `CSChrTimeActModule.anim_queue[2].anim_id` | `0x40` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_2_play_time` | `CSChrTimeActModule.anim_queue[2].play_time` | `0x44` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_2_anim_length` | `CSChrTimeActModule.anim_queue[2].anim_length` | `0x4C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_3_anim_id` | `CSChrTimeActModule.anim_queue[3].anim_id` | `0x50` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_3_play_time` | `CSChrTimeActModule.anim_queue[3].play_time` | `0x54` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_3_anim_length` | `CSChrTimeActModule.anim_queue[3].anim_length` | `0x5C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_4_anim_id` | `CSChrTimeActModule.anim_queue[4].anim_id` | `0x60` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_4_play_time` | `CSChrTimeActModule.anim_queue[4].play_time` | `0x64` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_4_anim_length` | `CSChrTimeActModule.anim_queue[4].anim_length` | `0x6C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_5_anim_id` | `CSChrTimeActModule.anim_queue[5].anim_id` | `0x70` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_5_play_time` | `CSChrTimeActModule.anim_queue[5].play_time` | `0x74` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_5_anim_length` | `CSChrTimeActModule.anim_queue[5].anim_length` | `0x7C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_6_anim_id` | `CSChrTimeActModule.anim_queue[6].anim_id` | `0x80` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_6_play_time` | `CSChrTimeActModule.anim_queue[6].play_time` | `0x84` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_6_anim_length` | `CSChrTimeActModule.anim_queue[6].anim_length` | `0x8C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_7_anim_id` | `CSChrTimeActModule.anim_queue[7].anim_id` | `0x90` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_7_play_time` | `CSChrTimeActModule.anim_queue[7].play_time` | `0x94` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_7_anim_length` | `CSChrTimeActModule.anim_queue[7].anim_length` | `0x9C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_8_anim_id` | `CSChrTimeActModule.anim_queue[8].anim_id` | `0xA0` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_8_play_time` | `CSChrTimeActModule.anim_queue[8].play_time` | `0xA4` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_8_anim_length` | `CSChrTimeActModule.anim_queue[8].anim_length` | `0xAC` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_9_anim_id` | `CSChrTimeActModule.anim_queue[9].anim_id` | `0xB0` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_9_play_time` | `CSChrTimeActModule.anim_queue[9].play_time` | `0xB4` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_9_anim_length` | `CSChrTimeActModule.anim_queue[9].anim_length` | `0xBC` | f32, 1 words | REFERENCE |
| AnimationTrack | `animation_speed` | `CSChrBehaviorModule.animation_speed` | `0x17C8` | f32, 1 words | REFERENCE |
| AnimationTrack | `max_ankle_pitch_angle_rad` | `CSChrBehaviorModule.max_ankle_pitch_angle_rad` | `0x1684` | f32, 1 words | REFERENCE |
| AnimationTrack | `max_ankle_roll_angle_rad` | `CSChrBehaviorModule.max_ankle_roll_angle_rad` | `0x1688` | f32, 1 words | REFERENCE |
| AnimationTrack | `ground_touch_state` | `CSChrBehaviorModule.ground_touch_state` | `0x1680` | u32, 1 words | REFERENCE |
| AnimationTrack | `root_motion` | `CSChrBehaviorModule.root_motion` | `0x30` | f32, 4 words | REFERENCE |
| AnimationTrack | `request_animation_id` | `CSChrEventModule.request_animation_id` | `0x18` | i32, 1 words | REFERENCE |
| AnimationTrack | `idle_anim_id` | `CSChrEventModule.idle_anim_id` | `0x1C` | i32, 1 words | REFERENCE |
| AnimationTrack | `ez_state_request_ladder` | `CSChrEventModule.ez_state_request_ladder` | `0x28` | i32, 1 words | REFERENCE |
| AnimationTrack | `ez_state_request_ladder_output` | `CSChrEventModule.ez_state_request_ladder_output` | `0x4C` | i32, 1 words | REFERENCE |
| AnimationTrack | `flags` | `CSChrEventModule.flags` | `0x40` | u8, 1 words | REFERENCE |
| ActionTrackRaw | `action_requests` | `CSChrActionRequestModule.action_requests` | `0x10` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `previous_action_requests` | `CSChrActionRequestModule.previous_action_requests` | `0x18` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `new_action_presses` | `CSChrActionRequestModule.new_action_presses` | `0x20` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `released_actions` | `CSChrActionRequestModule.released_actions` | `0x28` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `cancel_ready_actions` | `CSChrActionRequestModule.cancel_ready_actions` | `0x30` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `queued_action_inputs` | `CSChrActionRequestModule.queued_action_inputs` | `0x38` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `disabled_action_inputs` | `CSChrActionRequestModule.disabled_action_inputs` | `0x40` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `possible_action_inputs` | `CSChrActionRequestModule.possible_action_inputs` | `0x98` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `possible_action_cancels` | `CSChrActionRequestModule.possible_action_cancels` | `0xA0` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `prev_possible_action_inputs` | `CSChrActionRequestModule.prev_possible_action_inputs` | `0xA8` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `readback_new_presses` | `CSChrActionRequestModule.readback_new_presses` | `0x108` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `readback_cancel_ready` | `CSChrActionRequestModule.readback_cancel_ready` | `0x110` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `readback_queued_inputs` | `CSChrActionRequestModule.readback_queued_inputs` | `0x118` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `readback_possible_inputs` | `CSChrActionRequestModule.readback_possible_inputs` | `0x120` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `readback_possible_cancels` | `CSChrActionRequestModule.readback_possible_cancels` | `0x128` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `npc_action_id` | `CSChrActionRequestModule.npc_action_id` | `0xF4` | i32, 1 words | REFERENCE |
| ActionTrackRaw | `requested_gesture` | `CSChrActionRequestModule.requested_gesture` | `0xF8` | i32, 1 words | REFERENCE |
| ActionTrackRaw | `readback_npc_action_id` | `CSChrActionRequestModule.readback_npc_action_id` | `0x130` | i32, 1 words | REFERENCE |
| ActionTrackRaw | `queue_tae_id_override` | `CSChrActionRequestModule.queue_tae_id_override` | `0x134` | i32, 1 words | REFERENCE |
| ActionTrackRaw | `queue_current_tae_id` | `CSChrActionRequestModule.action_request_queue.current_tae_id` | `0x90` | i32, 1 words | REFERENCE |
| ActionTrackRaw | `tae_cancels` | `CSChrActionRequestModule.tae_cancels` | `0x100` | u32, 1 words | REFERENCE |
| ActionTrackRaw | `movement_request_flags` | `CSChrActionRequestModule.movement_request_flags` | `0xFC` | u32, 1 words | REFERENCE |
| ActionTrackRaw | `queue_mode_enabled` | `CSChrActionRequestModule.queue_mode_enabled` | `0x138` | u8, 1 words | REFERENCE |
| ActionTrackRaw | `timer_r1` | `CSChrActionRequestModule.action_timers.r1` | `0xB0` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_r2` | `CSChrActionRequestModule.action_timers.r2` | `0xB4` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_l1` | `CSChrActionRequestModule.action_timers.l1` | `0xB8` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_l2` | `CSChrActionRequestModule.action_timers.l2` | `0xBC` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_action` | `CSChrActionRequestModule.action_timers.action` | `0xC0` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_roll` | `CSChrActionRequestModule.action_timers.roll` | `0xC4` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_jump` | `CSChrActionRequestModule.action_timers.jump` | `0xC8` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_use_item` | `CSChrActionRequestModule.action_timers.use_item` | `0xCC` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_switch_spell` | `CSChrActionRequestModule.action_timers.switch_spell` | `0xD0` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_change_weapon_r` | `CSChrActionRequestModule.action_timers.change_weapon_r` | `0xD4` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_change_weapon_l` | `CSChrActionRequestModule.action_timers.change_weapon_l` | `0xD8` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_change_item` | `CSChrActionRequestModule.action_timers.change_item` | `0xDC` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_r3` | `CSChrActionRequestModule.action_timers.r3` | `0xE0` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_l3` | `CSChrActionRequestModule.action_timers.l3` | `0xE4` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_touch_r` | `CSChrActionRequestModule.action_timers.touch_r` | `0xE8` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_touch_l` | `CSChrActionRequestModule.action_timers.touch_l` | `0xEC` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `hks_root_motion_mult` | `CSChrBehaviorDataModule.hks_root_motion_mult` | `0x24C` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `turn_speed` | `CSChrBehaviorDataModule.turn_speed` | `0x250` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `hks_animation_speed_multiplier` | `CSChrBehaviorDataModule.hks_animation_speed_multiplier` | `0x310` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `fixed_rotation_direction` | `CSChrBehaviorDataModule.fixed_rotation_direction` | `0x1E3` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `has_twist_modifier` | `CSChrBehaviorDataModule.has_twist_modifier` | `0x1E2` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `root_motion_reduction` | `ChrCtrlModifier.data.root_motion_reduction` | `0x2C` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `movement_limit` | `ChrCtrlModifier.data.movement_limit` | `0x38` | u32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `action_flags` | `ChrCtrlModifier.data.action_flags` | `0x18` | u32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `hks_flags` | `ChrCtrlModifier.data.hks_flags` | `0x1C` | u32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `movement_request_duration` | `CSChrActionRequestModule.movement_request_duration` | `0xF0` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `normal_vector` | `CSChrPhysicsModule.material_info.normal_vector` | `0x230` | f32, 4 words | REFERENCE |
| LocomotionBehaviorTrack | `orientation_matrix` | `CSChrPhysicsModule.material_info.orientation_matrix` | `0x1F0` | f32, 16 words | REFERENCE |
| LocomotionBehaviorTrack | `hit_material` | `CSChrPhysicsModule.material_info.hit_material` | `0x250` | i32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `is_slippery_surface` | `CSChrPhysicsModule.material_info.is_slippery_surface` | `0x255` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `is_non_slippery_surface` | `CSChrPhysicsModule.material_info.is_non_slippery_surface` | `0x254` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `slide_vector` | `CSChrPhysicsModule.slide_info.slide_vector` | `0x260` | f32, 4 words | REFERENCE |
| LocomotionBehaviorTrack | `normal_angle` | `CSChrPhysicsModule.slide_info.normal_angle` | `0x278` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `normal_angle_deg` | `CSChrPhysicsModule.slide_info.normal_angle_deg` | `0x280` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `is_sliding` | `CSChrPhysicsModule.slide_info.is_sliding` | `0x27C` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `enable_angle_check` | `CSChrPhysicsModule.slide_info.enable_angle_check` | `0x27D` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `enabled` | `CSChrPhysicsModule.slide_info.enabled` | `0x284` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `enable_slide_interpolation` | `CSChrPhysicsModule.slide_info.enable_slide_interpolation` | `0x285` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `min_twist_rank` | `CSChrBehaviorDataModule.min_twist_rank` | `0x1E0` | i16, 1 words | REFERENCE |
| EquipmentTrack | `equipment_param_ids` | `ChrAsm.equipment_param_ids` | `0x7C` | i32, 22 words | REFERENCE |
| EquipmentTrack | `left_weapon_slot` | `ChrAsm.equipment.selected_slots.left_weapon_slot` | `0xC` | u32, 1 words | REFERENCE |
| EquipmentTrack | `right_weapon_slot` | `ChrAsm.equipment.selected_slots.right_weapon_slot` | `0x10` | u32, 1 words | REFERENCE |
| EquipmentTrack | `left_arrow_slot` | `ChrAsm.equipment.selected_slots.left_arrow_slot` | `0x14` | u32, 1 words | REFERENCE |
| EquipmentTrack | `right_arrow_slot` | `ChrAsm.equipment.selected_slots.right_arrow_slot` | `0x18` | u32, 1 words | REFERENCE |
| EquipmentTrack | `left_bolt_slot` | `ChrAsm.equipment.selected_slots.left_bolt_slot` | `0x1C` | u32, 1 words | REFERENCE |
| EquipmentTrack | `right_bolt_slot` | `ChrAsm.equipment.selected_slots.right_bolt_slot` | `0x20` | u32, 1 words | REFERENCE |
| EquipmentTrack | `arm_style` | `ChrAsm.equipment.arm_style` | `0x8` | u32, 1 words | REFERENCE |
| AppearanceTrack | `face_buffer` | `PlayerGameData.face_data.face_data_buffer` | `0x768` | bytes, 72 words | REFERENCE |
| AppearanceTrack | `gender` | `PlayerGameData.gender` | `0xBE` | u8, 1 words | REFERENCE |
| AppearanceTrack | `archetype` | `PlayerGameData.archetype` | `0xBF` | u8, 1 words | REFERENCE |
| GameplayStateTrack | `hp` | `CSChrDataModule.hp` | `0x138` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `max_hp` | `CSChrDataModule.max_hp` | `0x13C` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `max_uncapped_hp` | `CSChrDataModule.max_uncapped_hp` | `0x140` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `base_hp` | `CSChrDataModule.base_hp` | `0x144` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `fp` | `CSChrDataModule.fp` | `0x148` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `max_fp` | `CSChrDataModule.max_fp` | `0x14C` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `base_fp` | `CSChrDataModule.base_fp` | `0x150` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `stamina` | `CSChrDataModule.stamina` | `0x154` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `max_stamina` | `CSChrDataModule.max_stamina` | `0x158` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `base_stamina` | `CSChrDataModule.base_stamina` | `0x15C` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `chara_init_param_id` | `CSChrDataModule.chara_init_param_id` | `0xC4` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `recoverable_hp` | `CSChrDataModule.recoverable_hp` | `0x160` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `recoverable_hp_time` | `CSChrDataModule.recoverable_hp_time` | `0x168` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `sa_durability` | `CSChrSuperArmorModule.sa_durability` | `0x10` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `sa_durability_max` | `CSChrSuperArmorModule.sa_durability_max` | `0x14` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `armor_recover_time` | `CSChrSuperArmorModule.recover_time` | `0x1C` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `poise_broken_state` | `CSChrSuperArmorModule.poise_broken_state` | `0x22` | u8, 1 words | REFERENCE |
| GameplayStateTrack | `toughness` | `CSChrToughnessModule.toughness` | `0x10` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `toughness_max` | `CSChrToughnessModule.toughness_max` | `0x18` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `toughness_recover_time` | `CSChrToughnessModule.recover_time` | `0x1C` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `stamina_recovery_remainder` | `ChrIns.stamina_recovery_remainder` | `0xE4` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `stamina_recovery_modifier` | `ChrIns.stamina_recovery_modifier` | `0xE8` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `draw_params` | `CSChrDataModule.draw_params` | `0xC0` | u32, 1 words | REFERENCE |
| GameplayStateTrack | `block_id_origin` | `CSChrDataModule.block_id_origin` | `0x7C` | u32, 1 words | REFERENCE |
| GameplayStateTrack | `trigger_max_toughness_update` | `CSChrToughnessModule.trigger_max_toughness_update` | `0x2D` | u8, 1 words | REFERENCE |
| EffectSignalsTrack | `item_use_cast_sfx_id` | `ChrIns.item_use_cast_sfx_id` | `0x16C` | i32, 1 words | REFERENCE |
| EffectSignalsTrack | `item_use_fire_sfx_id` | `ChrIns.item_use_fire_sfx_id` | `0x170` | i32, 1 words | REFERENCE |
| EffectSignalsTrack | `item_use_effect_sfx_id` | `ChrIns.item_use_effect_sfx_id` | `0x174` | i32, 1 words | REFERENCE |
| EffectSignalsTrack | `tae_queued_use_item` | `ChrIns.tae_queued_use_item` | `0x160` | u32, 1 words | REFERENCE |
| CombatStateTrack | `is_locked_on` | `ChrIns.is_locked_on` | `0xC9` | u8, 1 words | REFERENCE |
| CombatStateTrack | `lock_on_target_position` | `ChrIns.lock_on_target_position` | `0xD0` | f32, 4 words | REFERENCE |
| CombatStateTrack | `animation_action_flags` | `CSChrActionFlagModule.animation_action_flags` | `0x10` | u32, 1 words | REFERENCE |
| CombatStateTrack | `action_modifiers_flags` | `CSChrActionFlagModule.action_modifiers_flags` | `0x40` | u64, 2 words | REFERENCE |
| CombatStateTrack | `damage_level` | `CSChrActionFlagModule.damage_level` | `0x1C` | u8, 1 words | REFERENCE |
| CombatStateTrack | `guard_level` | `CSChrActionFlagModule.guard_level` | `0x20` | u32, 1 words | REFERENCE |
| CombatStateTrack | `received_damage_type` | `CSChrActionFlagModule.received_damage_type` | `0x34` | u32, 1 words | REFERENCE |
| CombatStateTrack | `turn_speed` | `CSChrActionFlagModule.turn_speed` | `0x84` | f32, 1 words | REFERENCE |
| CombatStateTrack | `lock_on_turn_speed` | `CSChrActionFlagModule.lock_on_turn_speed` | `0x88` | f32, 1 words | REFERENCE |
| CombatStateTrack | `joint_turn_speed` | `CSChrActionFlagModule.joint_turn_speed` | `0x8C` | f32, 1 words | REFERENCE |
| CombatStateTrack | `facing_angle_correction_rad` | `CSChrActionFlagModule.facing_angle_correction_rad` | `0xA8` | f32, 1 words | REFERENCE |
| CombatStateTrack | `root_motion_div` | `CSChrActionFlagModule.root_motion_div` | `0xAC` | f32, 1 words | REFERENCE |
| CombatStateTrack | `root_motion_mult_min_dist` | `CSChrActionFlagModule.root_motion_mult_min_dist` | `0xB0` | f32, 1 words | REFERENCE |
| CombatStateTrack | `speed_default` | `CSChrActionFlagModule.speed_default` | `0x94` | f32, 1 words | REFERENCE |
| CombatStateTrack | `speed_extra` | `CSChrActionFlagModule.speed_extra` | `0x98` | f32, 1 words | REFERENCE |
| CombatStateTrack | `speed_boost` | `CSChrActionFlagModule.speed_boost` | `0x9C` | f32, 1 words | REFERENCE |
| CombatStateTrack | `root_motion_mult_target_radius` | `CSChrActionFlagModule.root_motion_mult_target_radius` | `0xBC` | f32, 1 words | REFERENCE |
| CombatStateTrack | `disable_lock_on_angle` | `CSChrActionFlagModule.disable_lock_on_angle` | `0x1E0` | f32, 1 words | REFERENCE |
| CombatStateTrack | `mov_dist_multiplier` | `CSChrActionFlagModule.mov_dist_multiplier` | `0x1F8` | f32, 1 words | REFERENCE |
| CombatStateTrack | `cam_turn_dist_multiplier` | `CSChrActionFlagModule.cam_turn_dist_multiplier` | `0x1FC` | f32, 1 words | REFERENCE |
| CombatStateTrack | `ladder_dist_multiplier` | `CSChrActionFlagModule.ladder_dist_multiplier` | `0x200` | f32, 1 words | REFERENCE |
| CombatStateTrack | `sa_durability_multiplier` | `CSChrActionFlagModule.sa_durability_multiplier` | `0x208` | f32, 1 words | REFERENCE |
| CombatStateTrack | `knockback_value` | `CSChrActionFlagModule.knockback_value` | `0x218` | f32, 1 words | REFERENCE |
| CombatStateTrack | `camera_lock_on_param_id` | `CSChrActionFlagModule.camera_lock_on_param_id` | `0x1E4` | i32, 1 words | REFERENCE |
| CombatStateTrack | `guard_behavior_judge_id` | `CSChrActionFlagModule.guard_behavior_judge_id` | `0x204` | u32, 1 words | REFERENCE |
| CombatStateTrack | `action_flags` | `CSChrActionFlagModule.action_flags` | `0x21C` | u32, 1 words | REFERENCE |
| CombatStateTrack | `weapon_model_location_overridden` | `CSChrActionFlagModule.weapon_model_location_overridden` | `0x78` | u8, 1 words | REFERENCE |
| CombatStateTrack | `sp_effect_wet_condition_depth` | `CSChrActionFlagModule.sp_effect_wet_condition_depth` | `0x20C` | u8, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_up_limit` | `CSChrActionFlagModule.bullet_aim_angle_up_limit` | `0x238` | i16, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_down_limit` | `CSChrActionFlagModule.bullet_aim_angle_down_limit` | `0x23A` | i16, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_right_limit` | `CSChrActionFlagModule.bullet_aim_angle_right_limit` | `0x23C` | i16, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_left_limit` | `CSChrActionFlagModule.bullet_aim_angle_left_limit` | `0x23E` | i16, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_up_dead_zone` | `CSChrActionFlagModule.bullet_aim_angle_up_dead_zone` | `0x240` | i16, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_down_dead_zone` | `CSChrActionFlagModule.bullet_aim_angle_down_dead_zone` | `0x242` | i16, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_right_dead_zone` | `CSChrActionFlagModule.bullet_aim_angle_right_dead_zone` | `0x244` | i16, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_left_dead_zone` | `CSChrActionFlagModule.bullet_aim_angle_left_dead_zone` | `0x246` | i16, 1 words | REFERENCE |

242 fields / 455 raw words; masks distinguish unavailable reads from actual zero. Appearance stores the native bounded 288-byte face buffer without decoding hair/colors/body semantics. No opaque memory-page dump, pointer-containing game object or executable content is saved. Nonfinite float bit patterns are preserved in raw tracks and reported as suspicious, not substituted. TransformTrack retains existing finite/quaternion validation.

Missing: PoseTrack; actual native proxy velocity/angular velocity; controller/proxy full internal state; HKS VM variables and behavior node/layer/blend weights; full per-state action vector contents; full active SpEffect list and bone attachments; independent animation transitions within a single callback interval. These are UNAVAILABLE or NOT YET IMPLEMENTED, not zero-valued synthetic states.

## SDK root access offsets (REFERENCE only)

```text
ROOT PlayerIns.chr_ins offset=0x0 confidence=REFERENCE
ROOT PlayerIns.chr_asm offset=0x638 confidence=REFERENCE
ROOT PlayerIns.player_game_data offset=0x580 confidence=REFERENCE
ROOT ChrIns.chr_ctrl offset=0x58 confidence=REFERENCE
ROOT ChrIns.modules offset=0x190 confidence=REFERENCE
ROOT ChrInsModuleContainer.physics offset=0x68 confidence=REFERENCE
ROOT ChrInsModuleContainer.behavior offset=0x28 confidence=REFERENCE
ROOT ChrInsModuleContainer.time_act offset=0x18 confidence=REFERENCE
ROOT ChrInsModuleContainer.event offset=0x58 confidence=REFERENCE
ROOT ChrInsModuleContainer.action_request offset=0x80 confidence=REFERENCE
ROOT ChrInsModuleContainer.behavior_data offset=0xC0 confidence=REFERENCE
ROOT ChrInsModuleContainer.data offset=0x0 confidence=REFERENCE
ROOT ChrInsModuleContainer.fall offset=0x70 confidence=REFERENCE
ROOT ChrInsModuleContainer.super_armor offset=0x40 confidence=REFERENCE
ROOT ChrInsModuleContainer.toughness offset=0x48 confidence=REFERENCE
ROOT ChrInsModuleContainer.action_flag offset=0x8 confidence=REFERENCE
```

These are source layout anchors. Existing production transform path is retained; additional module/equipment/appearance correctness still requires runtime correlation.

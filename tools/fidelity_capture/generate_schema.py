"""Generate our independent wire schema and field reader from explicit pinned SDK paths.
No offsets or engine calls are guessed. Re-run after changing this field catalog.
"""
from pathlib import Path
import json
ROOT=Path(__file__).resolve().parents[2]
tracks=[]
def track(id,name):
 t=dict(id=id,name=name,fields=[]);tracks.append(t);return t
def f(t,name,expr,kind='u32',words=1):
 t['fields'].append(dict(name=name,expression=expr,kind=kind,words=words,confidence='RUNTIME_READ_VERIFIED' if expr=='p.position' else 'REFERENCE'))
def fields(t,p,names,kind='f32'):
 for name in names.split():f(t,name,f'{p}.{name}',kind)
def vec(t,p,names,n=4):
 for name in names.split():f(t,name,f'{p}.{name}','f32',n)
t=track(5,'PhysicsTrack')
vec(t,'p','position last_update_position interpolated_orientation additional_rotation orientation_euler')
fields(t,'p','rotation_multiplier motion_multiplier gravity_multiplier chr_push_up_factor default_max_turn_rate hit_height hit_radius weight')
fields(t,'p','chr_proxy_pos_update_requested standing_on_solid_ground touching_solid_ground is_falling is_touching_ground gravity_disabled fade_out_gravity_disabled flying_character_fall_requested use_world_y_alignment_logic is_surface_constrained adjust_to_hi_collision','u8')
f(t,'move_type_flags','p.move_type_flags','u8')
vec(t,'ctrl','physics_model_matrix model_matrix',16)
vec(t,'ctrl','additional_orientation_quat')
fields(t,'ctrl','vertical_position_offset scale_size_x scale_size_y scale_size_z offset_y ragdoll_revive_time foot_ik_error_height_limit')
fields(t,'ctrl','disable_move height_correction_request chr_ragdoll_state','u8')
fields(t,'ctrl','flags flags_copy chr_proxy_flags','u32')
fields(t,'fall','fall_timer');fields(t,'fall','force_max_fall_height disable_fall_motion','u8')
t=track(6,'AnimationTrack')
fields(t,'tae','read_idx write_idx','u32')
for i in range(10):
 for name,kind in [('anim_id','i32'),('play_time','f32'),('anim_length','f32')]:f(t,f'queue_{i}_{name}',f'tae.anim_queue[{i}].{name}',kind)
fields(t,'behavior','animation_speed max_ankle_pitch_angle_rad max_ankle_roll_angle_rad')
fields(t,'behavior','ground_touch_state','u32');vec(t,'behavior','root_motion')
fields(t,'event','request_animation_id idle_anim_id ez_state_request_ladder ez_state_request_ladder_output','i32');fields(t,'event','flags','u8')
t=track(7,'ActionTrackRaw')
fields(t,'action','action_requests previous_action_requests new_action_presses released_actions cancel_ready_actions queued_action_inputs disabled_action_inputs possible_action_inputs possible_action_cancels prev_possible_action_inputs readback_new_presses readback_cancel_ready readback_queued_inputs readback_possible_inputs readback_possible_cancels','u64')
# u64 values occupy two little-endian words.
for field in t['fields']:field['words']=2
fields(t,'action','npc_action_id requested_gesture readback_npc_action_id queue_tae_id_override','i32')
f(t,'queue_current_tae_id','action.action_request_queue.current_tae_id','i32')
fields(t,'action','tae_cancels movement_request_flags','u32');fields(t,'action','queue_mode_enabled','u8')
for name in 'r1 r2 l1 l2 action roll jump use_item switch_spell change_weapon_r change_weapon_l change_item r3 l3 touch_r touch_l'.split():f(t,'timer_'+name,'action.action_timers.'+name,'f32')
t=track(8,'LocomotionBehaviorTrack')
fields(t,'behavior_data','hks_root_motion_mult turn_speed hks_animation_speed_multiplier')
fields(t,'behavior_data','fixed_rotation_direction has_twist_modifier','u8')
fields(t,'modifier','root_motion_reduction');fields(t,'modifier','movement_limit action_flags hks_flags','u32')
fields(t,'action','movement_request_duration')
vec(t,'p.material_info','normal_vector orientation_matrix',4) # matrix overridden below
for field in t['fields']:
 if field['name']=='orientation_matrix':field['words']=16
fields(t,'p.material_info','hit_material','i32');fields(t,'p.material_info','is_slippery_surface is_non_slippery_surface','u8')
vec(t,'p.slide_info','slide_vector');fields(t,'p.slide_info','normal_angle normal_angle_deg')
fields(t,'p.slide_info','is_sliding enable_angle_check enabled enable_slide_interpolation','u8')
t=track(9,'EquipmentTrack')
f(t,'equipment_param_ids','asm.equipment_param_ids','i32',22)
fields(t,'asm.equipment.selected_slots','left_weapon_slot right_weapon_slot left_arrow_slot right_arrow_slot left_bolt_slot right_bolt_slot','u32')
f(t,'arm_style','asm.equipment.arm_style','u32')
t=track(10,'AppearanceTrack')
f(t,'face_buffer','game_data.face_data.face_data_buffer','bytes',72)
fields(t,'game_data','gender archetype','u8')
t=track(11,'GameplayStateTrack')
fields(t,'data','hp max_hp max_uncapped_hp base_hp fp max_fp base_fp stamina max_stamina base_stamina chara_init_param_id','i32')
fields(t,'data','recoverable_hp recoverable_hp_time')
fields(t,'armor','sa_durability sa_durability_max recover_time');fields(t,'armor','poise_broken_state','u8')
fields(t,'toughness','toughness toughness_max recover_time')
fields(t,'chr','stamina_recovery_remainder stamina_recovery_modifier')
t=track(12,'EffectSignalsTrack')
fields(t,'chr','item_use_cast_sfx_id item_use_fire_sfx_id item_use_effect_sfx_id','i32')
f(t,'tae_queued_use_item','chr.tae_queued_use_item','u32')
t=track(13,'CombatStateTrack')
fields(t,'chr','is_locked_on','u8');vec(t,'chr','lock_on_target_position')
fields(t,'action_flag','animation_action_flags','u32');fields(t,'action_flag','action_modifiers_flags','u64')
t['fields'][-1]['words']=2
fields(t,'action_flag','damage_level','u8');fields(t,'action_flag','guard_level received_damage_type','u32')
fields(t,'action_flag','turn_speed lock_on_turn_speed joint_turn_speed facing_angle_correction_rad root_motion_div root_motion_mult_min_dist')
# Additional public scalar fields relevant to fidelity, without following opaque pointers.
t=tracks[0]
fields(t,'p','chr_hit_height chr_hit_radius step_disp_interpolate_time step_disp_interpolate_trigger_value')
fields(t,'p','is_enable_step_disp_interpolate is_watcher_stones','u8')
vec(t,'ctrl','physics_transform_matrix_squared model_matrix_squared',16)
fields(t,'ctrl','foot_ik_error_on_gain foot_ik_error_off_gain forward_undulation_limit_radians backward_undulation_limit_radians side_undulation undulation_correction_gain')
fields(t,'ctrl','weight_type','u32');fields(t,'ctrl','is_undulation use_ik_normal_by_undulation hit_group_and_navimesh','u8')
t=tracks[3];fields(t,'behavior_data','min_twist_rank','i16')
t=tracks[6];fields(t,'data','draw_params block_id_origin','u32');fields(t,'toughness','trigger_max_toughness_update','u8')
t=tracks[8]
fields(t,'action_flag','speed_default speed_extra speed_boost root_motion_mult_target_radius disable_lock_on_angle mov_dist_multiplier cam_turn_dist_multiplier ladder_dist_multiplier sa_durability_multiplier knockback_value')
fields(t,'action_flag','camera_lock_on_param_id','i32');fields(t,'action_flag','guard_behavior_judge_id action_flags','u32');fields(t,'action_flag','weapon_model_location_overridden sp_effect_wet_condition_depth','u8')
fields(t,'action_flag','bullet_aim_angle_up_limit bullet_aim_angle_down_limit bullet_aim_angle_right_limit bullet_aim_angle_left_limit bullet_aim_angle_up_dead_zone bullet_aim_angle_down_dead_zone bullet_aim_angle_right_dead_zone bullet_aim_angle_left_dead_zone','i16')
# Keep names unique (e.g. armor and toughness each expose recover_time).
for t in tracks:
 names=[f['name']for f in t['fields']]
 for field in t['fields']:
  if names.count(field['name'])>1:field['name']=field['expression'].split('.')[0]+'_'+field['name']
word=0
for t in tracks:
 t['begin']=word
 for field in t['fields']:field['offset']=word;word+=field['words']
 t['words']=word-t['begin'];t['mask_words']=(t['words']+31)//32;t['record_bytes']=32+4*t['mask_words']+4*t['words']
mask=(word+31)//32
schema=dict(schema=1,word_count=word,mask_words=mask,wire_bytes=16+4*(word+mask),sdk='3c8c1d7633a99309fb004c9f894ea10b7967d0e0',target_sha256='D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134',tracks=tracks)
(ROOT/'shared/capture_schema.json').write_text(json.dumps(schema,indent=2)+'\n')
header='// Generated by tools/fidelity_capture/generate_schema.py\n#pragma once\n#include <array>\n#include <cstdint>\nnamespace erplay {\n'
header+=f'inline constexpr unsigned capture_words={word}, capture_mask_words={mask}, capture_wire_bytes={schema["wire_bytes"]};\n'
header+='struct CaptureFrame { std::uint32_t schema{1}, words{capture_words}; std::uint64_t source_drops{}; std::array<std::uint32_t,capture_mask_words> valid{}; std::array<std::uint32_t,capture_words> values{}; };\nstatic_assert(sizeof(CaptureFrame)==capture_wire_bytes);\n'
header+='struct CaptureTrackDef {unsigned id,begin,words,mask_words,record_bytes;const char*name;};\ninline constexpr CaptureTrackDef capture_tracks[]{\n'
for t in tracks:header+=f'{{{t["id"]},{t["begin"]},{t["words"]},{t["mask_words"]},{t["record_bytes"]},"{t["name"]}"}},\n'
header+='};\nstruct CaptureFieldDef {unsigned track,offset,words;const char*name;const char*kind;};\ninline constexpr CaptureFieldDef capture_fields[]{\n'
for t in tracks:
 for field in t['fields']:header+=f'{{{t["id"]},{field["offset"]},{field["words"]},"{field["name"]}","{field["kind"]}"}},\n'
header+='};\n}\n';(ROOT/'src/capture_schema.hpp').write_text(header)
rust=f'// Generated by tools/fidelity_capture/generate_schema.py\npub const WORDS:usize={word};\npub const MASK_WORDS:usize={mask};\npub const WIRE_BYTES:usize={schema["wire_bytes"]};\n'
# RPM copies only named primitive/array fields, never pointer-containing object blobs.
rust+='pub fn observe(player:&eldenring::cs::PlayerIns)->Frame {\nlet mut out=Frame::default();\nlet chr=std::ptr::addr_of!(player.chr_ins);let owner=chr as usize;\n'
modules={'p':'physics','behavior':'behavior','tae':'time_act','event':'event','action':'action_request','behavior_data':'behavior_data','data':'data','fall':'fall','armor':'super_armor','toughness':'toughness','action_flag':'action_flag'}
for var,member in modules.items():rust+=f'let {var}=player.chr_ins.modules.{member}.as_ptr();\n'
rust+='let ctrl=player.chr_ins.chr_ctrl.as_ptr();\nlet modifier=unsafe{read_pointer(std::ptr::addr_of!((*ctrl).modifier))}.unwrap_or(0) as *const eldenring::cs::ChrCtrlModifier;\nlet asm=player.chr_asm.as_ptr();let game_data=player.player_game_data.as_ptr();\n'
# Check module owner with RPM; fields are raw-reference confidence until correlation.
for t in tracks:
 rust+=f'// {t["name"]}: no semantics guessed, raw bit patterns preserved.\n'
 for field in t['fields']:
  expr=field['expression'];base,rest=expr.split('.',1)
  # Pointer-access expression including nested subobjects (no reference constructed).
  if base=='modifier':rest='data.'+rest
  guard='true' if base in ('chr','asm','game_data') else f'owned({base},owner)'
  if base=='modifier':guard='owned(ctrl,owner)&&modifier_owned(modifier,ctrl as usize)'
  rust+=f'if {guard} {{ unsafe{{out.read(std::ptr::addr_of!((*{base}).{rest}),{field["offset"]},{field["words"]});}} }}\n'
 rust+='\n'
rust+='out\n}\n'
types={'p':'CSChrPhysicsModule','ctrl':'ChrCtrl','modifier':'ChrCtrlModifier','tae':'CSChrTimeActModule','behavior':'CSChrBehaviorModule','event':'CSChrEventModule','action':'CSChrActionRequestModule','behavior_data':'CSChrBehaviorDataModule','asm':'ChrAsm','game_data':'PlayerGameData','data':'CSChrDataModule','armor':'CSChrSuperArmorModule','toughness':'CSChrToughnessModule','chr':'ChrIns','fall':'CSChrFallModule','action_flag':'CSChrActionFlagModule'}
rust+='pub fn layout_report()->String {let mut out=String::new();\n'
for t in tracks:
 for field in t['fields']:
  base,rest=field['expression'].split('.',1);typ=types[base]
  if base=='modifier':rest='data.'+rest
  if 'anim_queue[' in rest:
   i=int(rest.split('[')[1].split(']')[0]);member=rest.split('].')[1]
   expr=f'std::mem::offset_of!(eldenring::cs::{typ},anim_queue)+{i}*std::mem::size_of::<eldenring::cs::CSChrTimeActModuleAnim>()+std::mem::offset_of!(eldenring::cs::CSChrTimeActModuleAnim,{member})'
  else:expr=f'std::mem::offset_of!(eldenring::cs::{typ},{rest})'
  rust+=f'out.push_str(&format!("FIELD {t["id"]} {field["name"]} {typ}.{rest} offset=0x{{:X}} words={field["words"]} confidence=REFERENCE\\n",{expr}));\n'
for typ,field in [('PlayerIns','chr_ins'),('PlayerIns','chr_asm'),('PlayerIns','player_game_data'),('ChrIns','chr_ctrl'),('ChrIns','modules')]+[('ChrInsModuleContainer',m)for m in modules.values()]:
 rust+=f'out.push_str(&format!("ROOT {typ}.{field} offset=0x{{:X}} confidence=REFERENCE\\n",std::mem::offset_of!(eldenring::cs::{typ},{field})));\n'
rust+='out}\n';(ROOT/'adapter/src/capture_fields.rs').write_text(rust)
print(f'{word} words, {sum(len(t["fields"])for t in tracks)} fields, {len(tracks)} tracks; wire {schema["wire_bytes"]} bytes')

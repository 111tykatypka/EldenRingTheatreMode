//! Camera-proximity asset visibility: ownership moved to `camera_fade.rs`.
//!
//! The cinematic branch (Codex C19) edited `ASSET_GEOMETORY_PARAM_ST.cam_near_behavior_type` values 1 and 2 only while a Free or
//! Dolly camera was owned, and restored them afterwards. In game that was not enough (the owner still saw fading): the
//! value -1 (434 rows) was left alone and rows already loaded keep the value they were created with. `camera_fade.rs` clears
//! every non-zero value (plus the character-model fade ids) as soon as Theater is connected, remembers each row and restores
//! it exactly; it is measured working in game. Two writers on the same table would overwrite each other's saved originals,
//! so this module is intentionally inert and the camera editor's "prevent asset fade" switch no longer changes the tables
//! (the always-on switch is the "No fade-out near the camera" checkbox, option bit 64). The near-plane part of the close-up
//! controls lives in the native camera runtime and is unaffected.
pub fn tick(_allowed:bool){}

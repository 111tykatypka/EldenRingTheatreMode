# Root motion, proxy and animation ordering

Pinned revision: `3c8c1d7633a99309fb004c9f894ea10b7967d0e0`; target 2.7.0.0 only.

Task enum exposes ChrIns_PreBehavior, HavokBehavior, ChrIns_PrePhysics, HavokWorldUpdate and ChrIns_PostPhysics. The existing, live-verified transform callback remains **ChrIns_PostPhysics**. Enum order indicates the intended stages; relative ordering among same-group tasks is not a guarantee. No additional guessed hook/signature is installed.

Public sources expose behavior.root_motion, behavior_data.hks_root_motion_mult and chr_ctrl.modifier.data.root_motion_reduction. These do not establish a safe permanent root-motion suppression API. Their data is TAE/HKS-owned. Zeroing them without restoring engine ownership can break jump/roll/collision. None is changed.

Transforms stay authoritative after physics. Before each apply, diagnostics compare the live transform with the last **actually applied interpolated** transform; after apply the requested values are written exactly. A growing error signals live input, animation/root motion, collision, gravity or origin changes but does not identify which one by itself. Logs from Phase4B show ~0.0655-unit motion and changing yaw while holding a target, consistent with input conflict; scoped local input suppression is now implemented but not live-verified.

Candidate animation playback writes only event.request_animation_id once on transitions. This can start next-frame root motion, so the same post-physics correction still runs. It may produce a sliding model, oscillation or collision mismatch even if memory follows. Do not claim doubled motion is solved until the new recorded roll/jump is observed. Documented ChrCtrl proxy sync flags remain OFF; test them only if memory follows but the visual proxy does not.

Pause holds transform. The exact local animation clock is not yet safely writable; no world timestop is used. Animation may continue while paused. Resume/seek may restart the right ID with unknown phase. Full arbitrary pose restoration needs a typed animation-player API or a separately validated time-control path. The current TimeAct queue is **observation**, not permission to write queue read/write indices or play_time.

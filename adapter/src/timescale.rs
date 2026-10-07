//! IGCS timing mechanism, independently implemented for the exact guarded 2.7.0.0 image.
//! Default is observation only. THEATER_WORLD_TIMESCALE=1 explicitly enables the experiment.
//! Called on the existing game task, never from IPC or the renderer. No clock API hooks.
use std::{ffi::c_void, sync::{Mutex, OnceLock}};

const ROOT_RVA:usize=0x358db58;
const SITES:[usize;2]=[0xdeb30f,0xdebe2f];
const SCALE:usize=0x2cc;
const DELTA:usize=0x268;
#[repr(C)] struct MemoryInfo {
    base:*mut c_void, allocation:*mut c_void, allocation_protect:u32,
    partition:u16, region_size:usize, state:u32, protect:u32, kind:u32,
}
#[link(name="kernel32")] unsafe extern "system" {
    fn GetModuleHandleW(name:*const u16)->*mut c_void;
    fn VirtualQuery(address:*const c_void,info:*mut MemoryInfo,size:usize)->usize;
}
fn accessible(address:usize,bytes:usize,write:bool)->bool {
    if address<0x10000 {return false;}
    let Some(end)=address.checked_add(bytes) else {return false;};
    let mut info:MemoryInfo=unsafe{std::mem::zeroed()};
    if unsafe{VirtualQuery(address as _,&mut info,std::mem::size_of::<MemoryInfo>())}==0 {return false;}
    let protection=info.protect&0xff;
    info.state==0x1000 && info.protect&0x100==0 &&
        matches!(protection,0x02|0x04|0x08|0x20|0x40|0x80) &&
        (!write||matches!(protection,0x04|0x08|0x40|0x80)) &&
        (info.base as usize).checked_add(info.region_size).is_some_and(|limit|end<=limit)
}
fn read<T:Copy>(address:usize)->Option<T> {
    accessible(address,std::mem::size_of::<T>(),false)
        .then(||unsafe{std::ptr::read_volatile(address as *const T)})
}
fn resolve()->Option<usize> {
    let base=unsafe{GetModuleHandleW(std::ptr::null())} as usize;
    if base==0 {return None;}
    // The outer initialization has already checked version, architecture and exact disk SHA.
    // Independently check both consumers and their RIP-relative target before trusting the RVA.
    for site in SITES {
        let bytes=read::<[u8;23]>(base+site)?;
        if bytes[..3]!=[0x48,0x8b,0x05] || bytes[7..23]!=
            [0xf3,0x0f,0x10,0x88,0xcc,0x02,0,0,0xf3,0x0f,0x59,0x88,0x68,0x02,0,0] {return None;}
        let displacement=i32::from_le_bytes(bytes[3..7].try_into().ok()?);
        if (base+site+7).checked_add_signed(displacement as isize)?!=base+ROOT_RVA {return None;}
    }
    accessible(base+ROOT_RVA,8,false).then_some(base+ROOT_RVA)
}
#[derive(Default)] struct Controller {
    root_slot:Option<usize>, resolved:bool, root:usize, saved:Option<f32>,
    last_written:Option<f32>, observed_root:usize, last_log_second:u64, inhibited:bool,
}
static STATE:Mutex<Controller>=Mutex::new(Controller{
    root_slot:None,resolved:false,root:0,saved:None,last_written:None,
    observed_root:0,last_log_second:0,inhibited:false,
});
fn enabled()->bool {
    static ENABLED:OnceLock<bool>=OnceLock::new();
    *ENABLED.get_or_init(||std::env::var("THEATER_WORLD_TIMESCALE").as_deref()==Ok("1"))
}
fn restore(s:&mut Controller,current_root:usize,reason:&str) {
    if let Some(saved)=s.saved.take() {
        // Never follow an old object after the manager has been replaced.
        let address=current_root+SCALE;
        let current=read::<f32>(address);
        if s.root==current_root && current==s.last_written && accessible(address,4,true) {
            unsafe{std::ptr::write_volatile(address as *mut f32,saved)};
            crate::log_game(&format!("TIMESCALE_RESET value={saved:.4} reason={reason}"));
        } else {
            crate::log_game(&format!("TIMESCALE_RELEASE reason={reason} root_changed_or_external_write; old object not touched"));
        }
    }
    s.last_written=None;s.root=0;
}
/// `active` means the skeleton replay currently owns a present player and is playing.
pub fn update(active:bool,speed:f64,now:u64) {
    let mut s=STATE.lock().unwrap();
    if !s.resolved {
        s.resolved=true;s.root_slot=resolve();
        crate::log_game(&format!("TIMESCALE_BINDING={} mode={} profile=EldenRing_1_17 root_slot_rva=0x{ROOT_RVA:X} scale_offset=0x{SCALE:X}",
            if s.root_slot.is_some(){"STATIC_VALIDATED"}else{"REJECTED"},if enabled(){"EXPERIMENTAL_WRITE"}else{"READ_ONLY"}));
    }
    let Some(slot)=s.root_slot else{return;};
    let Some(root)=read::<usize>(slot).filter(|p|*p>=0x10000&&*p%8==0) else {
        restore(&mut s,0,"manager unavailable");return;
    };
    let Some(scale)=read::<f32>(root+SCALE).filter(|v|v.is_finite()&&*v>=0.0&&*v<=16.0) else {
        restore(&mut s,root,"invalid timing value");return;
    };
    let delta=read::<f32>(root+DELTA);
    let second=now/1_000_000_000;
    if root!=s.observed_root || (active&&second!=s.last_log_second) {
        crate::log_game(&format!("TIMESCALE_OBSERVE root=0x{root:X} scale={scale:.6} delta={delta:?} requested={speed:.2} active={active} write_enabled={}",enabled()));
        s.observed_root=root;s.last_log_second=second;
    }
    if !enabled(){return;}
    if !active {restore(&mut s,root,"paused/stopped/unloaded/disconnected/player lost");s.inhibited=false;return;}
    // Every UI preset is supported. High speeds remain experimental until observed in-game.
    if !speed.is_finite()||!(0.1..=4.0).contains(&speed) {
        restore(&mut s,root,"invalid requested speed");s.inhibited=true;return;
    }
    if s.saved.is_some() && (s.root!=root || Some(scale)!=s.last_written) {
        restore(&mut s,root,"timing ownership changed");s.inhibited=true;
    }
    if s.inhibited{return;}
    if s.saved.is_none() {
        // Do not take over another speed mod or an engine transition with an unusual scalar.
        if (scale-1.0).abs()>0.0001 || !delta.is_some_and(|d|d.is_finite()&&d>=0.0&&d<=1.0) {
            s.inhibited=true;crate::log_game("TIMESCALE_REJECTED: expected normal scalar and plausible delta; no write");return;
        }
        s.root=root;s.saved=Some(scale);
    }
    let address=root+SCALE;
    if !accessible(address,4,true) {restore(&mut s,root,"timing storage not writable");s.inhibited=true;return;}
    let requested=speed as f32;
    if s.last_written!=Some(requested) {
        unsafe{std::ptr::write_volatile(address as *mut f32,requested)};
        s.last_written=Some(requested);
        crate::log_game(&format!("TIMESCALE_APPLY value={requested:.4} phase=ChrIns_PostPhysics"));
    }
}

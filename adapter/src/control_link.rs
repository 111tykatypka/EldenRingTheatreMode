//! Host <-> game control pipe, reduced to a status link. The host uses it to show "game connected"
//! and "player found" and to send STOP. The retired position-only replay, transform probe, traces
//! and ownership probes no longer exist: their commands are answered with status and ignored.
//! Bone replay does not use this pipe (it follows the overlay snapshot, see bone_replay.rs).
use crate::control_protocol as wire;
use std::ffi::c_void;
use std::sync::atomic::{AtomicU64,Ordering};

static COMMAND_SEQUENCE:AtomicU64=AtomicU64::new(0);
fn ready()->bool{crate::PROFILE.load(Ordering::Acquire)==crate::STATE_WAITING&&crate::INIT_STATE.load(Ordering::Acquire)==crate::STATE_READY&&crate::PRESENT.load(Ordering::Acquire)!=0}

#[link(name="kernel32")] unsafe extern "system" {
    fn CreateNamedPipeW(name:*const u16,open:u32,mode:u32,max:u32,out:u32,input:u32,timeout:u32,security:*mut c_void)->*mut c_void;
    fn ConnectNamedPipe(handle:*mut c_void,overlapped:*mut c_void)->i32;
    fn ReadFile(handle:*mut c_void,buffer:*mut c_void,size:u32,read:*mut u32,overlapped:*mut c_void)->i32;
    fn WriteFile(handle:*mut c_void,buffer:*const c_void,size:u32,written:*mut u32,overlapped:*mut c_void)->i32;
    fn DisconnectNamedPipe(handle:*mut c_void)->i32;fn CloseHandle(handle:*mut c_void)->i32;fn GetLastError()->u32;
}
fn read_exact(handle:*mut c_void,b:&mut [u8])->bool{
    let mut offset=0;
    while offset<b.len(){let mut count=0;if unsafe{ReadFile(handle,b[offset..].as_mut_ptr().cast(),(b.len()-offset) as u32,&mut count,std::ptr::null_mut())}==0||count==0{return false;}offset+=count as usize;}
    true}
fn read_packet(handle:*mut c_void)->Option<Vec<u8>>{
    let mut b=[0u8;wire::BYTES];
    if !read_exact(handle,&mut b[..wire::LEGACY_BYTES]){return None;}
    let size=match u16::from_le_bytes([b[4],b[5]]){2=>96,3=>wire::BYTES,_=>wire::LEGACY_BYTES};
    if !read_exact(handle,&mut b[wire::LEGACY_BYTES..size]){return None;}
    Some(b[..size].to_vec())}
fn status(version:u16)->wire::Packet{
    let sample=crate::latest().unwrap_or_default();
    wire::Packet{version,player_action:Default::default(),replay_state:0,replay_detail:0,session:0,replay_timestamp_ns:0,applied_sequence:0,kind:wire::STATUS,
        sequence:COMMAND_SEQUENCE.load(Ordering::Acquire),timestamp_ns:sample.timestamp_ns,position:sample.position,quaternion:sample.quaternion_xyzw,
        state:0,detail:0,flags:if ready(){1}else{0}}}
pub fn pipe_worker(){
    use std::os::windows::ffi::OsStrExt;
    let name=std::ffi::OsStr::new(wire::PIPE).encode_wide().chain(Some(0)).collect::<Vec<_>>();
    loop{
        let h=unsafe{CreateNamedPipeW(name.as_ptr(),3|0x0008_0000,0x8,1,wire::BYTES as u32,wire::BYTES as u32,0,std::ptr::null_mut())};
        if h==(-1isize as *mut c_void){crate::log_game(&format!("CONTROL_PIPE_CREATE_ERROR={}",unsafe{GetLastError()}));return;}
        if unsafe{ConnectNamedPipe(h,std::ptr::null_mut())}==0&&unsafe{GetLastError()}!=535{unsafe{CloseHandle(h)};std::thread::sleep(std::time::Duration::from_millis(100));continue;}
        COMMAND_SEQUENCE.store(0,Ordering::Release);crate::log_game("CONTROL_IPC=CONNECTED (status link)");
        while let Some(bytes)=read_packet(h){
            let Ok(packet)=wire::Packet::decode(&bytes) else {crate::log_game("CONTROL_ERROR=MALFORMED_PACKET");break;};
            if packet.sequence>COMMAND_SEQUENCE.load(Ordering::Acquire){COMMAND_SEQUENCE.store(packet.sequence,Ordering::Release);}
            if !matches!(packet.kind,wire::HELLO|wire::HEARTBEAT|wire::STOP){crate::log_game(&format!("CONTROL: retired command kind={} ignored",packet.kind));}
            let response=status(packet.version);let reply=response.encode();let mut written=0;
            if unsafe{WriteFile(h,reply.as_ptr().cast(),response.byte_count() as u32,&mut written,std::ptr::null_mut())}==0||written as usize!=response.byte_count(){break;}
        }
        crate::log_game("CONTROL_IPC=DISCONNECTED");
        unsafe{DisconnectNamedPipe(h);CloseHandle(h);}
    }
}

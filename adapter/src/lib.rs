use std::{
    ffi::{c_void, CStr},
    fs::OpenOptions,
    io::Write,
    os::windows::ffi::OsStringExt,
    path::PathBuf,
    sync::atomic::{AtomicU32, AtomicU64, Ordering},
    time::{Duration, Instant},
};
use eldenring::cs::{CSTaskGroupIndex, CSTaskImp, WorldChrMan};
use eldenring::fd4::FD4TaskData;
use fromsoftware_shared::{FromStatic, RecurringTask};
use pelite::pe64::{Pe, PeView};

mod game_profile { include!(concat!(env!("OUT_DIR"), "/game_profile.rs")); }
const STATE_WAITING:u32=0;const DLL_LOADED:u32=1;const PROFILE_VALIDATING:u32=2;const PROFILE_READY:u32=3;const TASK_SIGNATURE_SCAN:u32=10;const TASK_SIGNATURE_READY:u32=11;const TASK_RUNTIME_SEARCH:u32=12;const TASK_RUNTIME_READY:u32=13;const WORLDCHR_SEARCH:u32=20;const WORLDCHR_READY:u32=21;const PLAYER_SEARCH:u32=22;const PLAYER_FOUND:u32=23;const STATE_READY:u32=24;
const ERR_TASK_TIMEOUT:u32=0x201;const ERR_INIT_PANIC:u32=0x202;const ERR_SAMPLER_THREAD:u32=0x203;const ERR_IPC_THREAD:u32=0x204;const ERR_TASK_SIGNATURE:u32=0x205;
const TM_CHECK_PATH:u32=0x0001;const TM_CHECK_FILE_VERSION:u32=0x0002;const TM_CHECK_PRODUCT_VERSION:u32=0x0004;const TM_CHECK_ARCH:u32=0x0008;const TM_CHECK_SHA256:u32=0x0010;const TM_CHECK_IMAGE_BASE:u32=0x0020;

#[repr(C)]#[derive(Clone,Copy,Default)]pub struct PlayerSample{pub sequence:u64,pub timestamp_ns:u64,pub position:[f32;3],pub quaternion_xyzw:[f32;4],pub euler_raw:[f32;3],pub player_present:u32}
#[repr(C)]#[derive(Clone,Copy,Default)]struct WireMessage{magic:u32,version:u16,kind:u16,sequence:u64,timestamp_ns:u64,position:[f32;3],quaternion_xyzw:[f32;4],euler_raw:[f32;3],player_present:u32}
const _:()=assert!(std::mem::size_of::<WireMessage>()==72);
#[repr(C)]struct TmValidationReport{size:u32,status:u32,checked:u32,passed:u32,file_version:[u16;4],product_version:[u16;4],machine:u16,reserved:u16,image_base:usize,runtime_path:[u16;32768],sha256:[i8;65]}
#[link(name="GameProfile",kind="static")]unsafe extern "C"{fn tm_validate_profile(path:*const u16,image_base:usize,report:*mut TmValidationReport)->u32;}
#[link(name="kernel32")]unsafe extern "system"{fn GetModuleFileNameW(module:*mut c_void,buffer:*mut u16,size:u32)->u32;fn GetModuleHandleW(name:*const u16)->*mut c_void;fn GetCurrentProcessId()->u32;fn GetTickCount64()->u64;}
static SEQ:AtomicU64=AtomicU64::new(0);static TIME:AtomicU64=AtomicU64::new(0);static PRESENT:AtomicU32=AtomicU32::new(0);static PROFILE:AtomicU32=AtomicU32::new(STATE_WAITING);static INIT_STATE:AtomicU32=AtomicU32::new(STATE_WAITING);static VALUES:[AtomicU32;10]=[const{AtomicU32::new(0)};10];
fn log_game(message:&str){let path=std::env::temp_dir().join("TheaterModeGame.log");if let Ok(mut file)=OpenOptions::new().create(true).append(true).open(path){let _=writeln!(file,"{}",message);}}
fn set_state(state:u32,label:&str){let old=INIT_STATE.swap(state,Ordering::AcqRel);if old!=state{log_game(&format!("INIT_STATE={label} ({state})"));}}
type RegisterTaskFn=unsafe extern "C" fn(&CSTaskImp,CSTaskGroupIndex,&RecurringTask<FD4TaskData>);
const REGISTER_TASK_PATTERN:&[pelite::pattern::Atom]=pelite::pattern!("e8 ? ? ? ? 48 8b 0d ? ? ? ? 4c 8b c7 8b d3 e8 $ { ' }");
#[cfg(windows)]unsafe fn resolve_register_task()->Result<(RegisterTaskFn,u32),String>{
    let base=unsafe{GetModuleHandleW(std::ptr::null())} as usize;if base==0{return Err("eldenring.exe module base is null".into());}
    let pe=unsafe{PeView::module(base as *const u8)};let mut captures=[0u32;2];
    if !pe.scanner().finds_code(REGISTER_TASK_PATTERN,&mut captures){return Err("register_task signature missing or ambiguous".into());}
    let rva=captures[1];if rva==0{return Err("register_task signature resolved null RVA".into());}
    Ok((unsafe{std::mem::transmute::<usize,RegisterTaskFn>(base+rva as usize)},rva))
}
#[cfg(not(windows))]unsafe fn resolve_register_task()->Result<(RegisterTaskFn,u32),String>{Err("Windows runtime required".into())}
fn publish(s:PlayerSample){SEQ.fetch_add(1,Ordering::AcqRel);TIME.store(s.timestamp_ns,Ordering::Relaxed);PRESENT.store(s.player_present,Ordering::Relaxed);let values=[s.position[0],s.position[1],s.position[2],s.quaternion_xyzw[0],s.quaternion_xyzw[1],s.quaternion_xyzw[2],s.quaternion_xyzw[3],s.euler_raw[0],s.euler_raw[1],s.euler_raw[2]];for(dst,value)in VALUES.iter().zip(values){dst.store(value.to_bits(),Ordering::Relaxed);}SEQ.fetch_add(1,Ordering::Release);}
fn latest()->Option<PlayerSample>{loop{let before=SEQ.load(Ordering::Acquire);if before&1!=0{std::hint::spin_loop();continue;}let mut v=[0.0;10];for(dst,src)in v.iter_mut().zip(VALUES.iter()){*dst=f32::from_bits(src.load(Ordering::Relaxed));}let t=TIME.load(Ordering::Relaxed);let present=PRESENT.load(Ordering::Relaxed);let after=SEQ.load(Ordering::Acquire);if before==after{return if after==0{None}else{Some(PlayerSample{sequence:after/2,timestamp_ns:t,position:[v[0],v[1],v[2]],quaternion_xyzw:[v[3],v[4],v[5],v[6]],euler_raw:[v[7],v[8],v[9]],player_present:present})};}}}
#[unsafe(no_mangle)]pub unsafe extern "C" fn theater_get_latest_sample(out:*mut PlayerSample)->bool{if out.is_null(){return false;}if let Some(sample)=latest(){unsafe{out.write(sample);}true}else{false}}
fn version(v:[u16;4])->String{format!("{}.{}.{}.{}",v[0],v[1],v[2],v[3])}
fn check_text(report:&TmValidationReport,flag:u32)->&'static str{if report.checked&flag==0{"UNAVAILABLE"}else if report.passed&flag!=0{"PASS"}else{"FAIL"}}
#[cfg(windows)]fn validate_runtime_profile()->u32{
    let mut path=[0u16;32768];let n=unsafe{GetModuleFileNameW(std::ptr::null_mut(),path.as_mut_ptr(),path.len() as u32)};
    if n==0||n as usize>=path.len(){log_game("Runtime executable: UNAVAILABLE (GetModuleFileNameW failed)");log_game("Selected profile: NONE; expected profile: EldenRing_1_17; profile validation: FAIL (runtime path unavailable)");return 0x101;}
    let mut report:TmValidationReport=unsafe{std::mem::zeroed()};let image_base=unsafe{GetModuleHandleW(std::ptr::null())} as usize;
    let status=unsafe{tm_validate_profile(path.as_ptr(),image_base,&mut report)};
    let runtime_path=std::ffi::OsString::from_wide(&report.runtime_path[..report.runtime_path.iter().position(|c|*c==0).unwrap_or(report.runtime_path.len())]);
    let hash=unsafe{CStr::from_ptr(report.sha256.as_ptr())}.to_string_lossy();
    log_game(&format!("Runtime executable: {}",PathBuf::from(runtime_path).display()));
    log_game(&format!("Expected executable: {}",game_profile::EXPECTED_EXE_PATH));
    log_game(&format!("Runtime file version: {}; expected: {}",version(report.file_version),game_profile::EXPECTED_FILE_VERSION_STR));
    log_game(&format!("Runtime product version: {}; expected: {}",version(report.product_version),game_profile::EXPECTED_PRODUCT_VERSION_STR));
    log_game(&format!("Runtime architecture: {}; TheaterMode.dll architecture: AMD64 (x86_64-pc-windows-msvc)",if report.machine==0x8664{"AMD64"}else if report.machine==0x14c{"x86"}else{"OTHER/UNKNOWN"}));
    log_game(&format!("Runtime SHA-256 (file on disk, not loaded PE image): {}; expected: {}",hash,game_profile::EXPECTED_SHA256_HEX));
    log_game(&format!("Loaded executable image base: 0x{:X}",report.image_base));
    log_game(&format!("Profile checks: path={}, file_version={}, product_version={}, architecture={}, SHA256={}, image_base={}",check_text(&report,TM_CHECK_PATH),check_text(&report,TM_CHECK_FILE_VERSION),check_text(&report,TM_CHECK_PRODUCT_VERSION),check_text(&report,TM_CHECK_ARCH),check_text(&report,TM_CHECK_SHA256),check_text(&report,TM_CHECK_IMAGE_BASE)));
    log_game(&format!("Selected profile: {}; expected profile: {}",if status==0{game_profile::PROFILE_NAME}else{"NONE"},game_profile::PROFILE_NAME));
    log_game(&format!("Profile validation: {}; error code=0x{:X}; identity hash scope=file on disk",if status==0{"PASS"}else{"FAIL"},status));
    if status==0{log_game("Runtime signatures/task validation: pending CSTaskImp initialization");}status
}
#[cfg(not(windows))]fn validate_runtime_profile()->u32{0x101}
#[cfg(windows)]fn pipe_worker(){use std::os::windows::ffi::OsStrExt;type Handle=*mut c_void;#[link(name="kernel32")]unsafe extern "system"{fn CreateFileW(n:*const u16,a:u32,s:u32,sa:*mut c_void,c:u32,f:u32,t:Handle)->Handle;fn WriteFile(h:Handle,b:*const c_void,n:u32,w:*mut u32,o:*mut c_void)->i32;fn CloseHandle(h:Handle)->i32;fn Sleep(ms:u32);}const INVALID:Handle=-1isize as Handle;let name=std::ffi::OsStr::new(r"\\.\pipe\EldenRingTheaterMode_1_17").encode_wide().chain(Some(0)).collect::<Vec<_>>();let handle=loop{let h=unsafe{CreateFileW(name.as_ptr(),0x40000000,0,std::ptr::null_mut(),3,0,std::ptr::null_mut())};if h!=INVALID{break h;}unsafe{Sleep(500)}};let mut last=0u64;let mut last_state=u32::MAX;let mut last_send=std::time::Instant::now()-Duration::from_secs(3);let mut ready_sent=false;loop{let state=INIT_STATE.load(Ordering::Acquire);let err=PROFILE.load(Ordering::Acquire);if err!=STATE_WAITING{let msg=WireMessage{magic:0x544D5354,version:1,kind:5,sequence:err as u64,..Default::default()};let mut written=0;unsafe{WriteFile(handle,&msg as*const _ as*const c_void,std::mem::size_of::<WireMessage>() as u32,&mut written,std::ptr::null_mut());CloseHandle(handle);}return;}if state==STATE_WAITING{unsafe{Sleep(100)};continue;}if state!=STATE_READY{if state!=last_state||last_send.elapsed()>=Duration::from_secs(1){let msg=WireMessage{magic:0x544D5354,version:1,kind:4,sequence:state as u64,..Default::default()};let mut written=0;if unsafe{WriteFile(handle,&msg as*const _ as*const c_void,std::mem::size_of::<WireMessage>() as u32,&mut written,std::ptr::null_mut())}==0{break;}last_state=state;last_send=std::time::Instant::now();}unsafe{Sleep(50)};continue;}let msg=if let Some(s)=latest(){if s.sequence==last&&last_send.elapsed()<Duration::from_secs(2){unsafe{Sleep(8)};continue;}last=s.sequence;ready_sent=true;WireMessage{magic:0x544D5354,version:1,kind:if s.player_present!=0{2}else{3},sequence:s.sequence,timestamp_ns:s.timestamp_ns,position:s.position,quaternion_xyzw:s.quaternion_xyzw,euler_raw:s.euler_raw,player_present:s.player_present}}else{if ready_sent&&last_send.elapsed()<Duration::from_secs(2){unsafe{Sleep(8)};continue;}ready_sent=true;WireMessage{magic:0x544D5354,version:1,kind:1,..Default::default()}};let mut written=0;let ok=unsafe{WriteFile(handle,&msg as*const _ as*const c_void,std::mem::size_of::<WireMessage>() as u32,&mut written,std::ptr::null_mut())};if ok==0||written as usize!=std::mem::size_of::<WireMessage>(){break;}last_send=std::time::Instant::now();unsafe{Sleep(8)}}unsafe{CloseHandle(handle)};}
#[unsafe(no_mangle)]
pub unsafe extern "system" fn DllMain(_module:usize,reason:u32,_reserved:usize)->i32 {
    if reason==1 {
        log_game(&format!("DLL load timestamp={:?} PID={} module_base=0x{:X}",std::time::SystemTime::now(),unsafe{GetCurrentProcessId()},_module));
        set_state(DLL_LOADED,"DLL_LOADED");
        let sampler=std::thread::Builder::new().name("TheaterMode.Sampler".into()).spawn(|| {
            let outcome=std::panic::catch_unwind(|| {
                set_state(PROFILE_VALIDATING,"PROFILE_VALIDATING");
                let error=validate_runtime_profile();
                if error!=0 {
                    PROFILE.store(error,Ordering::Release);
                    log_game(&format!("Runtime profile rejection; code 0x{error:X}"));
                    return;
                }
                log_game("Runtime profile accepted: EldenRing_1_17 / WW 2.7.0.0");
                set_state(PROFILE_READY,"PROFILE_READY");
                set_state(TASK_SIGNATURE_SCAN,"TASK_SIGNATURE_SCAN");
                log_game("Task signature scan start: ERSoundBankLoader register_task pattern; profile=EldenRing_1_17 / WW_2.7.0.0");
                let (register_task,register_rva)=match unsafe{resolve_register_task()}{Ok(found)=>found,Err(error)=>{log_game(&format!("Task signature scan FAIL: {error}"));PROFILE.store(ERR_TASK_SIGNATURE,Ordering::Release);return;}};
                log_game(&format!("Task signature scan PASS: unique register_task at eldenring.exe+0x{register_rva:X}; signature=E8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? 4C 8B C7 8B D3 E8"));
                set_state(TASK_SIGNATURE_READY,"TASK_SIGNATURE_READY");
                set_state(TASK_RUNTIME_SEARCH,"TASK_RUNTIME_SEARCH");
                let start=Instant::now();let timeout=Duration::from_secs(120);let mut next_log=Duration::ZERO;let mut attempts=0u64;let task=loop {
                    attempts+=1;
                    match unsafe{CSTaskImp::instance()} {
                        Ok(task)=>break task,
                        Err(error)=>{
                            let diagnostic=match error{fromsoftware_shared::InstanceError::NotFound(name)=>format!("singleton not found: {name}"),fromsoftware_shared::InstanceError::Null(name)=>format!("singleton not initialized: {name}")};
                            if start.elapsed()>=timeout{log_game(&format!("CSTaskImp lookup TIMEOUT after {}s; attempts={attempts}; last={diagnostic}; preceding wait_for_system_init gate intentionally skipped; reflected lookup did not return instance",timeout.as_secs()));PROFILE.store(ERR_TASK_TIMEOUT,Ordering::Release);return;}
                            if start.elapsed()>=next_log{log_game(&format!("CSTaskImp wait attempt={attempts}; elapsed_ms={}; {diagnostic}",start.elapsed().as_millis()));next_log=start.elapsed()+Duration::from_secs(2);}
                            std::thread::sleep(Duration::from_millis(10));
                        }
                    }
                };
                log_game(&format!("CSTaskImp singleton READY after {}ms; attempts={attempts}; instance={:p}",start.elapsed().as_millis(),task));
                set_state(TASK_RUNTIME_READY,"TASK_RUNTIME_READY");
                set_state(WORLDCHR_SEARCH,"WORLDCHR_SEARCH");
                let world_ready=std::sync::atomic::AtomicBool::new(false);let player_found=std::sync::atomic::AtomicBool::new(false);
                let callback=RecurringTask::new(move |_:&FD4TaskData| {
                    let now=unsafe{GetTickCount64()}*1_000_000;
                    if let Ok(world)=unsafe{WorldChrMan::instance()} {
                        if !world_ready.swap(true,Ordering::AcqRel){log_game(&format!("WorldChrMan READY; instance={:p}",world));set_state(WORLDCHR_READY,"WORLDCHR_READY");set_state(PLAYER_SEARCH,"PLAYER_SEARCH");}
                        if let Some(player)=world.main_player.as_ref() {
                            let p=&player.chr_ins.modules.physics;
                            let pos=[p.position.0,p.position.1,p.position.2];
                            let q=[p.orientation.0,p.orientation.1,p.orientation.2,p.orientation.3];
                            let e=[p.orientation_euler.0,p.orientation_euler.1,p.orientation_euler.2];
                            if pos.iter().chain(q.iter()).chain(e.iter()).all(|v|v.is_finite()) {
                                if !player_found.swap(true,Ordering::AcqRel){log_game(&format!("main_player FOUND; PlayerIns={:p}; first_position=({:.3},{:.3},{:.3})",player,pos[0],pos[1],pos[2]));set_state(PLAYER_FOUND,"PLAYER_FOUND");}
                                publish(PlayerSample{sequence:0,timestamp_ns:now,position:pos,quaternion_xyzw:q,euler_raw:e,player_present:1});
                                set_state(STATE_READY,"READY");
                                return;
                            }
                        }
                    }
                    publish(PlayerSample{sequence:0,timestamp_ns:now,player_present:0,..Default::default()});
                    if world_ready.load(Ordering::Acquire){set_state(PLAYER_SEARCH,"PLAYER_SEARCH");}
                });
                let callback=Box::leak(Box::new(callback));
                unsafe{register_task(task,CSTaskGroupIndex::ChrIns_PostPhysics,callback);}
                log_game(&format!("Recurring task registered using resolved function eldenring.exe+0x{register_rva:X}; group=ChrIns_PostPhysics; waiting for WORLDCHR_READY and PLAYER_FOUND"));
                loop { std::thread::sleep(Duration::from_secs(60)); }
            });
            if outcome.is_err() {
                log_game("Initialization panic caught; module failed closed (0x202)");
                PROFILE.store(ERR_INIT_PANIC,Ordering::Release);
            }
        });
        if sampler.is_err() {
            log_game("Sampler worker thread creation failed");
            PROFILE.store(ERR_SAMPLER_THREAD,Ordering::Release);
        }
        let ipc=std::thread::Builder::new().name("TheaterMode.IPC".into()).spawn(pipe_worker);
        if ipc.is_err() {
            log_game("IPC worker thread creation failed");
            PROFILE.store(ERR_IPC_THREAD,Ordering::Release);
        }
    }
    1
}


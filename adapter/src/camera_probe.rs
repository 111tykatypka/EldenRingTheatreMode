//! Read-only SDK camera candidates. No ownership changes or game camera writes.
use eldenring::cs::{CSCamera,CSCam};
use fromsoftware_shared::FromStatic;
#[repr(C)]
#[derive(Clone,Copy,Default)]
struct Slot { matrix:[f32;16], fov:f32, aspect:f32, near_plane:f32, far_plane:f32, valid:u32 }
#[repr(C)]
#[derive(Default)]
struct Telemetry { timestamp_ns:u64, mask:u32, available:u32, slots:[Slot;4] }
const _:()=assert!(std::mem::size_of::<Telemetry>()==352);
const _:()=assert!(std::mem::offset_of!(CSCam,fov)==std::mem::offset_of!(CSCam,matrix)+64);
const _:()=assert!(std::mem::offset_of!(CSCam,far_plane)==std::mem::offset_of!(CSCam,matrix)+76);
unsafe extern "C" { fn tm_camera_publish(snapshot:*const Telemetry); fn tm_camera_probe_enabled()->bool;fn tm_camera_runtime_diagnostic(out:*mut i8,size:usize)->i32; }
pub fn tick(now:u64) {
 static LAST_RUNTIME_LOG:std::sync::atomic::AtomicU64=std::sync::atomic::AtomicU64::new(0);
 if now.saturating_sub(LAST_RUNTIME_LOG.load(std::sync::atomic::Ordering::Relaxed))>=2_000_000_000 {
  LAST_RUNTIME_LOG.store(now,std::sync::atomic::Ordering::Relaxed);let mut text=[0i8;512];
  if unsafe{tm_camera_runtime_diagnostic(text.as_mut_ptr(),text.len())}!=0{crate::log_game(&unsafe{std::ffi::CStr::from_ptr(text.as_ptr())}.to_string_lossy());}
 }
 if !unsafe{tm_camera_probe_enabled()}{return;}
 let mut out=Telemetry { timestamp_ns:now,..Default::default() };
 if let Ok(camera)=CSCamera::instance_ptr() {
  let base=camera as usize;
  if let Some(mask)=crate::companions::dword(base+std::mem::offset_of!(CSCamera,camera_mask)) {
   out.available=1;out.mask=mask;
   let offsets=[std::mem::offset_of!(CSCamera,pers_cam_1),std::mem::offset_of!(CSCamera,pers_cam_2),std::mem::offset_of!(CSCamera,pers_cam_3),std::mem::offset_of!(CSCamera,pers_cam_4)];
   for(dst,offset)in out.slots.iter_mut().zip(offsets) {
    let Some(address)=crate::companions::word(base+offset) else{continue};
    // The pinned CSCam fields are contiguous. Copy bytes; never dereference an
    // unchecked camera object or cache a slot pointer across callbacks.
    let mut bytes=[0u8;80];
    if !crate::companions::copy(address.saturating_add(std::mem::offset_of!(CSCam,matrix)),&mut bytes){continue;}
    let mut values=[0f32;20];for(v,b)in values.iter_mut().zip(bytes.chunks_exact(4)){*v=f32::from_le_bytes(b.try_into().unwrap());}
    dst.matrix.copy_from_slice(&values[..16]);dst.fov=values[16];dst.aspect=values[17];dst.near_plane=values[18];dst.far_plane=values[19];dst.valid=1;
   }
  }
 }
 // Publish a bounded copied structure; render-side mutex acquisition is nonblocking.
 unsafe { tm_camera_publish(&out); }
 static LAST_LOG:std::sync::atomic::AtomicU64=std::sync::atomic::AtomicU64::new(0);
 let last=LAST_LOG.load(std::sync::atomic::Ordering::Relaxed);
 if now.saturating_sub(last)>=2_000_000_000 {
  LAST_LOG.store(now,std::sync::atomic::Ordering::Relaxed);
  crate::log_game(&format!("CAMERA_PROBE read_only=1 available={} mask=0x{:X} active_owner=UNKNOWN",out.available,out.mask));
  for(i,s)in out.slots.iter().enumerate(){crate::log_game(&format!("CAMERA_PROBE slot={} copied={} position={:?} fov_raw={} aspect={} near={} far={}",i+1,s.valid,&s.matrix[12..15],s.fov,s.aspect,s.near_plane,s.far_plane));}
 }
}

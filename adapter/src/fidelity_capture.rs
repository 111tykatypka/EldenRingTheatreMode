//! Read-only fidelity probe. Named fields copied through ReadProcessMemory.
//! New field semantics/layout confidence stays REFERENCE until live correlation.
use std::{ffi::c_void,mem::size_of,sync::{OnceLock,Mutex,mpsc::{sync_channel,SyncSender,Receiver},atomic::{AtomicBool,AtomicU64,Ordering}}};
use eldenring::cs::*;
include!("capture_fields.rs");
#[derive(Clone,Copy)]
pub struct Frame {pub schema:u32,pub words:u32,pub drops:u64,pub valid:[u32;MASK_WORDS],pub values:[u32;WORDS]}
impl Default for Frame {fn default()->Self{Self{schema:1,words:WORDS as u32,drops:0,valid:[0;MASK_WORDS],values:[0;WORDS]}}}
#[link(name="kernel32")]unsafe extern "system"{fn GetCurrentProcess()->*mut c_void;fn ReadProcessMemory(process:*mut c_void,source:*const c_void,target:*mut c_void,size:usize,read:*mut usize)->i32;}
fn copy(source:*const c_void,target:*mut c_void,size:usize)->bool{if source.is_null(){return false;}let mut read=0;unsafe{ReadProcessMemory(GetCurrentProcess(),source,target,size,&mut read)!=0&&read==size}}
unsafe fn read_pointer<T>(p:*const T)->Option<usize>{if size_of::<T>()!=8{return None;}let mut value=0usize;if copy(p.cast(),(&mut value as *mut usize).cast(),8){Some(value)}else{None}}
trait Owner{unsafe fn owner_ptr(p:*const Self)->*const std::ptr::NonNull<ChrIns>;}
macro_rules! owner {($($t:ty),*)=>{$(impl Owner for $t{unsafe fn owner_ptr(p:*const Self)->*const std::ptr::NonNull<ChrIns>{unsafe{std::ptr::addr_of!((*p).owner)}}})*};}
owner!(CSChrPhysicsModule,CSChrBehaviorModule,CSChrTimeActModule,CSChrEventModule,CSChrActionRequestModule,CSChrBehaviorDataModule,CSChrDataModule,CSChrFallModule,CSChrSuperArmorModule,CSChrToughnessModule,CSChrActionFlagModule,ChrCtrl);
fn owned<T:Owner>(p:*const T,owner:usize)->bool{!p.is_null()&&unsafe{read_pointer(T::owner_ptr(p))}==Some(owner)}
fn modifier_owned(p:*const ChrCtrlModifier,owner:usize)->bool{!p.is_null()&&unsafe{read_pointer(std::ptr::addr_of!((*p).owner))}==Some(owner)}
impl Frame{
 unsafe fn read<T>(&mut self,p:*const T,start:usize,words:usize){let bytes=size_of::<T>();if bytes>words*4||start+words>WORDS{return;}
  if copy(p.cast(),self.values[start..].as_mut_ptr().cast(),bytes){for n in start..start+words{self.valid[n/32]|=1<<(n%32);}}
 }
 pub fn encode(&self)->[u8;WIRE_BYTES]{let mut b=[0;WIRE_BYTES];b[..4].copy_from_slice(&self.schema.to_le_bytes());b[4..8].copy_from_slice(&self.words.to_le_bytes());b[8..16].copy_from_slice(&self.drops.to_le_bytes());for(i,v)in self.valid.iter().chain(self.values.iter()).enumerate(){b[16+i*4..20+i*4].copy_from_slice(&v.to_le_bytes());}b}
}
#[derive(Clone,Copy)]pub struct Packet{pub sample:crate::PlayerSample,pub action:crate::player_action::State,pub capture:Frame}
static CONNECTED:AtomicBool=AtomicBool::new(false);
static DROPS:AtomicU64=AtomicU64::new(0);
static CHANNEL:OnceLock<(SyncSender<Packet>,Mutex<Option<Receiver<Packet>>>)>=OnceLock::new();
pub fn queue_capacity()->usize{static CAPACITY:OnceLock<usize>=OnceLock::new();*CAPACITY.get_or_init(||std::env::var("THEATER_CAPTURE_QUEUE_FRAMES").ok().and_then(|s|s.parse::<usize>().ok()).filter(|n|*n>0).unwrap_or(4096))}
fn channel()->&'static(SyncSender<Packet>,Mutex<Option<Receiver<Packet>>>){CHANNEL.get_or_init(||{let(tx,rx)=sync_channel(queue_capacity());(tx,Mutex::new(Some(rx)))})}
pub fn initialize(){let _=channel();for line in layout_report().lines(){crate::log_game(line);}}
pub fn receiver()->Receiver<Packet>{channel().1.lock().unwrap().take().expect("one player pipe worker")}
pub fn connected(value:bool){CONNECTED.store(value,Ordering::Release);}
static LAST_STATS:AtomicU64=AtomicU64::new(0);
static READ_NS:AtomicU64=AtomicU64::new(0);
static READ_COUNT:AtomicU64=AtomicU64::new(0);
pub fn publish(sample:crate::PlayerSample,action:crate::player_action::State,player:Option<&PlayerIns>){if !CONNECTED.load(Ordering::Acquire){return;}let started=crate::monotonic_ns();let mut capture=player.map(observe).unwrap_or_default();let elapsed=crate::monotonic_ns().saturating_sub(started);READ_NS.fetch_add(elapsed,Ordering::Relaxed);READ_COUNT.fetch_add(1,Ordering::Relaxed);if started.saturating_sub(LAST_STATS.load(Ordering::Relaxed))>=1_000_000_000{LAST_STATS.store(started,Ordering::Relaxed);let count=READ_COUNT.swap(0,Ordering::Relaxed);let total=READ_NS.swap(0,Ordering::Relaxed);crate::log_game(&format!("CAPTURE_STATS callbacks={} mean_read_us={:.3} available_words={}/{} queue_drops={} confidence=REFERENCE",count,total as f64/count.max(1) as f64/1000.0,capture.valid.iter().map(|v|v.count_ones()).sum::<u32>(),WORDS,DROPS.load(Ordering::Relaxed)));}capture.drops=DROPS.load(Ordering::Relaxed);let packet=Packet{sample,action,capture};if channel().0.try_send(packet).is_err(){DROPS.fetch_add(1,Ordering::Relaxed);}}
#[cfg(test)]mod tests{use super::*;
 #[test]fn sdk_layout_inventory(){assert_eq!(std::mem::offset_of!(CSChrPhysicsModule,position),0x70);assert_eq!(std::mem::offset_of!(CSChrPhysicsModule,orientation),0x50);assert_eq!(std::mem::offset_of!(CSChrBehaviorModule,animation_speed),0x17c8);assert_eq!(std::mem::offset_of!(ChrCtrl,chr_proxy_flags),0xfc);println!("{}",layout_report());}
 #[test]fn wire_preserves_nonfinite_bits_and_availability(){let mut f=Frame::default();f.values[7]=0x7fc01234;f.valid[0]=1<<7;f.drops=9;let b=f.encode();assert_eq!(b.len(),WIRE_BYTES);assert_eq!(u64::from_le_bytes(b[8..16].try_into().unwrap()),9);assert_eq!(u32::from_le_bytes(b[16+4*MASK_WORDS+28..20+4*MASK_WORDS+28].try_into().unwrap()),0x7fc01234);}
 #[test]fn unreadable_fields_are_unavailable_not_zero_state(){let mut f=Frame::default();unsafe{f.read(std::ptr::null::<u64>(),0,2);}assert_eq!(f.valid,[0;MASK_WORDS]);let data=123u64;unsafe{f.read(&data,0,2);}assert_eq!(f.valid[0]&3,3);}
}

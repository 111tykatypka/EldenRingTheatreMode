//! Exact-build, read-only native recorder/actor evidence. No engine virtual calls.
//! Bounded game-thread snapshots; file/log writes belong to the worker.
use eldenring::cs::{WorldChrMan,ChrIns,ChrCtrl,PlayerIns,ReplayRecorder,ChrSetEntry,ChrInsModuleContainer,CSChrPhysicsModule};
use fromsoftware_shared::FromStatic;
use std::{ffi::c_void,mem::{offset_of,size_of},sync::mpsc::{sync_channel,SyncSender},io::Write};
const RECORDER_VTABLE:usize=0x2a4aa40;
const RECORDER_BYTES:usize=0x860;
const NODE_BYTES:usize=0x248;
const MAX_GHOST_SLOTS:usize=512; // diagnostic sampling budget, not a replay actor cap
#[link(name="kernel32")]unsafe extern "system"{
 fn GetCurrentProcess()->*mut c_void;
 fn ReadProcessMemory(process:*mut c_void,address:*const c_void,output:*mut c_void,size:usize,read:*mut usize)->i32;
}
fn read(address:usize,size:usize)->Option<Vec<u8>>{
 if address<0x10000||size==0||size>0x4000||address.checked_add(size).is_none(){return None;}
 let mut bytes=vec![0;size];let mut got=0;
 if unsafe{ReadProcessMemory(GetCurrentProcess(),address as *const c_void,bytes.as_mut_ptr().cast(),size,&mut got)}!=0&&got==size{Some(bytes)}else{None}
}
fn u64_at(b:&[u8],i:usize)->Option<u64>{Some(u64::from_le_bytes(b.get(i..i.checked_add(8)?)?.try_into().ok()?))}
fn u32_at(b:&[u8],i:usize)->Option<u32>{Some(u32::from_le_bytes(b.get(i..i.checked_add(4)?)?.try_into().ok()?))}
fn pointer(b:&[u8],i:usize)->usize{u64_at(b,i).unwrap_or(0) as usize}
fn hex(b:&[u8])->String{use std::fmt::Write;let mut s=String::with_capacity(b.len()*2);for v in b{let _=write!(s,"{v:02x}");}s}
fn json_quote(s:&str)->String{format!("\"{}\"",s.replace('\\',"\\\\").replace('"',"\\\"").replace('\n',"\\n").replace('\r',"\\r"))}
fn class_name(address:usize,base:usize,image_size:usize)->String{
 let Some(object)=read(address,8)else{return "UNREADABLE".into()};let vtable=pointer(&object,0);
 let in_image=|a:usize,n:usize|a>=base&&a.checked_add(n).is_some_and(|e|e<=base.saturating_add(image_size));
 if !in_image(vtable,8)||vtable<8{return "VTABLE_OUTSIDE_TARGET".into();}
 let Some(slot)=read(vtable-8,8)else{return "NO_COL".into()};let col=pointer(&slot,0);
 if !in_image(col,24){return "COL_OUTSIDE_TARGET".into();}
 let Some(c)=read(col,24)else{return "NO_COL".into()};
 if u32_at(&c,0)!=Some(1)||base.checked_add(u32_at(&c,20).unwrap_or(0)as usize)!=Some(col){return "COL_INVALID".into();}
 let td=base.saturating_add(u32_at(&c,12).unwrap_or(0)as usize);
 if !in_image(td+16,128){return "TYPE_OUTSIDE_TARGET".into();}
 let Some(name)=read(td+16,128)else{return "NO_TYPE".into()};let end=name.iter().position(|v|*v==0).unwrap_or(128);
 if !name[..end].iter().all(|v|(32..127).contains(v)){return "TYPE_NON_ASCII".into();}
 String::from_utf8_lossy(&name[..end]).into_owned()
}
// Read vtable slot bytes rather than invoking pinned SDK's incompatible reference-return ABI.
fn manipulator_kind(address:usize,base:usize,image_size:usize)->Option<u32>{
 let m=read(address,8)?;let vt=pointer(&m,0);if vt<base||vt.checked_add(24)?>base.checked_add(image_size)?{return None;}
 let slot=read(vt+16,8)?;let function=pointer(&slot,0);
 if function<base||function.checked_add(6)?>base.checked_add(image_size)?{return None;}
 let code=read(function,6)?;literal_kind(&code)
}
fn literal_kind(code:&[u8])->Option<u32>{if code.len()!=6||code[0]!=0xb8||code[5]!=0xc3{return None;}let n=u32_at(code,1)?;(n<=7).then_some(n)}
pub fn write_command(kind:u16)->bool{matches!(kind,3|5|6|7|11|14|15)}
struct Row{prefix:&'static str,message:String,address:usize,bytes:Vec<u8>}
struct Batch{timestamp:u64,rows:Vec<Row>,dropped:u64}
pub struct Capture{tx:Option<SyncSender<Batch>>,next:u64,base:usize,image_size:usize,dropped:u64}
impl Capture{
 pub fn new()->Self{
  if !cfg!(feature="native-bloodstain-readonly"){return Self{tx:None,next:0,base:0,image_size:0,dropped:0};}
  let base=unsafe{crate::GetModuleHandleW(std::ptr::null())}as usize;
  let image_size=read(base,0x1000).and_then(|h|{let nt=u32_at(&h,0x3c)?as usize;u32_at(&h,nt+24+56)}).unwrap_or(0)as usize;
  let(tx,rx)=sync_channel::<Batch>(8);
  let started=std::thread::Builder::new().name("TheaterMode.NativeReplayEvidence".into()).spawn(move||{
   let root=std::env::var_os("LOCALAPPDATA").map(std::path::PathBuf::from).unwrap_or_else(std::env::temp_dir).join("EldenRingTheaterMode/native-replay");
   if let Err(e)=std::fs::create_dir_all(&root){crate::log_game(&format!("NATIVE_REPLAY: STORAGE_ERROR {e}"));return;}
   let path=root.join(format!("native_replay_{}_{}.jsonl",std::process::id(),crate::monotonic_ns()));
   let Ok(mut file)=std::fs::File::create(&path)else{crate::log_game("NATIVE_REPLAY: STORAGE_ERROR create journal");return;};
   crate::log_game(&format!("NATIVE_REPLAY: READONLY=ON profile=EldenRing_1_17 interval=1s; engine calls/writes blocked; journal={}",path.display()));
   while let Ok(batch)=rx.recv(){for row in batch.rows{
    crate::log_game(&format!("{}: t={} {}",row.prefix,batch.timestamp,row.message));
    let line=format!("{{\"schema\":1,\"time_ns\":{},\"source_drops\":{},\"prefix\":{},\"address\":\"0x{:x}\",\"message\":{},\"raw_hex\":{}}}\n",batch.timestamp,batch.dropped,json_quote(row.prefix),row.address,json_quote(&row.message),json_quote(&hex(&row.bytes)));
    if let Err(e)=file.write_all(line.as_bytes()){crate::log_game(&format!("NATIVE_REPLAY: JOURNAL_ERROR {e}"));return;}
   }if let Err(e)=file.flush(){crate::log_game(&format!("NATIVE_REPLAY: FLUSH_ERROR {e}"));return;}}
  });
  if started.is_err(){crate::log_game("NATIVE_REPLAY: WORKER_UNAVAILABLE");return Self{tx:None,next:0,base,image_size,dropped:0};}
  Self{tx:Some(tx),next:0,base,image_size,dropped:0}
 }
 pub fn tick(&mut self,now:u64){
  let Some(tx)=self.tx.as_ref()else{return;};if now<self.next{return;}self.next=now.saturating_add(1_000_000_000);
  let mut rows=Vec::new();
  if let Ok(world)=unsafe{WorldChrMan::instance()}{
   if let Some(player)=world.main_player.as_ref(){
    let address=&**player as *const PlayerIns as usize;
    self.actor(&mut rows,address,"LOCAL",-1,0);
    let recorder=read(address+offset_of!(PlayerIns,replay_recorder),8).map(|b|pointer(&b,0)).unwrap_or(0);
    if let Some(raw)=read(recorder,RECORDER_BYTES){
     let owner=pointer(&raw,0x10);let vt=pointer(&raw,0);let matched=vt.checked_sub(self.base)==Some(RECORDER_VTABLE)&&owner==address;
     let r=|i|u32_at(&raw,i).unwrap_or(0);let floats=[f32::from_bits(r(0x4c)),f32::from_bits(r(0x50)),f32::from_bits(r(0x54))];
     rows.push(Row{prefix:"REPLAY_RECORDER",address:recorder,message:format!("player=0x{address:X} recorder=0x{recorder:X} vtable_rva={:?} owner_matches={} prefix_names=REFERENCE raw40={} frame44={} raw48=0x{:08X} elapsed48_as_f32={} oldest={floats:?} yaw58={} block5c={} unknown=[8:{:X},18:{:X},20:{:X},28:{:X},30:{:X},38:{:X},60:{:X},64:{:X},68:{:X},6c:{:X}] full_object_read={} codec=UNVERIFIED",vt.checked_sub(self.base),owner==address,r(0x40),r(0x44),r(0x48),f32::from_bits(r(0x48)),f32::from_bits(r(0x58)),r(0x5c)as i32,pointer(&raw,8),pointer(&raw,0x18),pointer(&raw,0x20),pointer(&raw,0x28),pointer(&raw,0x30),pointer(&raw,0x38),r(0x60),r(0x64),r(0x68),r(0x6c),matched),bytes:if matched{raw.clone()}else{Vec::new()}});
     if matched{for(role,offset)in [("active_head",0x20),("active_tail",0x28),("free_head",0x30)]{
      let node=pointer(&raw,offset);if let Some(b)=read(node,NODE_BYTES){rows.push(Row{prefix:"REPLAY_FRAME",address:node,message:format!("role={role} len0={} len104={} elapsed208={} next240=0x{:X}; raw static node stride=0x248; payload interpretation UNKNOWN",u32_at(&b,0).unwrap_or(0),u32_at(&b,0x104).unwrap_or(0),f32::from_bits(u32_at(&b,0x208).unwrap_or(0)),pointer(&b,0x240)),bytes:b});}
     }}
    }else{rows.push(Row{prefix:"REPLAY_RECORDER",address:recorder,message:format!("player=0x{address:X} recorder=0x{recorder:X} absent_or_unreadable; NO buffer dereference"),bytes:Vec::new()});}
   }else{rows.push(Row{prefix:"NATIVE_REPLAY",address:0,message:"PLAYER_UNAVAILABLE".into(),bytes:Vec::new()});}
   // RPM snapshot slots, not SDK characters() which yields mutable references from &self.
   let set=&world.ghost_chr_set;let capacity=set.capacity as usize;let count=capacity.min(MAX_GHOST_SLOTS);let entries=set.entries.as_ptr()as usize;let stride=size_of::<ChrSetEntry<ChrIns>>();let mut active=0;
   for i in 0..count{let Some(slot)=read(entries.saturating_add(i*stride),stride)else{continue;};let addr=pointer(&slot,0);if addr!=0&&slot.get(8)==Some(&2){active+=1;self.actor(&mut rows,addr,"GHOST_SET",set.index,i);}}
   rows.push(Row{prefix:"BLOODSTAIN_GHOST",address:entries,message:format!("ghost_set_index={} capacity={capacity} inspected_slots={count} active_slots={active} truncated={}; active slots do not prove bloodstain type",set.index,count<capacity),bytes:Vec::new()});
   for e in world.chr_inses_by_distance.iter().take(1024){let addr=e.chr_ins.as_ptr()as usize;
    if let Some(b)=read(addr+offset_of!(ChrIns,chr_type),4){let kind=u32_at(&b,0);if kind==Some(0){continue;}self.actor(&mut rows,addr,"DISTANCE_SAMPLE",-1,0);break;}
   }
  }else{rows.push(Row{prefix:"NATIVE_REPLAY",address:0,message:"WORLD_UNAVAILABLE".into(),bytes:Vec::new()});}
  if tx.try_send(Batch{timestamp:now,rows,dropped:self.dropped}).is_err(){self.dropped+=1;}
 }
 fn actor(&self,rows:&mut Vec<Row>,address:usize,source:&str,set:i32,slot:usize){
  let Some(b)=read(address,size_of::<ChrIns>())else{return;};
  let ctrl=pointer(&b,offset_of!(ChrIns,chr_ctrl));let modules=pointer(&b,offset_of!(ChrIns,modules));
  let c=read(ctrl,32);let manip=c.as_ref().map(|v|pointer(v,offset_of!(ChrCtrl,manipulator))).unwrap_or(0);
  let class=class_name(address,self.base,self.image_size);let manip_class=class_name(manip,self.base,self.image_size);let kind=manipulator_kind(manip,self.base,self.image_size);
  let r=|o|u32_at(&b,o).unwrap_or(0);let flags=b.get(offset_of!(ChrIns,net_chr_sync_flags)).copied().unwrap_or(0);
  let m=read(modules,size_of::<ChrInsModuleContainer>());let physics=m.as_ref().map(|v|pointer(v,offset_of!(ChrInsModuleContainer,physics))).unwrap_or(0);let behavior=m.as_ref().map(|v|pointer(v,offset_of!(ChrInsModuleContainer,behavior))).unwrap_or(0);
  let p=read(physics+offset_of!(CSChrPhysicsModule,position),12);let q=read(physics+offset_of!(CSChrPhysicsModule,orientation),16);
  let float_text=|v:Option<Vec<u8>>|v.map(|v|format!("{:?}",v.chunks_exact(4).map(|f|f32::from_le_bytes(f.try_into().unwrap())).collect::<Vec<_>>())).unwrap_or_else(||"UNREADABLE".into());
  rows.push(Row{prefix:"ACTOR_CONTROL",address,message:format!("source={source} set={set} slot={slot} chr=0x{address:X} class={class} chr_type={} ctrl=0x{ctrl:X} ctrl_owner_matches={} manipulator=0x{manip:X} manip_class={manip_class} type_literal={kind:?} flags=0x{flags:X} recorder_enabled={} authority={} net_position_sync={:?} block={} origin={} override={} physics=0x{physics:X} behavior=0x{behavior:X} pos={} quat={}; no virtual call",r(offset_of!(ChrIns,chr_type)),c.as_ref().is_some_and(|v|pointer(v,offset_of!(ChrCtrl,owner))==address),flags&8!=0,r(offset_of!(ChrIns,network_authority)),b.get(offset_of!(ChrIns,net_position_synchronized)),r(offset_of!(ChrIns,block_id))as i32,r(offset_of!(ChrIns,block_origin))as i32,r(offset_of!(ChrIns,block_origin_override))as i32,float_text(p),float_text(q)),bytes:b});
  if let Some(m)=read(manip,if kind==Some(3)&&manip_class==".?AVReplayManipulator@CS@@"{0x150}else{0xc0}){
   let data=if m.len()>=0x150{pointer(&m,0x100)}else{0};
   let owned=pointer(&m,0xa0)==address;
   rows.push(Row{prefix:"REPLAY_MANIPULATOR",address:manip,message:format!("class={manip_class} type_literal={kind:?} ownerA0=0x{:X} owner_matches={owned} data100=0x{data:X}; no engine call",pointer(&m,0xa0)),bytes:m});
   if owned&&data!=0{if let Some(bytes)=read(data,0x240){rows.push(Row{prefix:"REPLAY_FRAME",address:data,message:"kind=attached_data_prefix size=0x240; consumer at RVA3df010 references +220/+230; NOT a decoded frame; no followed buffer pointers".into(),bytes});}}
  }
  if class==".?AVReplayGhostIns@CS@@"{if let Some(bytes)=read(address,0x760){rows.push(Row{prefix:"BLOODSTAIN_GHOST",address,message:format!("class=ReplayGhostIns data740=0x{:X} full_760_snapshot=true; ChrType10 association requires actual observation",pointer(&bytes,0x740)),bytes});}}
 }
}
#[cfg(test)]mod tests{use super::*;
 #[test]fn known_sdk_prefix(){assert_eq!(size_of::<ReplayRecorder>(),0x70);assert_eq!(offset_of!(ReplayRecorder,owning_player),0x10);assert_eq!(offset_of!(ReplayRecorder,frame_counter),0x44);assert_eq!(offset_of!(PlayerIns,replay_recorder),0x5c8);assert_eq!(offset_of!(ChrCtrl,manipulator),0x18);}
 #[test]fn scalar_kind_without_virtual_call(){assert_eq!(literal_kind(&[0xb8,3,0,0,0,0xc3]),Some(3));assert_eq!(literal_kind(&[0xb8,1,0,0,0,0xc3]),Some(1));assert_eq!(literal_kind(&[0x48,0,0,0,0,0xc3]),None);assert_eq!(literal_kind(&[0xb8,9,0,0,0,0xc3]),None);}
 #[test]fn bounded_decode_and_readonly_command_policy(){assert_eq!(u64_at(&[0;7],0),None);assert_eq!(u32_at(&[0;4],usize::MAX),None);for k in [3,5,6,7,11,14,15]{assert!(write_command(k));}for k in [1,2,4,8,9,10,12,13]{assert!(!write_command(k));}}
}

"""Independent bounded native replay payload reader; no game calls or writes.
Exact-build evidence: native_payload JSON disassembly; opaque state IDs retain numeric names.
"""
import argparse,json,math,struct
from pathlib import Path
class DecodeError(ValueError):pass
class Cursor:
 def __init__(self,data):self.data=data;self.pos=0
 def take(self,n):
  if n<0 or self.pos+n>len(self.data):raise DecodeError(f'truncated at {self.pos}: need {n}, remaining {len(self.data)-self.pos}')
  b=self.data[self.pos:self.pos+n];self.pos+=n;return b
 def u8(self):return self.take(1)[0]
 def mask(self,count):
  m=self.u8()
  if m>>count:raise DecodeError(f'unknown presence bits 0x{m:x} for {count} groups')
  return m
 def array(self,width):
  count=self.u8();raw=self.take(count*width)
  return {'count':count,'element_bytes':width,'elements_hex':[raw[i:i+width].hex()for i in range(0,len(raw),width)]}
def signed(n,bits):return (n&((1<<bits)-1))-(1<<bits)if n&(1<<(bits-1))else n&((1<<bits)-1)
def packed_position(a,b):
 return [signed(a,20)*.02,signed(((a>>20)<<5)|(b&31),17)*.04,signed(b>>5,20)*.02]
def finite(v):
 if not math.isfinite(v):raise DecodeError('nonfinite decoded float')
 return v
def behavior(c):
 mask=c.mask(4);groups={}
 for i,width in enumerate([3,4,6,6]):
  if mask&(1<<i):groups[str(i)]=c.array(width)
 return {'mask':mask,'groups':groups,'semantics':'numeric entries only; behavior meaning unresolved'}
def decode_payload(data):
 if not 1<=len(data)<=256:raise DecodeError('native payload length must be 1..256')
 c=Cursor(data);mask=c.mask(6);groups={}
 for i,width in enumerate([20,16,28,16,0,24]):
  if not mask&(1<<i):continue
  start=c.pos
  if i==4:g=behavior(c)
  else:
   raw=c.take(width);g={'raw_hex':raw.hex()}
   if i==0:
    block,x,y,z,component_low,component_high=struct.unpack('<i3fHH',raw);g.update(block_id=block,position_msb=[finite(v)for v in [x,y,z]],orientation_components_raw=[component_low,component_high])
   if i==2:
    flags,duration,block,a,b,extra,last=struct.unpack('<IfiIIIi',raw)
    g.update(flags_raw=flags,duration_seconds=finite(duration),block_id=block,position_msb_quantized=packed_position(a,b),control_yaw_radians=((b>>25)-64)*math.pi/64,unknown3c=extra,unknown40=last)
  g.update(payload_offset=start,encoded_bytes=c.pos-start);groups[str(i)]=g
 state_bytes=c.pos;events_mask=c.mask(4);events={}
 for i in range(4):
  if not events_mask&(1<<i):continue
  if i in (0,1):events[str(i)]=c.array(4 if i==0 else 2)
  elif i==2:events[str(i)]=behavior(c)
  else:events[str(i)]={'raw_u32':struct.unpack('<I',c.take(4))[0]}
 if c.pos!=len(data):raise DecodeError(f'{len(data)-c.pos} unexpected trailing bytes')
 return {'state_mask':mask,'state_bytes':state_bytes,'groups':groups,'events_mask':events_mask,'events':events,'consumed_bytes':c.pos,'semantics':'transform/time fields decoded; numeric behavior/event/action fields not mapped to gameplay'}
def decode_node(raw):
 if len(raw)!=0x248:raise DecodeError('node size must be 0x248')
 result={}
 for name,offset in [('primary',0),('secondary',0x104)]:
  length=struct.unpack_from('<I',raw,offset)[0]
  if length>256:raise DecodeError('payload length exceeds 256')
  result[name]=decode_payload(raw[offset+4:offset+4+length])if length else None
 result['node_duration208']=finite(struct.unpack_from('<f',raw,0x208)[0]);result['node_position220']=[finite(v) for v in struct.unpack_from('<3f',raw,0x220)];result['node_yaw22c']=finite(struct.unpack_from('<f',raw,0x22c)[0]);result['node_block20c']=struct.unpack_from('<i',raw,0x20c)[0]
 primary=result['primary'];g=primary['groups'].get('2')if primary else None
 if g:result['correlation']={'quantized_minus_node_position':[g['position_msb_quantized'][i]-result['node_position220'][i]for i in range(3)],'wrapped_yaw_error':math.remainder(g['control_yaw_radians']-result['node_yaw22c'],math.tau),'duration_minus_node208':g['duration_seconds']-result['node_duration208'],'block_matches':g['block_id']==result['node_block20c']}
 return result
def analyze_journal(path):
 samples=[];errors=[]
 for n,line in enumerate(Path(path).read_text(encoding='utf-8').splitlines(),1):
  try:
   r=json.loads(line)
   if r['prefix']!='REPLAY_FRAME' or not r['message'].startswith('role='):continue
   result=decode_node(bytes.fromhex(r['raw_hex']));samples.append({'time_ns':r['time_ns'],'address':r['address'],'role':r['message'].split()[0],**result})
  except(ValueError,KeyError,struct.error)as e:errors.append({'line':n,'error':str(e)})
 return {'status':'OFFLINE_NATIVE_BYTES_DECODED_NOT_PLAYBACK_PROOF','samples':samples,'errors':errors}
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('journal',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=analyze_journal(a.journal);a.output.write_text(json.dumps(r,indent=2,allow_nan=False),encoding='utf-8');print(f"Decoded {len(r['samples'])} snapshots; errors {len(r['errors'])}")

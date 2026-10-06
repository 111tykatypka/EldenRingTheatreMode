"""Read-only ERPLAY2/3 inspector, including raw fidelity track schema 1.
No game access. Payload CRC is checked before decoding. Exports are data, not replay writes.
"""
import argparse,json,struct,zlib,math
from pathlib import Path
DEFAULT_SCHEMA=Path(__file__).resolve().parents[2]/'shared/capture_schema.json'
if not DEFAULT_SCHEMA.exists():DEFAULT_SCHEMA=Path(__file__).with_name('capture_schema.json')
def exact(f,n):
 b=f.read(n)
 if len(b)!=n:raise ValueError('Truncated replay')
 return b
def unpack(f,fmt):return struct.unpack('<'+fmt,exact(f,struct.calcsize('<'+fmt)))
def string(f):
 n,=unpack(f,'I')
 if n>16*1024*1024:raise ValueError('Metadata field too large')
 return exact(f,n).decode('utf-8')
def decode_record(data,t,schema):
 time,source,seq,drops=struct.unpack_from('<QQQQ',data)
 masks=struct.unpack_from('<'+'I'*t['mask_words'],data,32)
 values=struct.unpack_from('<'+'I'*t['words'],data,32+4*t['mask_words'])
 if t['words']%32 and masks[-1]>>(t['words']%32):raise ValueError('Invalid availability mask')
 fields={}
 for field in t['fields']:
  start=field['offset']-t['begin'];n=field['words'];available=all(masks[(start+j)//32]&(1<<((start+j)%32)) for j in range(n))
  raw=values[start:start+n];value=None
  if available:
   if field['kind']=='f32':value=[struct.unpack('<f',struct.pack('<I',v))[0] for v in raw];value=[x if math.isfinite(x) else str(x) for x in value]
   elif field['kind']=='i16':value=[(v&65535) if (v&65535)<32768 else (v&65535)-65536 for v in raw]
   elif field['kind']=='i32':value=[v if v<2**31 else v-2**32 for v in raw]
   elif field['kind']=='u64':value=raw[0]|(raw[1]<<32)
   elif field['kind']=='bytes':value=b''.join(struct.pack('<I',v)for v in raw).hex()
   else:value=list(raw)
  fields[field['name']]={'available':available,'raw_hex':[f'{v:08X}'for v in raw],'value':value,'confidence':field['confidence'],'semantics':'SDK_REFERENCE_NOT_RUNTIME_DECODED'}
 return {'track':t['name'],'replay_timestamp_ns':time,'source_timestamp_ns':source,'sequence':seq,'source_drops':drops,'fields':fields}
def inspect(path,schema_path=DEFAULT_SCHEMA,time=None,export=None):
 schema=json.loads(Path(schema_path).read_text(encoding='utf-8-sig'));tracks={t['id']:t for t in schema['tracks']}
 report={'file':str(path),'tracks':{},'selected':{},'source_drops_max':0};previous={};total=0;duration=0;chunks=0;action_count=0;last_transform=None
 with Path(path).open('rb') as f:
  magic=exact(f,8);version,flags,start,hz,actual,samples,duration_header,paused=unpack(f,'IIQddQQQ')
  if version not in (2,3) or magic!=f'ERPLAY0{version}'.encode():raise ValueError('Unsupported magic/version')
  report['metadata']={'format_version':version,'date_unix_ns':start,'requested_hz':hz,'actual_hz':actual,'samples':samples,'duration_ns':duration_header,'paused_ns':paused}
  for key in ('game_version','mod_version','title','description','tags'):report['metadata'][key]=string(f)
  while True:
   marker,=unpack(f,'I')
   if marker==0x544f4f46:
    footer=unpack(f,'QQQQ')
    if footer!=(chunks,total,duration,paused) or total!=samples or duration!=duration_header:raise ValueError('Header/footer summary mismatch')
    if version==3 and unpack(f,'Q')[0]!=action_count:raise ValueError('Action footer mismatch')
    if f.read(1):raise ValueError('Trailing data')
    for row in previous.values():
     if row['replay_timestamp_ns']>duration:raise ValueError('Capture beyond duration')
    break
   if marker==0x4b4e4843:
    count,size,crc=unpack(f,'IQI');track=0
    if not count or size!=count*52:raise ValueError('Bad transform chunk size')
   elif marker==0x4b415254 and version==3:
    track,flags,count,size,crc=unpack(f,'IIIQI')
    if not count or flags>1:raise ValueError('Bad track header')
    if track not in tracks and track not in (2,3,4) and flags&1:raise ValueError('Unknown required track')
    if track in tracks and size!=count*tracks[track]['record_bytes']:raise ValueError('Schema/chunk length mismatch')
   else:raise ValueError('Bad chunk marker')
   if size>64*1024*1024:raise ValueError('Oversized payload')
   payload=exact(f,size)
   if zlib.crc32(payload)!=crc:raise ValueError('CRC mismatch')
   if track==0:
    for off in range(0,size,52):
     index,replay_ns,source_ns=struct.unpack_from('<QQQ',payload,off)
     if index!=total or (last_transform and (replay_ns<last_transform[0]or source_ns<last_transform[1])):raise ValueError('Transform ordering')
     last_transform=(replay_ns,source_ns);duration=replay_ns;total+=1
    chunks+=1
   elif track==2:action_count+=count
   elif track in tracks:
    t=tracks[track];stats=report['tracks'].setdefault(t['name'],{'records':0,'changed_records':0,'last_available_fields':0,'nonfinite_fields':0})
    for off in range(0,size,t['record_bytes']):
     row=decode_record(payload[off:off+t['record_bytes']],t,schema);old=previous.get(track)
     if old and (row['sequence']<=old['sequence'] or row['source_timestamp_ns']<old['source_timestamp_ns'] or row['replay_timestamp_ns']<old['replay_timestamp_ns'] or row['source_drops']<old['source_drops']):raise ValueError('Capture ordering regression')
     stats['records']+=1
     stats['last_available_fields']=sum(x['available']for x in row['fields'].values())
     if old and any(row['fields'][key]['raw_hex']!=value['raw_hex'] or row['fields'][key]['available']!=value['available'] for key,value in old['fields'].items()):stats['changed_records']+=1
     stats['nonfinite_fields']+=sum(isinstance(x['value'],list) and any(isinstance(v,str)for v in x['value'])for x in row['fields'].values())
     previous[track]=row;report['source_drops_max']=max(report['source_drops_max'],row['source_drops'])
     if time is not None and row['replay_timestamp_ns']<=time:report['selected'][t['name']]=row
     if export:export.write(json.dumps(row,ensure_ascii=False,allow_nan=False)+'\n')
 report['file_bytes']=Path(path).stat().st_size
 report['bytes_per_second']=report['file_bytes']/(duration/1e9) if duration else None
 return report
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('replay',type=Path);p.add_argument('--schema',type=Path,default=DEFAULT_SCHEMA);p.add_argument('--time',type=float);p.add_argument('--export-jsonl',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
 out=a.export_jsonl.open('w',encoding='utf-8')if a.export_jsonl else None
 try:result=inspect(a.replay,a.schema,None if a.time is None else int(a.time*1e9),out)
 finally:
  if out:out.close()
 text=json.dumps(result,indent=2,ensure_ascii=False,allow_nan=False)
 if a.output:a.output.write_text(text+'\n',encoding='utf-8')
 else:print(text)

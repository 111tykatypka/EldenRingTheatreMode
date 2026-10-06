"""Read-only analysis of native-bloodstain JSONL snapshots. Does not prove semantics."""
import argparse,json,struct,hashlib,math
from pathlib import Path

def analyze(path):
    rows=[];previous={};changes={};errors=[];nodes=[];actors=[];drops=0
    with Path(path).open(encoding='utf-8') as f:
        for line_num,line in enumerate(f,1):
            try:
                row=json.loads(line);raw=bytes.fromhex(row['raw_hex']);t=int(row['time_ns'])
                if row['schema']!=1:raise ValueError('unknown journal schema')
                key=(row['prefix'],row['address']);old=previous.get(key)
                if old and t<old[0]:raise ValueError('timestamp regression')
                if old:
                    for offset in range(0,min(len(raw),len(old[1])),4):
                        if raw[offset:offset+4]!=old[1][offset:offset+4]:changes[(key,offset)]=changes.get((key,offset),0)+1
                previous[key]=(t,raw);drops=max(drops,int(row.get('source_drops',0)))
                if row['prefix']=='REPLAY_FRAME' and raw and row['message'].startswith('role='):
                    if len(raw)!=0x248:raise ValueError('native node snapshot length')
                    n0,n1=struct.unpack_from('<I',raw,0)[0],struct.unpack_from('<I',raw,0x104)[0]
                    nodes.append({'time_ns':t,'address':row['address'],'primary_bytes':n0,'secondary_bytes':n1,'lengths_within_static_capacity':n0<=256 and n1<=256,'elapsed208_as_f32':(lambda v:v if math.isfinite(v)else None)(struct.unpack_from('<f',raw,0x208)[0]),'payload_sha256':hashlib.sha256(raw[4:4+min(n0,256)]).hexdigest()})
                if row['prefix']=='ACTOR_CONTROL':actors.append(row['message'])
                rows.append(row)
            except (ValueError,KeyError,struct.error,TypeError) as e:errors.append({'line':line_num,'error':str(e)})
    return {'classification':'OBSERVATION_NOT_RUNTIME_PLAYBACK_PROOF','rows':len(rows),'errors':errors,'queue_drops':drops,'changed_words':[{'prefix':k[0][0],'address':k[0][1],'offset':hex(k[1]),'changes':n}for k,n in sorted(changes.items())],'nodes':nodes,'actor_controls':list(dict.fromkeys(actors))}
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('journal',type=Path);p.add_argument('--output',type=Path);a=p.parse_args();r=analyze(a.journal);s=json.dumps(r,ensure_ascii=False,indent=2,allow_nan=False)
    if a.output:a.output.write_text(s+'\n',encoding='utf-8')
    else:print(s)

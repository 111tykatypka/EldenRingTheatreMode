"""Read-only callback observations, labeled node diffs and bounded payload decoding.
Rates are observation rates, never claimed to be native hook invocation rates.
"""
import argparse,json,re,statistics
from pathlib import Path
from payload_codec import decode_node

def analyze(path):
 ticks=[];nodes=[];contexts={};errors=[];drops=0;previous=None
 for line_number,line in enumerate(Path(path).read_text(encoding='utf-8').splitlines(),1):
  try:
   r=json.loads(line);drops=max(drops,r.get('source_drops',0))
   if r['prefix']=='PAYLOAD_TICK':ticks.append(r['time_ns'])
   if r['prefix']=='PAYLOAD_CONTEXT':contexts[r['time_ns']]=r['message']
   if r['prefix']!='REPLAY_FRAME' or not r['message'].startswith('role=observed_write'):continue
   raw=bytes.fromhex(r['raw_hex']);decoded=decode_node(raw)
   node={'time_ns':r['time_ns'],'message':r['message'],'decoded':decoded,
         'changed_offsets':[hex(i) for i in range(len(raw)) if previous is not None and raw[i]!=previous[i]]}
   previous=raw;nodes.append(node)
  except (ValueError,KeyError) as e:errors.append({'line':line_number,'error':str(e)})
 intervals=[(b-a)/1e9 for a,b in zip(ticks,ticks[1:]) if b>a]
 groups={}
 for n in nodes:
  n['live_context']=contexts.get(n['time_ns'])
  label=re.search(r'user_marker=(\w+)',n['message']);key=label.group(1) if label else 'UNKNOWN'
  groups.setdefault(key,[]).append(n['time_ns'])
 return {'status':'OBSERVATIONS_ONLY_ACTION_SEMANTICS_UNVERIFIED','callbacks':len(ticks),
  'callback_interval_median_seconds':statistics.median(intervals) if intervals else None,
  'source_drops_max':drops,'observed_nodes_including_initial':len(nodes),
  'marker_summary':{k:{'observations':len(v),'span_seconds':(v[-1]-v[0])/1e9,
   'observed_nodes_per_second':(len(v)-1)*1e9/(v[-1]-v[0]) if len(v)>1 and v[-1]>v[0] else None} for k,v in groups.items()},
  'nodes':nodes,'errors':errors,
  'limitations':'PostPhysics can miss multiple native writes between callbacks. User markers are annotations, not verified animation classifications. Sparse fields require native state inheritance; this utility does not implement playback.'}

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('journal',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 r=analyze(a.journal);a.output.write_text(json.dumps(r,indent=2,allow_nan=False),encoding='utf-8');print(f"callbacks={r['callbacks']} nodes={r['observed_nodes_including_initial']} drops={r['source_drops_max']} errors={len(r['errors'])}")

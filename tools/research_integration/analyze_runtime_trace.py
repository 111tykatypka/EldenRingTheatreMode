"""Stream the bounded native JSONL trace. Statistics are observations, not visual verification."""
import argparse, collections, json, math
from pathlib import Path

def vector(value, count=3):
    return isinstance(value,list) and len(value)==count and all(isinstance(v,(int,float)) and math.isfinite(v) for v in value)

def analyze(lines):
    stages=collections.Counter(); failures=collections.Counter(); owners={}; malformed=0; invalid=0; pairs=0
    immediate_max=0.; next_max=0.; next_count=0; model_offset_max=0.; drops=0; rates={}
    for line in lines:
        try:r=json.loads(line)
        except (ValueError,TypeError):malformed+=1;continue
        if not isinstance(r,dict) or r.get('schema')!=1:malformed+=1;continue
        stage=r.get('stage','UNKNOWN');stages[stage]+=1
        if any(t in stage for t in ('rejected','failed','mismatch','invalid')):failures[stage]+=1
        drops=max(drops,r.get('queue_drops',0));key=(r.get('run'),r.get('handle'))
        if not r.get('has_transform'):continue  # IPC events are not zero-valued native samples.
        p=r.get('position');q=r.get('orientation');time=r.get('time_ns')
        if not vector(p) or not vector(q,4) or not isinstance(time,int):invalid+=1;continue
        if abs(sum(v*v for v in q)-1.)>.01:invalid+=1
        model=r.get('model_position');physics=r.get('physics_model_position');offset=r.get('vertical_offset')
        if vector(model) and vector(physics) and isinstance(offset,(int,float)):
            model_offset_max=max(model_offset_max,abs(model[1]-physics[1]-offset))
        if stage=='player_post_physics_before':
            bucket=rates.setdefault(r.get('run'),[time,time,0]);bucket[1]=time;bucket[2]+=1
        old=owners.get(key)
        if stage in ('player_post_physics_before','actor_write_before') and old and time>old[0]:
            next_max=max(next_max,math.dist(p,old[1]));next_count+=1;owners.pop(key,None)
        target=r.get('target')
        if stage in ('player_write_after','actor_write_after') and isinstance(target,dict) and vector(target.get('position')):
            error=math.dist(p,target['position']);immediate_max=max(immediate_max,error);pairs+=1;owners[key]=(time,target['position'])
    return {'schema':1,'stage_counts':dict(stages),'failure_counts':dict(failures),'malformed_rows':malformed,
            'invalid_native_samples':invalid,'queue_drops_observed':drops,'immediate_readback_pairs':pairs,
            'max_immediate_position_error':immediate_max if pairs else None,'next_callback_pairs':next_count,
            'max_next_callback_position_error':next_max if next_count else None,'max_model_y_minus_physics_y_minus_vertical_offset':model_offset_max,
            'post_physics_hz_by_run':{str(k):(v[2]-1)*1e9/(v[1]-v[0]) if v[1]>v[0] else None for k,v in rates.items()},
            'limitations':['No collision/proxy ground-height binding.','Next-callback deviation includes legitimate live simulation.','No character-model visual confirmation.','Stage counts are not packet-delivery acknowledgements.']}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('trace',type=Path);ap.add_argument('--output',type=Path);args=ap.parse_args()
    with args.trace.open(encoding='utf-8-sig') as f:result=analyze(f)
    text=json.dumps(result,ensure_ascii=False,indent=2)+'\n'
    if args.output:args.output.write_text(text,encoding='utf-8')
    else:print(text,end='')
if __name__=='__main__':main()

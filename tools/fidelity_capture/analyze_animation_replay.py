"""Read-only native replay request/observation audit; no game writes."""
import argparse,json,re
from pathlib import Path

def analyze(text,session=None):
    groups={}
    for line in text.splitlines():
        if "REPLAY_ANIMATION_COMPARE " not in line and "REPLAY_ANIMATION_REQUEST " not in line:continue
        sid=re.search(r"session=(\d+)",line)
        if not sid:continue
        sid=int(sid.group(1))
        if session is not None and sid!=session:continue
        group=groups.setdefault(sid,{"requests":0,"comparisons":0,"id_matches":0,"phase_errors_s":[],"mismatches":[]})
        def number(key):
            value=re.search(r"\b"+key+r"=(-?\d+)",line)
            return int(value.group(1)) if value else None
        if "REPLAY_ANIMATION_REQUEST " in line:group["requests"]+=1
        else:
            group["comparisons"]+=1
            if "id_match=true" in line:group["id_matches"]+=1
            else:group["mismatches"].append({"replay_ns":number("replay_ns"),"requested_id":number("requested_id"),"observed_id":number("observed_id")})
            phase=re.search(r"phase_error=Some\(([^)]+)\)",line)
            if phase and "id_match=true" in line:group["phase_errors_s"].append(float(phase.group(1)))
    for group in groups.values():
        errors=group.pop("phase_errors_s")
        group["max_abs_phase_error_s"]=max(map(abs,errors),default=None)
        group["mean_abs_phase_error_s"]=sum(map(abs,errors))/len(errors) if errors else None
        group["id_match_fraction"]=group["id_matches"]/group["comparisons"] if group["comparisons"] else None
    return {"sessions":groups,"limitation":"Sampled native state comparisons, not visual pose verification or proof of exact action replay."}
if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("log",type=Path);parser.add_argument("--session",type=int);args=parser.parse_args()
    print(json.dumps(analyze(args.log.read_text(encoding="utf-8-sig",errors="replace"),args.session),indent=2))

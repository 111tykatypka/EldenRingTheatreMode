"""Produce evidence metadata; never execute CE scripts, alter references or load the game."""
import argparse, hashlib, importlib.util, json, re, sqlite3, struct, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RESEARCH = ROOT.parent / 'research'
GIT = Path('C:/Program Files/Git/cmd/git.exe')

def git(path, *args):
    return subprocess.check_output([str(GIT), '-c', f'safe.directory={path.as_posix()}', '-C', str(path), *args], text=True, encoding='utf-8').strip()

def find_pattern(data, pattern):
    tokens = re.findall(r'\?\?|\?|[0-9a-fA-F]{2}', pattern)
    values = [None if '?' in t else int(t, 16) for t in tokens]
    fixed = next((i for i, v in enumerate(values) if v is not None), None)
    if fixed is None:
        raise ValueError('All-wildcard pattern rejected')
    needle = bytes([values[fixed]])
    hits, cursor = [], fixed
    while True:
        hit = data.find(needle, cursor)
        if hit < 0:
            return hits
        start = hit - fixed
        if start >= 0 and start + len(values) <= len(data) and all(v is None or data[start+i] == v for i, v in enumerate(values)):
            hits.append(start)
        cursor = hit+1

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--output', type=Path, default=ROOT/'research/symbols_2_7_0_0.json')
    args = ap.parse_args()
    spec = importlib.util.spec_from_file_location('query', ROOT/'tools/ghidra_query/research_query.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    image = module.Image(RESEARCH/'ghidra-eldenring/export')  # Exact target disk SHA mandatory.
    sources = []
    for name in ('FreecamMod', 'EldenRingHKS', 'Elden-Ring-CT-TGA'):
        path = RESEARCH/'community-nightly'/name
        sources.append({'name':name, 'commit':git(path,'rev-parse','HEAD'), 'url':git(path,'remote','get-url','origin'), 'path':str(path)})
    anchors = [
      ('FieldArea', '48 8B 3D ? ? ? ? 49 8B D8 48 8B F2 4C 8B F1 48 85 FF', 'FreecamMod/src/core/game_data_manager.h', 'rip7'),
      ('WorldChrMan_Freecam', '48 8B 05 ? ? ? ? 48 85 C0 74 0F 48 39 88', 'FreecamMod/src/core/game_data_manager.h','rip7'),
      ('CSLuaEventManager', '48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 41 BE 01 00 00 00 44 89 75', 'Hexinton-v8.0.4.CT/[ Enable ]','rip7'),
      ('W_Event', '74 ?? 48 85 d2 74 ?? 48 8d 4c 24 50', 'Hexinton-v8.0.4.CT/PlayAnimation','minus13'),
      ('LuaWarp_01', 'C3 ?? ?? ???????? 57 48 83 EC ?? 48 8B FA 44', 'Hexinton-v8.0.4.CT/[ Fast Travel and Warp ]','plus2'),
    ]
    rows=[]
    for name,pattern,source,resolve in anchors:
        hits=[]
        for s in image.meta['sections']:
            if not int(s['characteristics'],16)&0x20000000:continue
            region=image.data[s['file_offset']:s['file_offset']+s['raw_size']]
            for off in find_pattern(region,pattern):
                va=image.base+s['rva']+off
                target=va+7+struct.unpack('<i',image.read(va+3,4))[0] if resolve=='rip7' else va+(-13 if resolve=='minus13' else 2)
                hits.append({'site_va':hex(va),'candidate_va':hex(target),'candidate_rva':hex(target-image.base)})
        rows.append({'name':name,'source':source,'pattern':pattern,'hits':hits,'unique':len(hits)==1,
                     'status':'CROSS_CHECKED' if len(hits)==1 else 'REFERENCE',
                     'runtime_read_verified':False,'runtime_write_verified':False,'abi':'UNKNOWN',
                     'limitation':'Unique bytes do not establish function entry, ABI, owner lifetime or safe call thread.'})
    db_path=RESEARCH/'ghidra-eldenring/export/research.sqlite'
    with sqlite3.connect(db_path.resolve().as_uri()+'?mode=ro',uri=True) as db:
        db.execute('PRAGMA query_only=ON')
        rtti=[{'va':'0x'+a,'name':v} for a,v in db.execute("SELECT address,value FROM strings_fts WHERE value LIKE '%hkbCharacter%' AND value LIKE '.?AV%' LIMIT 20")]
    result={'schema':1,'target':'EldenRing_1_17 / 2.7.0.0','disk_sha256':module.SHA.upper(),
            'sources':sources,'anchors':rows,'hkb_rtti':rtti,
            'typed_sdk_reference':{'revision':'3c8c1d7633a99309fb004c9f894ea10b7967d0e0',
              'ChrDebugFlags':{'noMove':5,'noAttack':4,'noUpdate':8},
              'ChrCtrlChrProxyFlags':{'position_sync_requested':0,'rotation_sync_requested':1},
              'CSChrBehaviorModule.animation_speed':'0x17c8',
              'ChrIns.debug_flags':{'sdk_offset':'0x530','freecam_offset':'0x538','status':'CONFLICT; flag writes BLOCKED'},
              'status':'REFERENCE; bit semantics agree, enclosing member offset CONFLICTS; animationSpeed experiment UNVERIFIED'},
            'world_binding':{'singleton':'CSLuaEventMan','type':'CSLuaEventManImp','source':'SDK cs/lua_event_man.rs; TGA .cea baseAliases',
                             'proxy_offset':'0x08','script_imitation_offset':'0x18','warp_bonfire_id_offset':'0x1c','status':'CROSS_CHECKED; READ-ONLY runtime instrumentation implemented, UNVERIFIED'},
            'unresolved':['W_Event ABI/hkbCharacter chain','warp ownership and request prerequisites','proxy sync consumers','native AI control ownership'],
            'note':'Metadata only. No reference code, game binary, pseudocode or binaries bundled. Partial Ghidra is not ground truth.'}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps([{'name':r['name'],'matches':len(r['hits']),'hits':r['hits']} for r in rows],indent=2))

if __name__ == '__main__':main()

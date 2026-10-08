"""Refresh local research provenance without network access or running references."""
import hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
refs=root/'research/references'
result={'format_version':1,'repositories':[],'files':[]}
for directory in sorted(refs.iterdir()):
    if not (directory/'.git').exists():continue
    command=['git','-c',f'safe.directory={directory.as_posix()}','-C',str(directory)]
    def git(*args):return subprocess.check_output(command+list(args),text=True).strip()
    result['repositories'].append({'name':directory.name,'path':directory.relative_to(root).as_posix(),'origin':git('remote','get-url','origin'),'commit':git('rev-parse','HEAD'),'commit_date':git('log','-1','--format=%aI'),'tracked_files':int(git('ls-files').count('\n')+1)})
for name in ['research/fxr_reference_sheet.csv','native_ui/ParticleReferenceData.inc','research/format_tools_reference_inventory.json','research/misspia_elden_ring_data_inventory.json']:
    path=root/name
    if path.exists():result['files'].append({'path':name,'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
output=root/'research/REFERENCE_MANIFEST.json'
output.write_text(json.dumps(result,indent=2),encoding='utf-8')
print(f'Saved {len(result["repositories"])} repository snapshots and {len(result["files"])} file fingerprints to {output}')

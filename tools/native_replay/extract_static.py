"""Bounded exact-image replay research extraction. Generated game code stays outside Git."""
import argparse,json,sqlite3,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'ghidra_query'))
from research_query import Image,DEFAULT,SHA
FUNCTIONS=('1404e4da0','1404e4ec0','1404e4f70','1404e5070','1404e5310','1404e5530','1404e5af0','1404e6520','1404e6420','1404e65d0','1404e6630','1406f1db0','1406f1e30','140660ac0','140657840','14065db40','1403deaa0','1403df010','1403dfd50','1404f1840','1404f19c0','1404f1ab0','140404570','140403d50','1406f2410','1404e9f90','1404eb680','140423d90','140423d10','1403e6e30','1403dedc0','1403def60','1403dee00','1403deec0','1403def70','140a027a0','140a02810','140a023d0','1403f92b0','1403dfdc0','1403df190','1403df650','1406f1e60','1406f1e90','1404e9490','1404e9840','1404e9940','1403cd380')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--database',type=Path,default=DEFAULT);p.add_argument('--output',type=Path,required=True);p.add_argument('--functions',nargs='+',help='Explicit preferred-VA functions; omit to use bounded known list');a=p.parse_args()
 a.output.mkdir(parents=True,exist_ok=True);db=sqlite3.connect(a.database.resolve().as_uri()+'?mode=ro',uri=True);db.row_factory=sqlite3.Row;db.execute('PRAGMA query_only=ON');image=Image(a.database.parent)
 sys.path.insert(0,str(a.database.parent.parent/'tools/python-deps'))
 import capstone
 decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
 summary=[]
 for key in (tuple(v.lower().removeprefix('0x') for v in a.functions) if a.functions else FUNCTIONS):
  f=db.execute('SELECT * FROM functions WHERE entry=?',(key,)).fetchone();size=min(int(f['body_bytes']),0x3000)if f else 0x180
  if size<=0:size=0x180
  code=[dict(x)for x in db.execute('SELECT * FROM code_fts WHERE entry=?',(key,))]
  disasm=[{'va':hex(i.address),'bytes':i.bytes.hex(),'mnemonic':i.mnemonic,'operands':i.op_str}for i in decoder.disasm(image.read(int(key,16),size),int(key,16))]
  packet={'sha256':SHA,'entry':key,'rva':hex(int(key,16)-image.base),'indexed_function':dict(f)if f else None,'byte_limit':size,'linear_disassembly_not_cfg':True,'pseudocode':code,'callers':[dict(x)for x in db.execute('SELECT * FROM calls WHERE callee=?',(key,))],'callees':[dict(x)for x in db.execute('SELECT * FROM calls WHERE caller=?',(key,))],'xrefs':[dict(x)for x in db.execute('SELECT * FROM refs WHERE target=?',(key,))],'disassembly':disasm}
  (a.output/(key+'.json')).write_text(json.dumps(packet,indent=2),encoding='utf-8');summary.append({'entry':key,'rva':packet['rva'],'pseudocode_available':bool(code),'instructions':len(disasm),'calls':packet['callees']})
 (a.output/'function_index.json').write_text(json.dumps(summary,indent=2),encoding='utf-8');print('Exact SHA matched; extracted',len(summary),'bounded functions. No runtime calls/writes.')
if __name__=='__main__':main()

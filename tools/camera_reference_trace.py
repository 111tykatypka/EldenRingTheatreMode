"""Read-only PE inventory and focused reference-DLL disassembly. No execution/injection."""
from pathlib import Path
import struct, hashlib, json, sys
ROOT=Path(r'C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRing_CameraTools_v1018')
OUT=Path(__file__).resolve().parents[1]/'research'
def pe(data):
 o=struct.unpack_from('<I',data,0x3c)[0];machine,count=struct.unpack_from('<HH',data,o+4);opt=o+24;magic=struct.unpack_from('<H',data,opt)[0]
 size=struct.unpack_from('<H',data,o+20)[0];base=struct.unpack_from('<Q' if magic==0x20b else '<I',data,opt+(24 if magic==0x20b else 28))[0]
 sections=[]
 for n in range(count):
  x=opt+size+n*40;name=data[x:x+8].rstrip(b'\0').decode();vsize,rva,raw,offset=struct.unpack_from('<4I',data,x+8);sections.append((name,rva,raw,offset))
 def offset(rva):
  for _,v,n,o in sections:
   if v<=rva<v+n:return o+rva-v
  raise ValueError('Unmapped RVA')
 def string(rva):
  a=offset(rva);return data[a:data.index(b'\0',a)].decode('ascii','replace')
 directory=opt+(112 if magic==0x20b else 96);imports=[];imp=struct.unpack_from('<I',data,directory+8)[0]
 if imp:
  a=offset(imp)
  while any(data[a:a+20]):
   lookup,_,_,name,first=struct.unpack_from('<5I',data,a);imports.append(string(name));a+=20
 clr=struct.unpack_from('<I',data,directory+14*8)[0]
 return {'machine':hex(machine),'pe_magic':hex(magic),'image_base':hex(base),'managed_clr':bool(clr),'clr_flags':hex(struct.unpack_from('<I',data,offset(clr)+16)[0]) if clr else None,'imports':imports,'sections':[s[0] for s in sections]},offset,base
inventory=[]
for path in sorted(ROOT.rglob('*')):
 if not path.is_file():continue
 data=path.read_bytes();item={'path':str(path.relative_to(ROOT)),'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
 if data[:2]==b'MZ':item['pe']=pe(data)[0]
 inventory.append(item)
(OUT/'CAMERATOOLS_INVENTORY.json').write_text(json.dumps(inventory,indent=2)+'\n')
native=ROOT/'EldenRingCameraTools.dll';data=native.read_bytes()
if hashlib.sha256(data).hexdigest()!='1a1da1fdbeb9f3eb85ef29469ff9ee2a1322108d47f107c11f137d4e13a077eb':raise SystemExit('Reference hash mismatch')
sys.path.insert(0,str(OUT.parents[1]/'research/ghidra-eldenring/tools/python-deps'))
import capstone
_,offset,base=pe(data);decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
traces={}
for rva,size in [(0x6c110,200),(0x21e350,128),(0x218a00,64),(0x2188c0,100),(0x21e260,240),(0x21e570,280),(0x21e680,190),(0x21ed40,220),(0x210f40,400),(0x21ba30,340),(0x21b5b0,720),(0x209b20,90),(0x209b70,90),(0x209d40,90),(0x209d80,520),(0x209fc0,230),(0x20cdc0,130)]:
 a=offset(rva);traces[hex(rva)]=[{'va':hex(i.address),'op':i.mnemonic,'args':i.op_str,'bytes':i.bytes.hex()} for i in decoder.disasm(data[a:a+size],base+rva)]
(OUT/'CAMERATOOLS_NATIVE_TRACE.json').write_text(json.dumps(traces,indent=2)+'\n')
print('Inventory files:',len(inventory),'Native traces:',len(traces))

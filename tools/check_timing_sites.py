import struct,pathlib,json,sys
p=pathlib.Path(sys.argv[1]);d=p.read_bytes();pe=struct.unpack_from('<I',d,60)[0];n=struct.unpack_from('<H',d,pe+6)[0];opt=struct.unpack_from('<H',d,pe+20)[0];sections=[]
for i in range(n):
 s=pe+24+opt+i*40;name=d[s:s+8].rstrip(b'\0').decode();vs,va,sz,raw=struct.unpack_from('<IIII',d,s+8);sections.append((name,va,vs,raw,sz))
for rva in [0xdeb30f,0xdebe2f]:
 for name,va,vs,raw,sz in sections:
  if va<=rva<va+sz:
   b=d[raw+rva-va:raw+rva-va+23];disp=struct.unpack_from('<i',b,3)[0];print(hex(rva),b.hex(' '),'rip_target',hex(rva+7+disp))

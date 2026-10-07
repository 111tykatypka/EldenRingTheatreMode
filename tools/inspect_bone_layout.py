import struct,json,math,pathlib,sys
p=pathlib.Path(sys.argv[1])
d=p.read_bytes();stride=struct.unpack_from('<I',d,16)[0];n=struct.unpack_from('<Q',d,24)[0]
r={'file':str(p),'frames':n,'stride':stride,'arrays':{}}
for label,start in [('local',120),('model',7320)]:
 norms=[];scales=[];bad=0
 for f in range(n):
  for b in range(150):
   v=struct.unpack_from('<12f',d,32+f*stride+start+b*48)
   q=math.sqrt(sum(x*x for x in v[4:8]));norms.append(q);scales.extend(v[8:11]);bad+=int(not all(math.isfinite(x) for x in v[:3]+v[4:11]))
 r['arrays'][label]={'quaternion_norm_min':min(norms),'quaternion_norm_max':max(norms),'scale_min':min(scales),'scale_max':max(scales),'invalid':bad,'first_bone':struct.unpack_from('<12f',d,32+start)}
print(json.dumps(r,indent=2))

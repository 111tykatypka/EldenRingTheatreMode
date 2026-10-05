"""Generate reproducible static SDK anchors, never executable runtime bindings."""
import argparse
import json
from pathlib import Path
import sqlite3
import struct
from research_query import DEFAULT, Image, query

TYPES=('WorldChrMan','PlayerIns','ChrIns','ChrCtrl','ReplayRecorder','NetChrSync',
       'NetChrSetSync','ChrManipulator','ReplayManipulator','PadManipulator','ComManipulator','NetworkManipulator',
       'CSChrPhysicsModule','CSChrBehaviorModule','CSChrActionRequestModule')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--database',type=Path,default=DEFAULT)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    db=sqlite3.connect(a.database.resolve().as_uri()+'?mode=ro',uri=True);db.row_factory=sqlite3.Row
    db.execute('PRAGMA query_only=ON')
    image=Image(a.database.parent)
    anchors=[]
    for name in TYPES:
        strings=query(db,'SELECT address,rva,value FROM strings_fts WHERE value LIKE ? LIMIT 100',('%'+name+'%',))
        for s in strings:
            s['xrefs']=query(db,'SELECT * FROM refs WHERE target=? LIMIT 100',(s['address'],))
        # MSVC x64 COL signature=1, TypeDescriptor is image-relative; name follows 16-byte descriptor.
        # Search initialized read-only data only; structural validation does not establish ABI.
        cols=[]
        for s in strings:
            if not s['value'].startswith(('.?AV'+name+'@','.?AU'+name+'@')):continue
            td=int(s['address'],16)-16
            needle=struct.pack('<I',td-image.base)
            for section in image.meta['sections']:
                if int(section['characteristics'],16)&0x20000000 or section['name']!='.rdata':continue
                start=section['file_offset'];end=start+section['raw_size'];pos=start
                while True:
                    pos=image.data.find(needle,pos,end)
                    if pos<0:break
                    col=image.base+section['rva']+(pos-start)-12;pos+=1
                    try:
                        sig,offset,cd,td_rva,ch_rva,self_rva=struct.unpack('<6I',image.read(col,24))
                        if sig!=1 or self_rva!=col-image.base or td_rva!=td-image.base:continue
                        hierarchy=image.read(image.base+ch_rva,16)
                        col_refs=[]
                        packed=struct.pack('<Q',col);pointer_pos=start
                        while True:
                            pointer_pos=image.data.find(packed,pointer_pos,end)
                            if pointer_pos<0:break
                            table=image.base+section['rva']+pointer_pos-start+8;pointer_pos+=1
                            slots=[]
                            for i in range(8):
                                target=struct.unpack('<Q',image.read(table+i*8,8))[0]
                                if not image.executable(target):break
                                slots.append({'index':i,'target':hex(target),'function':query(db,'SELECT name,status FROM functions WHERE entry=?',(format(target,'x'),))})
                            if slots:col_refs.append({'vtable_candidate':hex(table),'first_slots':slots})
                        cols.append({'classification':'HIGH_CONFIDENCE_MSVC_RTTI_STRUCTURE_NOT_RUNTIME_ABI','address':hex(col),
                                     'object_offset':offset,'constructor_displacement':cd,'type_descriptor':hex(td),
                                     'hierarchy':hex(image.base+ch_rva),'hierarchy_bytes':hierarchy.hex(' '),'vtables':col_refs})
                    except (ValueError,struct.error):continue
        anchors.append({'sdk_type':name,'strings':strings,'col_candidates':cols})
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps({'target_sha256':image.meta['input_sha256'],
        'pinned_sdk':'3c8c1d7633a99309fb004c9f894ea10b7967d0e0',
        'status':'STATIC_CANDIDATES_RUNTIME_VALIDATION_REQUIRED','types':anchors},indent=2)+'\n')
    print(json.dumps({r['sdk_type']:{'strings':len(r['strings']),'COL':len(r['col_candidates']),
        'vtable_candidates':sum(len(c['vtables']) for c in r['col_candidates'])} for r in anchors},indent=2))
    db.close()

if __name__=='__main__':main()

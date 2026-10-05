"""Read-only queries against the local, partial Elden Ring Ghidra export."""
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import struct
import sys

DEFAULT = Path(__file__).resolve().parents[3] / 'research/ghidra-eldenring/export/research.sqlite'
SHA = 'd1a84083c6c7c7902162ff098f7d86812839aa6b3575959398857e539c488134'

def address(value):
    return format(int(value, 16), 'x')

def quoted(value):
    return '"' + value.replace('"', '""') + '"'

def query(db, sql, args=()):
    return [dict(r) for r in db.execute(sql, args)]

class Image:
    def __init__(self, export):
        self.meta = json.loads((export / 'pe_metadata.json').read_text())
        self.data = (export.parent / 'input/eldenring.exe').read_bytes()
        if hashlib.sha256(self.data).hexdigest() != SHA:
            raise ValueError('Analysis copy SHA-256 mismatch')
        self.base = int(self.meta['image_base'], 16)

    def offset(self, va, size):
        rva = va - self.base
        for s in self.meta['sections']:
            delta = rva - s['rva']
            if 0 <= delta and delta + size <= s['raw_size']:
                return s['file_offset'] + delta
        raise ValueError(f'Not backed by section bytes: {va:#x}, length {size}')

    def read(self, va, size):
        off = self.offset(va, size)
        return self.data[off:off + size]

    def executable(self, va):
        return any(int(s['characteristics'], 16) & 0x20000000 and
                   s['rva'] <= va-self.base < s['rva']+s['virtual_size']
                   for s in self.meta['sections'])

def run(args, db):
    n = args.limit
    a = args.value
    if args.command == 'schema':
        return query(db, "SELECT name,sql FROM sqlite_master WHERE sql IS NOT NULL ORDER BY name")
    if args.command in ('text', 'strings', 'rtti', 'references'):
        hits = query(db, 'SELECT address,rva,encoding,value FROM strings_fts WHERE strings_fts MATCH ? LIMIT ?', (quoted(a), n))
        if args.command == 'rtti':
            hits = query(db, "SELECT address,rva,encoding,value FROM strings_fts WHERE value LIKE ? AND (value LIKE '.?AV%' OR value LIKE '.?AU%') LIMIT ?", ('%'+a+'%', n))
        if args.command == 'references':
            for hit in hits:
                hit['xrefs'] = query(db, 'SELECT * FROM refs WHERE target=? LIMIT ?', (hit['address'], n))
        if args.command == 'text':
            return {'strings': hits, 'symbols': query(db, 'SELECT * FROM symbols WHERE name LIKE ? LIMIT ?', ('%'+a+'%', n)),
                    'code': query(db, 'SELECT entry,c_file,snippet(code_fts,2,\'[\',\']\',\'...\',32) AS excerpt FROM code_fts WHERE code_fts MATCH ? LIMIT ?', (quoted(a), n))}
        return hits
    if args.command in ('code', 'functions-containing', 'offset'):
        term = a if args.command != 'offset' else hex(int(a, 16))
        # Textual offset occurrences are candidates, never proven member accesses.
        return query(db, 'SELECT entry,c_file,snippet(code_fts,2,\'[\',\']\',\'...\',40) AS excerpt FROM code_fts WHERE code_fts MATCH ? LIMIT ?', (quoted(term), n))
    if args.command == 'name':
        return query(db, 'SELECT * FROM functions WHERE name LIKE ? LIMIT ?', ('%'+a+'%', n))
    key = address(a)
    if args.command == 'function':
        return {'function': query(db, 'SELECT * FROM functions WHERE entry=?', (key,)),
                'pseudocode': query(db, 'SELECT entry,c_file,body FROM code_fts WHERE entry=? LIMIT 1', (key,))}
    if args.command == 'xrefs':
        return query(db, 'SELECT * FROM refs WHERE target=? LIMIT ?', (key, n))
    if args.command == 'callers':
        return query(db, 'SELECT c.*,f.name AS caller_name FROM calls c LEFT JOIN functions f ON f.entry=c.caller WHERE c.callee=? LIMIT ?', (key, n))
    if args.command == 'callees':
        return query(db, 'SELECT * FROM calls WHERE caller=? LIMIT ?', (key, n))
    if args.command in ('around', 'range'):
        lo = int(a, 16)-args.radius if args.command == 'around' else int(a, 16)
        hi = int(a, 16)+args.radius if args.command == 'around' else int(args.end, 16)
        # Address text must be converted numerically; not all records have equal widths.
        if lo<0 or hi<lo:raise ValueError('Invalid address range')
        rows=[]
        for width in range(len(format(lo,'x')),len(format(hi,'x'))+1):
            low=max(lo,0 if width==1 else 16**(width-1))
            high=min(hi,16**width-1)
            rows.extend(query(db, 'SELECT * FROM functions WHERE entry>=? AND entry<=? AND length(entry)=? ORDER BY entry LIMIT ?', (format(low,'x'),format(high,'x'),width,n)))
        return sorted(rows,key=lambda r:int(r['entry'],16))[:n]
    image = Image(args.database.parent)
    va = int(a, 16)
    if args.command == 'bytes':
        return {'address': hex(va), 'bytes': image.read(va, args.count).hex(' '), 'sha256': SHA}
    if args.command == 'vtable':
        # Candidate pointer slots only. This does not establish class identity or ABI.
        slots = []
        for i in range(args.count):
            target = struct.unpack('<Q', image.read(va+i*8, 8))[0]
            slots.append({'slot': i, 'address': hex(va+i*8), 'target': hex(target),
                          'executable_section': image.executable(target),
                          'function': query(db, 'SELECT name,status FROM functions WHERE entry=?', (format(target,'x'),))})
        return {'classification':'UNVERIFIED_POINTER_TABLE', 'slots':slots}
    if args.command == 'disasm':
        deps=args.database.parent.parent/'tools/python-deps'
        if deps.is_dir():sys.path.insert(0,str(deps))
        try:
            import capstone
        except ImportError as e:
            raise ValueError('Install optional capstone in a dedicated research environment; see README') from e
        if not hasattr(capstone,'Cs'):
            raise ValueError('Capstone package is incomplete or unreadable in this process; check permissions for tools/python-deps')
        decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
        return [{'address':hex(i.address),'bytes':i.bytes.hex(' '),'mnemonic':i.mnemonic,'operands':i.op_str}
                for i in decoder.disasm(image.read(va,args.count),va)]
    raise ValueError('Unsupported command')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--database',type=Path,default=DEFAULT)
    parser.add_argument('--limit',type=int,default=20)
    parser.add_argument('--output',type=Path)
    subs=parser.add_subparsers(dest='command',required=True)
    subs.add_parser('schema')
    for name in ('text','strings','rtti','references','code','functions-containing','offset','name','function','xrefs','callers','callees','around','range','bytes','vtable','disasm'):
        sub=subs.add_parser(name);sub.add_argument('value')
        if name=='range':sub.add_argument('end')
        if name=='around':sub.add_argument('--radius',type=int,default=20,help='Byte radius, not instruction count')
        if name in ('bytes','vtable','disasm'):sub.add_argument('--count',type=int,default=16 if name=='vtable' else 128)
    args=parser.parse_args()
    if not 1<=args.limit<=10000 or getattr(args,'count',1)<1 or getattr(args,'count',1)>65536 or getattr(args,'radius',0)<0:
        parser.error('Invalid limit/count/radius')
    db=sqlite3.connect(args.database.resolve().as_uri()+'?mode=ro',uri=True)
    db.row_factory=sqlite3.Row
    db.execute('PRAGMA query_only=ON')
    try:
        result=run(args,db)
    finally:
        db.close()
    rendered=json.dumps(result,ensure_ascii=False,indent=2)
    if args.output:args.output.write_text(rendered+'\n',encoding='utf-8')
    else:print(rendered)

if __name__=='__main__':
    try:main()
    except (ValueError,sqlite3.Error,OSError) as error:
        raise SystemExit(f'Query failed: {error}')

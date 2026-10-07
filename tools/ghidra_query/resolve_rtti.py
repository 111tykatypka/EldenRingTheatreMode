"""Read-only MSVC x64 RTTI/COL/vtable discovery from the exact analysis image.
This verifies binary layout, not constructor ABI or native object ownership.
"""
import argparse
import json
import struct
from research_query import Image, DEFAULT

def occurrences(data, needle):
    at = 0
    while True:
        at = data.find(needle, at)
        if at < 0:
            return
        yield at
        at += 1

def resolve(image, name):
    needle = name.encode('ascii') + b'\0'
    sections = image.meta['sections']
    results = []
    for section in sections:
        chunk = image.data[section['file_offset']:section['file_offset']+section['raw_size']]
        for offset in occurrences(chunk, needle):
            descriptor_rva = section['rva'] + offset - 16
            if offset < 16:
                continue
            locators = []
            for scan in sections:
                if scan['name'] != '.rdata':
                    continue
                data = image.data[scan['file_offset']:scan['file_offset']+scan['raw_size']]
                for hit in occurrences(data, struct.pack('<I', descriptor_rva)):
                    start = hit - 12
                    if start < 0 or start + 24 > len(data):
                        continue
                    signature, displacement, construction, _, hierarchy, self_rva = struct.unpack_from('<6I', data, start)
                    if signature != 1 or self_rva != scan['rva'] + start:
                        continue
                    col_va = image.base + self_rva
                    tables = []
                    for ref in occurrences(data, struct.pack('<Q', col_va)):
                        vt = image.base + scan['rva'] + ref + 8
                        slots = []
                        for n in range(16):
                            if ref + 8 + (n+1)*8 > len(data):
                                break
                            target = struct.unpack_from('<Q', data, ref+8+n*8)[0]
                            if not image.executable(target):
                                break
                            slots.append(hex(target))
                        if slots:
                            tables.append({'vtable': hex(vt), 'first_executable_slots': slots})
                    locators.append({'col': hex(col_va), 'subobject_offset': displacement,
                                     'construction_displacement': construction,
                                     'hierarchy_rva': hex(hierarchy), 'tables': tables})
            results.append({'type_descriptor': hex(image.base+descriptor_rva),
                            'name': name, 'locators': locators,
                            'confidence': 'BINARY_LAYOUT_VERIFIED; ABI_AND_LIFECYCLE_UNKNOWN'})
    return results

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('names', nargs='+', help='Exact MSVC decorated RTTI names')
    parser.add_argument('--output')
    args = parser.parse_args()
    image = Image(DEFAULT.parent)
    result = {name: resolve(image, name) for name in args.names}
    text = json.dumps(result, indent=2) + '\n'
    if args.output:
        from pathlib import Path
        Path(args.output).write_text(text, encoding='utf-8')
    else:
        print(text)

if __name__ == '__main__':
    main()

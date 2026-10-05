# Targeted Ghidra queries

Python 3.10+. Standard library for SQLite/PE/RTTI; optional **Capstone 5.0.6** for
`disasm`. Never loads or executes the game. SQLite is opened `mode=ro` plus
`query_only=ON`. Binary commands verify the analysis copy's exact SHA-256 before
reading section bytes. No original game access is needed.

Default database: `../../../research/ghidra-eldenring/export/research.sqlite`
relative to this tool. `--database`, `--limit`, `--output` precede the command.

```powershell
python tools/ghidra_query/research_query.py text WorldChrMan
python tools/ghidra_query/research_query.py rtti ReplayRecorder
python tools/ghidra_query/research_query.py references PlayerIns
python tools/ghidra_query/research_query.py callers 0x1404e4f70
python tools/ghidra_query/research_query.py callees 0x1404e5310
python tools/ghidra_query/research_query.py function 0x14050f9e0
python tools/ghidra_query/research_query.py around 0x1404e4f86 --radius 80
python tools/ghidra_query/research_query.py range 0x1404e4f00 0x1404e5400
python tools/ghidra_query/research_query.py offset 0x1f1d8
python tools/ghidra_query/research_query.py functions-containing root_motion
python tools/ghidra_query/research_query.py vtable 0x142a4aa40 --count 4
python tools/ghidra_query/research_query.py disasm 0x1404e4f70 --count 180
python tools/ghidra_query/research_query.py schema
python tools/ghidra_query/correlate_sdk.py --output ../research/ghidra-eldenring/targeted/sdk_anchors.json
```

`text` searches literal FTS phrases in strings and C, plus symbol substrings.
`name` searches function-name substrings. `rtti` uses a bounded substring search
in decorated class-name strings because FTS does not split `AVReplayRecorder`.
All addresses are **preferred image VAs**, hexadecimal, optional `0x`; RVAs must
be added to image base 0x140000000. They are NOT live ASLR addresses.
`around` uses a byte radius, not an instruction count; it lists function entries,
not containing function bodies. `offset` searches textual hex constants only:
results can be sizes, absolute addresses, unrelated structures or false positives.
`functions-containing` is an alias for `code`. Matches are candidates.

## Actual schema

`schema` prints sqlite_master definitions, including FTS internal tables.

| Table | Columns |
|---|---|
| functions | entry (PK), rva, name, signature, body_bytes, status, c_file |
| symbols | address, name, type, source, external |
| calls | caller, callee, callee_name |
| refs | source, target, type, source_type, operand, primary_ref |
| string_refs | string_address, source, function, type |
| strings_fts | address, rva, file_offset, encoding, value (FTS) |
| code_fts | entry, c_file, body (FTS) |

Indexes exist on functions.entry, call endpoints, ref endpoints, symbols.address
and string_refs.string_address. Numeric range queries split address-width bands
and use the function PK. FTS is used for C searches; per-entry C retrieval and
decorated RTTI substring queries scan their smaller tables. Indirect calls and
references absent from partial Ghidra analysis will NOT be magically recovered.

## Disassembly and structural RTTI verification

Optional installation (separate research directory, never global Python):

```powershell
python -m pip install --target ../research/ghidra-eldenring/tools/python-deps capstone==5.0.6
```

The tool discovers that directory alongside the database. If a sandbox cannot
read packages installed under another identity, execute the query in the same
authorized environment or install in a readable dedicated environment.
`disasm` decodes a caller-specified byte range, not a proven CFG. Bytes after a
RET can be padding, embedded data, another function or obfuscation; do not treat
all decoded instructions as belonging to the requested function.

`vtable` only decodes pointer slots, labels executable-section targets and checks
indexed function entries. It does NOT prove a vtable or function ABI.
`correlate_sdk.py` separately validates x64 MSVC COL signature, self RVA, descriptor
reference, hierarchy bytes and pointers back to COL followed by executable slots.
It records candidates, exact bytes and xrefs. RTTI identity/layout/live lifetime
must still be checked. No output is automatically converted into an engine call.

Research exports remain outside the repository; do not commit or distribute
game-derived pseudocode or original binaries. Tool source and evidence notes are
independently implemented. No SDK or Ghidra source is copied into these tools.

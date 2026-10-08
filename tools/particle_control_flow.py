"""Read-only bounded symbolic trace of VFX debug creation trampolines.

Constant/stack transfers only; unknown branches/calls are recorded, never guessed.
This is a discovery tool, not an ABI or lifetime verifier.
"""
import argparse
import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent / 'ghidra_query'))
import research_query as query
sys.path.insert(0, str(query.DEFAULT.parent.parent / 'tools/python-deps'))
import capstone
from capstone.x86 import X86_OP_REG, X86_OP_IMM, X86_OP_MEM

def trace(start, limit):
    image = query.Image(query.DEFAULT.parent)
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    decoder.detail = True
    todo = [(start, {'rsp': 0x100000}, {})]
    visited = set()
    instructions = []
    calls = []
    unresolved = []
    def name(i, r):
        n = i.reg_name(r)
        return {'eax':'rax','ebx':'rbx','ecx':'rcx','edx':'rdx','esp':'rsp','ebp':'rbp','esi':'rsi','edi':'rdi',
                'r8d':'r8','r9d':'r9','r10d':'r10','r11d':'r11'}.get(n,n)
    while todo and len(visited) < limit:
        pc, regs, stack = todo.pop()
        while pc not in visited and len(visited) < limit:
            visited.add(pc)
            try:
                i = next(decoder.disasm(image.read(pc,15),pc,count=1))
            except (ValueError, StopIteration):
                unresolved.append({'address':hex(pc),'reason':'unreadable instruction'});break
            instructions.append({'address':hex(pc),'bytes':i.bytes.hex(' '),'op':i.mnemonic,'args':i.op_str})
            after = pc + i.size
            def addr(op):
                if op.type != X86_OP_MEM: return None
                m = op.mem
                if m.index: return None
                base = after if i.reg_name(m.base) == 'rip' else regs.get(name(i,m.base),0 if not m.base else None)
                return None if base is None else base + m.disp
            def get(op):
                if op.type == X86_OP_IMM: return op.imm
                if op.type == X86_OP_REG: return regs.get(name(i,op.reg))
                a = addr(op)
                if a is None:return None
                if a in stack:return stack[a]
                try:return int.from_bytes(image.read(a,op.size),'little')
                except ValueError:return None
            def put(op,v):
                if op.type == X86_OP_REG:regs[name(i,op.reg)] = v
                elif op.type == X86_OP_MEM:
                    a=addr(op)
                    # Simulate only our abstract stack; never write input image.
                    if a is not None and 0xf0000 <= a <= 0x101000:stack[a]=v
            ops=i.operands
            if i.mnemonic in ('mov','movabs') and len(ops)==2:put(ops[0],get(ops[1]))
            elif i.mnemonic=='lea' and len(ops)==2:put(ops[0],addr(ops[1]))
            elif i.mnemonic=='xchg' and len(ops)==2:
                a,b=get(ops[0]),get(ops[1]);put(ops[0],b);put(ops[1],a)
            elif i.mnemonic=='push':
                v=get(ops[0]);s=regs.get('rsp');regs['rsp']=None if s is None else s-8
                if s is not None:stack[s-8]=v
            elif i.mnemonic=='pop':
                s=regs.get('rsp');v=stack.get(s);regs['rsp']=None if s is None else s+8;put(ops[0],v)
            elif i.mnemonic in ('add','sub') and len(ops)==2:
                a,b=get(ops[0]),get(ops[1]);put(ops[0],None if a is None or b is None else (a+b if i.mnemonic=='add' else a-b))
            elif i.mnemonic.startswith('cmov'):
                other=regs.copy();a=get(ops[1]);put(ops[0],a)
                todo.append((after,other,stack.copy()))
            elif i.mnemonic=='call':
                target=get(ops[0]);calls.append({'at':hex(pc),'target':hex(target) if target is not None else None,'operand':i.op_str})
                for r in ('rax','rcx','rdx','r8','r9','r10','r11'):regs[r]=None
            elif i.mnemonic=='jmp':
                target=get(ops[0])
                if target is None or not image.executable(target):
                    unresolved.append({'address':hex(pc),'reason':'unresolved jump','operand':i.op_str,'target':hex(target) if target is not None else None});break
                pc=target;continue
            elif i.mnemonic.startswith('j') and ops:
                target=get(ops[0])
                if target is not None and image.executable(target):todo.append((target,regs.copy(),stack.copy()))
            elif i.mnemonic in ('ret','retf','int3'):break
            else:
                # Conservatively invalidate explicit register destinations for arithmetic.
                if ops and ops[0].type==X86_OP_REG and i.mnemonic not in ('cmp','test') and not i.mnemonic.startswith('mov'):
                    put(ops[0],None)
            pc=after
    return {'classification':'STATIC_CANDIDATE_TRACE_NOT_LIFETIME_PROOF','sha256':query.SHA,
            'start':hex(start),'instruction_count':len(visited),'limit_reached':bool(todo),
            'calls':calls,'unresolved':unresolved,'instructions':instructions}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--start',default='1454ea58e');p.add_argument('--limit',type=int,default=10000);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    if not 1<=a.limit<=100000:p.error('limit out of range')
    result=trace(int(a.start,16),a.limit)
    a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:result[k] for k in ('instruction_count','limit_reached','calls','unresolved')},indent=2))

import struct,sys,re
from capstone import *
d=open(sys.argv[1]+'/jp/ov77.bin','rb').read(); base=0x21e4e40
md=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
st=set(o for o in range(0,len(d)-1,2) if (struct.unpack_from('<H',d,o)[0]&0xFE00)==0xB400 and struct.unpack_from('<H',d,o)[0]&0x100)
out=[];o=0
while o<len(d)-1:
    if o in st: out.append('\n; ---- func %08x'%(base+o))
    ins=list(md.disasm(d[o:o+4],base+o,1))
    if not ins:
        out.append(f"{base+o:08x}: .hword 0x{struct.unpack_from('<H',d,o)[0]:04x}"); o+=2; continue
    i=ins[0]; line=f"{i.address:08x}: {i.mnemonic:8s} {i.op_str}"
    if i.mnemonic=='ldr' and 'pc' in i.op_str:
        m=re.search(r'#(0x[0-9a-f]+|\d+)\]',i.op_str)
        if m:
            a=((i.address+4)&~3)+int(m.group(1),0)
            if 0<=a-base<=len(d)-4: line+=f"   ; =0x{struct.unpack_from('<I',d,a-base)[0]:08x}"
    out.append(line); o+=i.size
open(sys.argv[1]+'/jp/ov77.asm','w').write('\n'.join(out))
print(len(out))

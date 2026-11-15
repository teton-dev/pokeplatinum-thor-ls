import ndspy.rom, struct
from capstone import *
jp=ndspy.rom.NintendoDSRom.fromFile('/Users/trysten/thor-dev/HEARTGOLD_JP.nds')
o=jp.loadArm9Overlays()[77]
d=bytes(o.data); base=o.ramAddress
print(hex(base),len(d),'bss',o.bssSize if hasattr(o,'bssSize') else '')
open('/private/tmp/claude-502/-Users-trysten-pokeplatinum-thor-ls/3a2b8559-f9f5-4b67-8173-42c266aadeda/scratchpad/jp/ov77.bin','wb').write(d)
open('/private/tmp/claude-502/-Users-trysten-pokeplatinum-thor-ls/3a2b8559-f9f5-4b67-8173-42c266aadeda/scratchpad/jp/arm9.bin','wb').write(bytes(jp.arm9))
print(hex(jp.arm9RamAddress))
# find function prologues (thumb push with lr)
import re
funcs=[]
for off in range(0,len(d)-1,2):
    h=struct.unpack_from('<H',d,off)[0]
    if (h&0xFE00)==0xB400 and (h&0x100):
        funcs.append(off)
print(len(funcs),'push-lr sites')
print([hex(base+f) for f in funcs[:20]])
# strings
for m in re.finditer(rb'[\x20-\x7e]{5,}',d): print(hex(base+m.start()),m.group())

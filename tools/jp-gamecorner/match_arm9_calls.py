import ndspy.rom,re,collections
jp=ndspy.rom.NintendoDSRom.fromFile('/Users/trysten/thor-dev/HEARTGOLD_JP.nds')
us=ndspy.rom.NintendoDSRom.fromFile('/Users/trysten/thor-dev/Pokemon - HeartGold Version (USA).nds')
import ndspy.codeCompression as cc
ja=bytes(cc.decompress(jp.arm9));ua=bytes(cc.decompress(us.arm9))
print(len(ja),len(ua),hex(jp.arm9RamAddress),hex(us.arm9RamAddress))
asm=open('jp/ov77.asm').read()
cnt=collections.Counter(int(m,16) for m in re.findall(r'\bblx?\s+#0x([0-9a-f]+)',asm))
res={}
for t,n in cnt.most_common():
    if not (0x2000000<=t<0x2100000): continue
    o=t-0x2000000
    for L in (24,16,12,8):
        pat=ja[o:o+L]
        hits=[m.start() for m in re.finditer(b'(?=' + re.escape(pat)+b')',ua,re.S) if m.start()%2==0]
        if len(hits)==1: res[t]=(0x2000000+hits[0],n,L);break
        if len(hits)>1 and L==8: res[t]=(None,n,len(hits))
for t,(u,n,l) in sorted(res.items(),key=lambda x:-x[1][1]):
    print(hex(t),n,hex(u) if u else 'ambig',l)
print(len([1 for v in res.values() if v[0]]),'/',len([t for t in cnt if 0x2000000<=t<0x2100000]))

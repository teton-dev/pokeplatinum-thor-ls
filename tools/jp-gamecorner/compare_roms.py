import ndspy.rom, sys
jp=ndspy.rom.NintendoDSRom.fromFile('/Users/trysten/thor-dev/HEARTGOLD_JP.nds')
us=ndspy.rom.NintendoDSRom.fromFile('/Users/trysten/thor-dev/Pokemon - HeartGold Version (USA).nds')
def files(r):
    d={}
    def walk(f,p):
        for n in f.files: d[p+n]=r.files[f.idOf(n)]
        for n,s in f.folders: walk(s,p+n+'/')
    walk(r.filenames,'')
    return d
fj,fu=files(jp),files(us)
print('files jp',len(fj),'us',len(fu))
print('only jp',[k for k in fj if k not in fu][:50])
print('only us',[k for k in fu if k not in fj][:50])
print('arm9 jp',len(jp.arm9),'us',len(us.arm9))
oj,ou=jp.loadArm9Overlays(),us.loadArm9Overlays()
print('overlays jp',len(oj),'us',len(ou))
for i in sorted(set(oj)|set(ou)):
    a=oj[i].data if i in oj else None; b=ou[i].data if i in ou else None
    print(i, len(a) if a else None, len(b) if b else None)
print('=== file size diffs (narc only, big)')
for k in sorted(fj):
    if k in fu and fj[k]!=fu[k]:
        print(k,len(fj[k]),len(fu[k]))

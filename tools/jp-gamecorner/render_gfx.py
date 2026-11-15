import ndspy.narc,struct,zlib,sys
S='./'  # run from a scratch folder holding t/out (the importer's output)
base=S+'t/out/a/1/'
def png(path,w,h,rows):
    raw=b''.join(b'\0'+bytes(r) for r in rows)
    def ch(t,d):
        c=struct.pack('>I',len(d))+t+d; return c+struct.pack('>I',zlib.crc32(t+d))
    open(path,'wb').write(b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+ch(b'IDAT',zlib.compress(raw))+ch(b'IEND',b''))
def sec(d,tag):
    o=0x10
    while o<len(d):
        t=d[o:o+4]; sz=struct.unpack_from('<I',d,o+4)[0]
        if t==tag: return d[o+8:o+sz]
        o+=sz
def pal(d):
    p=sec(d,b'TTLP'); size=struct.unpack_from('<I',p,8)[0]; data=p[16:16+size]
    return [(((v&31)*255)//31,(((v>>5)&31)*255)//31,(((v>>10)&31)*255)//31) for v in struct.unpack('<%dH'%(len(data)//2),data)]
def chr_(d):
    p=sec(d,b'RAHC'); depth=struct.unpack_from('<I',p,4)[0]; dsize=struct.unpack_from('<I',p,16)[0]
    return depth,p[24:24+dsize]
def scr(d):
    p=sec(d,b'NRCS'); w,h=struct.unpack_from('<HH',p,0); dsize=struct.unpack_from('<I',p,8)[0]
    return w,h,list(struct.unpack('<%dH'%(dsize//2),p[12:12+dsize]))
def render(pd,cd,sd,name):
    cols=pal(pd); depth,data=chr_(cd); sw,sh,ents=scr(sd)
    b8=depth==4; tsz=64 if b8 else 32; nt=len(data)//tsz
    rows=[bytearray(sw*3) for _ in range(sh)]
    for ty in range(sh//8):
        for tx in range(sw//8):
            e=ents[ty*(sw//8)+tx]; t=e&0x3ff; hf=e>>10&1; vf=e>>11&1; pb=e>>12
            if t>=nt: continue
            for y in range(8):
                for x in range(8):
                    if b8: c=data[t*64+y*8+x]
                    else:
                        v=data[t*32+y*4+x//2]; c=((v>>4) if x&1 else v&15)+pb*16
                    xx=7-x if hf else x; yy=7-y if vf else y
                    rgb=cols[c] if c<len(cols) else (255,0,255)
                    rows[ty*8+yy][(tx*8+xx)*3:(tx*8+xx)*3+3]=bytes(rgb)
    png(S+'img/'+name+'.png',sw,sh,rows); print(name,sw,sh,'8bpp' if b8 else '4bpp',len(data)//tsz,'tiles',len(cols),'colors')
a=[bytes(f) for f in ndspy.narc.NARC(open(base+'2/7','rb').read()).files]
b=[bytes(f) for f in ndspy.narc.NARC(open(base+'5/5','rb').read()).files]
for si in (10,11,12): render(a[8],a[9],a[si],f'a27_s{si}')
for si in (15,16,17,18,19): render(a[13],a[14],a[si],f'a27_s{si}')
for si in (22,23,24): render(a[20],a[21],a[si],f'a27_s{si}')
render(b[1],b[0],b[2],'a55_s2')

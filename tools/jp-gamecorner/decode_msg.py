import ndspy.rom,ndspy.narc,struct,sys
def decode(b):
    n,seed=struct.unpack_from('<HH',b,0)
    key1=(seed*0x2FD)&0xFFFF
    out=[]
    for i in range(n):
        k=(key1*(i+1))&0xFFFF; k|=k<<16
        off,ln=struct.unpack_from('<II',b,4+8*i); off^=k; ln^=k
        k2=(0x91BD3*(i+1))&0xFFFF
        codes=[]
        for j in range(ln):
            c=struct.unpack_from('<H',b,off+2*j)[0]^k2
            k2=(k2+0x493D)&0xFFFF
            codes.append(c)
        # decompress if starts with 0xF100
        if codes and codes[0]==0xF100:
            bits=[];cc=codes[1:]
            res=[];acc=0;nb=0
            for c in cc:
                acc|=c<<nb; nb+=16
                while nb>=9:
                    v=acc&0x1FF; acc>>=9; nb-=9
                    if v==0x1FF: break
                    res.append(v)
                else: continue
                break
            codes=res
        out.append(codes)
    return out
if __name__=='__main__':
    rom=ndspy.rom.NintendoDSRom.fromFile(sys.argv[1])
    narc=ndspy.narc.NARC(rom.getFileByName('a/0/2/7'))
    b=narc.files[int(sys.argv[2])]
    for i,c in enumerate(decode(bytes(b))): print(i,len(c),' '.join('%04x'%x for x in c[:40]))

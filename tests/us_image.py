"""Loads the US v2.00 executable and BIN overlays (MWo3 images, 16-bit LZ) from an extracted disc folder.
REO_GAME_DIR names the folder and must be set."""
import struct, os, sys
GAME=os.environ.get("REO_GAME_DIR")
if not GAME: sys.exit("Set REO_GAME_DIR to the folder with the files extracted from your own US v2.00 disc.")
def decompress(d):
    out=bytearray(); pos=0; flags=0; mask=0
    def r16():
        nonlocal pos
        v=d[pos]|(d[pos+1]<<8); pos+=2; return v
    while True:
        if mask==0: flags=r16(); mask=0x8000
        ref=flags&mask; mask>>=1
        if not ref:
            out+=d[pos:pos+2]; pos+=2
        else:
            v=r16(); length=v>>11; off=v&0x7ff
            if length==0: length=r16()
            if off==0 and length==0: break
            if off==0: out+=bytes(length*2); continue
            for i in range(length):
                f=len(out)-off*2; out+=out[f:f+2]
    return bytes(out)
def overlay(n):
    d=open(os.path.join(GAME,'BIN',f'{n}.dat'),'rb').read()
    if d[:4]!=b'MWo3': d=decompress(d)
    assert d[:4]==b'MWo3'
    load,text,data,bss,end=struct.unpack('<IIIII',d[8:28])
    name=d[0x20:0x40].split(b'\0')[0].decode()
    return dict(n=n,name=name,load=load,text=text,data=data,bss=bss,end=end,image=d)
def elf(path):
    d=open(path,'rb').read()
    e_phoff,=struct.unpack('<I',d[0x1c:0x20]); phnum,=struct.unpack('<H',d[0x2c:0x2e]); phes,=struct.unpack('<H',d[0x2a:0x2c])
    segs=[]
    for i in range(phnum):
        t,off,va,pa,fs,ms,fl,al=struct.unpack('<8I',d[e_phoff+i*phes:e_phoff+i*phes+32])
        if t==1: segs.append((va,d[off:off+fs],ms))
    return segs
if __name__=='__main__':
    for va,b,ms in elf(os.path.join(GAME,'SLUS_207.65')): print('ELF seg %08x filesz %x memsz %x end %08x'%(va,len(b),ms,va+ms))
    for n in (0,1,2,3,9):
        o=overlay(n); print(n,o['name'],'%08x text %x data %x bss %x end %08x'%(o['load'],o['text'],o['data'],o['bss'],o['end']))

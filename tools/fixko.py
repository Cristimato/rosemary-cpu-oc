#!/usr/bin/env python3
# fixko.py — corrige a relocação de .exit em __this_module.
# O struct module do kernel do aparelho tem `exit` em 0x370, mas a árvore de
# fonte gera 0x378. Sem isto o módulo fica [permanent] (não descarrega).
import struct,sys,subprocess
p=sys.argv[1]; d=bytearray(open(p,'rb').read())
# ELF64 section headers
shoff=struct.unpack_from('<Q',d,0x28)[0]; shentsize,shnum,shstrndx=struct.unpack_from('<HHH',d,0x3a)
def sh(i): return struct.unpack_from('<IIQQQQIIQQ',d,shoff+i*shentsize)
stro=sh(shstrndx)[4]
def nm(o): return d[stro+o:d.index(b'\0',stro+o)].decode()
n=0
for i in range(shnum):
    s=sh(i)
    if nm(s[0])=='.rela.gnu.linkonce.this_module':
        for e in range(s[5]//24):
            off=s[4]+e*24; r_off=struct.unpack_from('<Q',d,off)[0]
            if r_off==0x378: struct.pack_into('<Q',d,off,0x370); n+=1
open(p,'wb').write(d); print('patched',n)

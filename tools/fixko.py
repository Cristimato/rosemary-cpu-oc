#!/usr/bin/env python3
"""fixko.py — move a relocação de `exit` (cleanup_module) em __this_module.

O struct module do kernel validado (YuuriLegacy:3) tem o campo `exit` em
0x370, mas a árvore de fonte pública gera 0x378. Sem a correção o módulo
carrega, mas fica [permanent] (não descarrega).

NÃO aplique às cegas em outro kernel: num kernel em que 0x378 já é o certo,
isto aponta `exit` para o campo errado. Para descobrir se precisa: compile
sem a correção, carregue; se /proc/modules mostrar o módulo como
[permanent], o offset do seu kernel difere.

Uso: fixko.py <arquivo.ko> <offset_origem> <offset_destino>
     ex.: fixko.py rmoc.ko 0x378 0x370
"""
import struct
import sys


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    path, src, dst = sys.argv[1], int(sys.argv[2], 0), int(sys.argv[3], 0)
    d = bytearray(open(path, 'rb').read())
    if d[:4] != b'\x7fELF' or d[4] != 2:
        sys.exit('ERRO: não é ELF64')

    shoff = struct.unpack_from('<Q', d, 0x28)[0]
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x3a)

    def sh(i):
        return struct.unpack_from('<IIQQQQIIQQ', d, shoff + i * shentsize)

    def cstr(base, off):
        end = d.index(b'\0', base + off)
        return d[base + off:end].decode()

    shstr = sh(shstrndx)[4]
    patched = 0
    for i in range(shnum):
        s = sh(i)
        if cstr(shstr, s[0]) != '.rela.gnu.linkonce.this_module':
            continue
        symtab = sh(s[6])            # sh_link -> .symtab
        strtab = sh(symtab[6])[4]    # sh_link -> .strtab
        for e in range(s[5] // 24):
            off = s[4] + e * 24
            r_off, r_info = struct.unpack_from('<QQ', d, off)
            sym = r_info >> 32
            name = cstr(strtab, struct.unpack_from('<I', d, symtab[4] + sym * 24)[0])
            if name != 'cleanup_module':
                continue
            if r_off == dst:
                print(f'já corrigido ({name} @ {dst:#x})')
                return
            if r_off != src:
                sys.exit(f'ERRO: {name} está em {r_off:#x}, esperado {src:#x}; nada alterado')
            struct.pack_into('<Q', d, off, dst)
            patched += 1
    if patched != 1:
        sys.exit(f'ERRO: relocação de cleanup_module não encontrada (achadas {patched})')
    open(path, 'wb').write(d)
    print(f'cleanup_module: {src:#x} -> {dst:#x}')


if __name__ == '__main__':
    main()

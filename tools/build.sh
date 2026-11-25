#!/usr/bin/env bash
# Compila src/rmoc.c contra uma árvore de kernel já configurada e preparada.
#
# Uso: KDIR=/caminho/do/kernel [EXIT_FIX=0x378:0x370] tools/build.sh
#
#   KDIR      árvore do kernel; o .config tem de ser IGUAL ao do aparelho
#             (copie /proc/config.gz). Um .config diferente (ex.: SHADOW_CALL_STACK=y)
#             gera um módulo que derruba o kernel no insmod.
#   EXIT_FIX  opcional, "origem:destino" para tools/fixko.py. Para o kernel validado
#             (YuuriLegacy:3) use 0x378:0x370. Em outro kernel, só use se o módulo
#             compilado sem isso aparecer como [permanent] em /proc/modules.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
: "${KDIR:?defina KDIR com a árvore do kernel}"
export ARCH=arm64
make -C "$KDIR" M="$ROOT/src" LLVM=1 LLVM_IAS=1 CC=clang LD=ld.lld ${MAKE_EXTRA:-} modules
KO="$ROOT/src/rmoc.ko"

if [ -n "${EXIT_FIX:-}" ]; then
  python3 "$ROOT/tools/fixko.py" "$KO" "${EXIT_FIX%%:*}" "${EXIT_FIX##*:}"
fi

# checagens de sanidade
if llvm-objdump -d "$KO" | grep -q 'x18\]'; then
  echo "ERRO: módulo usa shadow call stack (x18) — .config não bate com o aparelho" >&2; exit 1
fi
llvm-readelf -r "$KO" | grep cleanup_module
llvm-readelf -p .modinfo "$KO" | grep -E 'vermagic|description'
cp "$KO" "$ROOT/module/rmoc.ko"
echo "OK: module/rmoc.ko atualizado"

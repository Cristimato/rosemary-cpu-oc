#!/usr/bin/env bash
# Compila src/rmoc.c contra uma árvore de kernel já configurada e preparada.
# Uso: KDIR=/caminho/do/kernel tools/build.sh
#
# IMPORTANTE: o .config de KDIR tem de ser IGUAL ao do kernel do aparelho
# (copie /proc/config.gz). Um .config diferente (ex.: SHADOW_CALL_STACK=y)
# gera um módulo que derruba o kernel no insmod.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
: "${KDIR:?defina KDIR com a árvore do kernel}"
export ARCH=arm64
make -C "$KDIR" M="$ROOT/src" LLVM=1 LLVM_IAS=1 modules
python3 "$ROOT/tools/fixko.py" "$ROOT/src/rmoc.ko"

# checagens de sanidade
if llvm-objdump -d "$ROOT/src/rmoc.ko" | grep -q 'x18\]'; then
  echo "ERRO: módulo usa shadow call stack (x18) — .config não bate com o aparelho" >&2; exit 1
fi
llvm-readelf -p .modinfo "$ROOT/src/rmoc.ko" | grep -E 'vermagic|description'
cp "$ROOT/src/rmoc.ko" "$ROOT/module/rmoc.ko"
echo "OK: module/rmoc.ko atualizado"

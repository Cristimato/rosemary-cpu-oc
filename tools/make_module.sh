#!/usr/bin/env bash
# Empacota module/ num zip instalável (KernelSU / Magisk / APatch).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
VER=$(sed -n 's/^version=//p' "$ROOT/module/module.prop")
OUT="$ROOT/out/rmoc_oc-$VER.zip"
mkdir -p "$ROOT/out"; rm -f "$OUT"
cd "$ROOT/module"
zip -q -r -X "$OUT" module.prop customize.sh service.sh uninstall.sh rmoc.ko META-INF
echo "$OUT"; sha256sum "$OUT"

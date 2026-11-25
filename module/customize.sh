#!/system/bin/sh
# Instalação do rmoc_oc (KernelSU / Magisk / APatch)
SKIPUNZIP=0

ui_print "- Rosemary CPU OC v5.1 (A76 2050 -> 2112 MHz)"

PLAT=$(getprop ro.board.platform)
DEV=$(getprop ro.product.device)
[ "$PLAT" = "mt6785" ] || abort "! Plataforma $PLAT não suportada (só MT6785 / Helio G95)"
[ "$DEV" = "rosemary" ] || ui_print "! Aviso: aparelho '$DEV' não testado (testado: rosemary)"
[ "$ARCH" = "arm64" ] || abort "! Arquitetura $ARCH não suportada"

KREL=$(uname -r)
ui_print "- Kernel: $KREL"
case "$KREL" in
  4.19.325-cip134-st18-YuuriLegacy:3) ui_print "- Kernel validado";;
  *) ui_print "! Kernel diferente do validado: o serviço NÃO vai carregar o rmoc.ko."
     ui_print "! Recompile src/rmoc.c contra o seu kernel e ajuste KERNEL_OK no service.sh.";;
esac

# reinstalação: zera o estado anti-bootloop, mantém o kill switch se existir
rm -f /data/adb/rmoc_state
[ -f /data/adb/rmoc_disable ] && ui_print "! /data/adb/rmoc_disable existe: OC continuará desligado até removê-lo"

set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/service.sh" 0 0 0755
set_perm "$MODPATH/uninstall.sh" 0 0 0755
ui_print "- OC aplica 90 s após o boot. Log: /data/adb/rmoc.log"
